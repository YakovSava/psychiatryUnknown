#include "psych.hpp"

#include "up_helpers.hpp"

#include <algorithm>
#include <cstring>
#include <random>
#include <string>

namespace psych {

static State g_state;
static std::mt19937 g_rng(0x9E3779B9u);

static void compulsion_start(uint64_t body, State& s);
static void compulsion_decay(uint64_t body, float dt, State& s);

static float rnd01()
{
    return std::uniform_real_distribution<float>(0.0f, 1.0f)(g_rng);
}

static float rnd_range(float low, float high)
{
    return low + (high - low) * rnd01();
}

static float clampf(float value, float low, float high)
{
    return value < low ? low : (value > high ? high : value);
}

static float move_to(float current, float target, float step)
{
    if (current < target)
    {
        return std::min(current + step, target);
    }
    return std::max(current - step, target);
}

void reset_state(int32_t inst)
{
    g_state = State();
    g_state.body_instance = inst;
}

static float read_f(uint64_t body, const char* name)
{
    float value = 0.0f;
    up::f32_field(body, name, &value);
    return value;
}

static void write_f(uint64_t body, const char* name, float value)
{
    up::set_f32(body, name, value);
}

static void add_f(uint64_t body, const char* name, float delta)
{
    up::add_f32(body, name, delta);
}

static bool read_i(uint64_t body, const char* name, int32_t* out)
{
    return up::i32_field(body, name, out);
}

static bool read_b(uint64_t body, const char* name, bool* out)
{
    return up::bool_field(body, name, out);
}

static void write_b(uint64_t body, const char* name, bool value)
{
    up::set_bool(body, name, value);
}

static bool position_of(uint64_t obj, float* x, float* y)
{
    uint64_t transform = up::obj_field(obj, "transform");
    if (transform == 0)
    {
        return false;
    }
    up_value value;
    std::memset(&value, 0, sizeof(value));
    bool ok = up::get_field(transform, "position", &value);
    up::release(transform);
    if (!ok || value.type != UP_VAL_VEC3)
    {
        return false;
    }
    *x = value.as.vec3.x;
    *y = value.as.vec3.y;
    return true;
}

static void play_sound(const char* clip, float x, float y, bool two_d)
{
    up_value args[9];
    args[0] = up::value_str(clip);
    args[1] = up::value_vec3(x, y, 0.0f);
    args[2] = up::value_bool(two_d);
    args[3] = up::value_bool(true);
    args[4] = up::value_nil();
    args[5] = up::value_f32(1.0f);
    args[6] = up::value_f32(1.0f);
    args[7] = up::value_bool(false);
    args[8] = up::value_bool(false);
    up::invoke_static("Sound", "Play", args, 9, nullptr);
}

static void play_2d(const char* clip)
{
    play_sound(clip, 0.0f, 0.0f, true);
}

static void play_at_body(uint64_t body, const char* clip)
{
    float x = 0.0f;
    float y = 0.0f;
    if (position_of(body, &x, &y))
    {
        play_sound(clip, x, y, false);
    }
    else
    {
        play_2d(clip);
    }
}

static uint64_t camera_main()
{
    return up::obj_static("PlayerCamera", "main");
}

static void flash_brain()
{
    uint64_t camera = camera_main();
    if (camera == 0)
    {
        return;
    }
    up_value arg = up::value_str("FlashBrain");
    up::invoke(camera, "StartCoroutine", &arg, 1, nullptr);
    up::release(camera);
}

static void camera_field(const char* name, float value)
{
    uint64_t camera = camera_main();
    if (camera == 0)
    {
        return;
    }
    up::set_f32(camera, name, value);
    up::release(camera);
}

static void vomit(uint64_t body)
{
    uint64_t vomiter = up::component(body, "Vomiter");
    if (vomiter == 0)
    {
        return;
    }
    up::invoke(vomiter, "Vomit", nullptr, 0, nullptr);
    up::release(vomiter);
}

static void talk(uint64_t body, const char* text)
{
    uint64_t talker = up::obj_field(body, "talker");
    if (talker == 0)
    {
        return;
    }
    up_value args[4];
    args[0] = up::value_str(text);
    args[1] = up::value_nil();
    args[2] = up::value_bool(false);
    args[3] = up::value_bool(false);
    up::invoke(talker, "Talk", args, 4, nullptr);
    up::release(talker);
}

static void harm_limb(uint64_t body, float muscle, float skin,
                      float bleed)
{
    uint64_t limbs = up::obj_field(body, "limbs");
    if (limbs == 0)
    {
        return;
    }
    int32_t count = up::array_length(limbs);
    if (count <= 1)
    {
        up::release(limbs);
        return;
    }
    int32_t index = 1 + static_cast<int32_t>(rnd01() * (count - 1));
    uint64_t limb = up::array_get(limbs, index);
    if (limb != 0)
    {
        add_f(limb, "muscleHealth", -muscle);
        add_f(limb, "skinHealth", -skin);
        add_f(limb, "bleedAmount", bleed);
        add_f(limb, "pain", muscle * 1.5f);
        up::release(limb);
    }
    up::release(limbs);
}

static void hit_blocks(uint64_t body, int32_t count, float damage)
{
    uint64_t world = up::obj_static("WorldGeneration", "world");
    float x = 0.0f;
    float y = 0.0f;
    if (world == 0 || !position_of(body, &x, &y))
    {
        up::release(world);
        return;
    }
    for (int32_t i = 0; i < count; i++)
    {
        up_value args[4];
        args[0] = up::value_vec3(x + rnd_range(-3.0f, 3.0f),
                                 y + rnd_range(-2.0f, 2.0f), 0.0f);
        args[1] = up::value_f32(damage);
        args[2] = up::value_bool(true);
        args[3] = up::value_bool(false);
        up::invoke(world, "DamageBlock", args, 4, nullptr);
    }
    up::release(world);
}

static void hallucinate(uint64_t body, int32_t count)
{
    static const char* clips[] = {
        "mine", "turretsee", "pistolshot", "rifleshot",
        "instantearthquake", "growl", "sonarcharge"
    };
    for (int32_t i = 0; i < count; i++)
    {
        int32_t pick = static_cast<int32_t>(rnd01() * 7.0f);
        play_2d(clips[pick]);
    }
    camera_field("threatMusicTime", 8.0f);
    write_f(body, "eyeScareTime", 5.0f);
    talk(body, "..!");
}

static int32_t count_near(uint64_t body, const char* type, float radius)
{
    uint64_t type_handle = up::cached_type(type);
    if (type_handle == 0)
    {
        return 0;
    }
    float cx = 0.0f;
    float cy = 0.0f;
    if (!position_of(body, &cx, &cy))
    {
        return 0;
    }
    up_value arg = up::value_obj(type_handle);
    up_value result;
    std::memset(&result, 0, sizeof(result));
    up::invoke_static("UnityEngine.Object", "FindObjectsOfType", &arg,
                      1, &result);
    uint64_t array = result.type == UP_VAL_OBJ ? result.as.obj : 0;
    if (array == 0)
    {
        return 0;
    }
    int32_t count = 0;
    int32_t length = up::array_length(array);
    float limit = radius * radius;
    for (int32_t i = 0; i < length; i++)
    {
        uint64_t obj = up::array_get(array, i);
        if (obj == 0)
        {
            continue;
        }
        float x = 0.0f;
        float y = 0.0f;
        if (position_of(obj, &x, &y))
        {
            float dx = x - cx;
            float dy = y - cy;
            if (dx * dx + dy * dy < limit)
            {
                count++;
            }
        }
        up::release(obj);
    }
    up::release(array);
    return count;
}

static void scare(uint64_t body, float adrenaline, float mood)
{
    add_f(body, "adrenaline", adrenaline);
    add_f(body, "curAdrenaline", adrenaline);
    add_f(body, "traumaAmount", 1.5f);
    add_f(body, "happiness", mood);
    write_f(body, "eyeScareTime", 3.0f);
    play_2d("harmSting");
}

uint64_t current_body()
{
    uint64_t camera = camera_main();
    if (camera == 0)
    {
        return 0;
    }
    uint64_t body = up::obj_field(camera, "body");
    up::release(camera);
    return body;
}

bool body_alive(uint64_t body)
{
    float brain = 0.0f;
    return up::f32_field(body, "brainHealth", &brain) && brain > 0.0f;
}

static bool world_busy()
{
    uint64_t world = up::obj_static("WorldGeneration", "world");
    if (world == 0)
    {
        return true;
    }
    bool generating = false;
    bool cutscene = false;
    up::bool_field(world, "generatingWorld", &generating);
    up::bool_field(world, "cutsceneActive", &cutscene);
    up::release(world);
    return generating || cutscene;
}

static void schedule_mood(float delta, float delay)
{
    g_state.mood_pending += delta;
    g_state.mood_timer = delay;
}

static void trigger_ssri_effect(uint64_t body)
{
    int32_t roll = static_cast<int32_t>(rnd01() * 9.0f);
    switch (roll)
    {
    case 0:
        schedule_mood(25.0f, 12.0f);
        add_f(body, "happiness", -25.0f);
        talk(body, "...");
        break;
    case 1:
        hit_blocks(body, 3, 25.0f);
        play_at_body(body, "scrapmetal");
        break;
    case 2:
        harm_limb(body, 15.0f, 30.0f, 8.0f);
        play_at_body(body, "selfharm");
        break;
    case 3:
        add_f(body, "brainHealth", -rnd_range(4.0f, 12.0f));
        play_2d("headhit");
        break;
    case 4:
        schedule_mood(18.0f, 6.0f);
        add_f(body, "happiness", -18.0f);
        break;
    case 5:
        add_f(body, "sicknessAmount", 18.0f);
        vomit(body);
        break;
    case 6:
        add_f(body, "temperature", rnd01() < 0.5f ? -1.2f : 1.2f);
        break;
    case 7:
        camera_field("bonusAbber", 2.5f);
        add_f(body, "happiness", -5.0f);
        flash_brain();
        break;
    default:
        hallucinate(body, 1);
        break;
    }
}

static void update_meds(uint64_t body, float dt, State& s)
{
    s.med_valproate = std::max(0.0f, s.med_valproate - dt);
    s.med_quetiapine = std::max(0.0f, s.med_quetiapine - dt);
    s.med_haloperidol = std::max(0.0f, s.med_haloperidol - dt);
    bool had_benzo = s.med_benzo > 0.0f;
    s.med_benzo = std::max(0.0f, s.med_benzo - dt);
    if (had_benzo && s.med_benzo <= 0.0f)
    {
        s.benzo_withdrawal += 6.0f;
    }
    s.benzo_withdrawal = std::max(0.0f, s.benzo_withdrawal - dt * 0.05f);

    if (s.med_valproate > 0.0f)
    {
        s.ssri_load = std::max(0.0f, s.ssri_load - dt * 1.4f);
        float mood = read_f(body, "happiness");
        write_f(body, "happiness", move_to(mood, 0.0f, dt * 0.8f));
        add_f(body, "traumaAmount", -dt * 0.08f);
    }
    if (s.med_quetiapine > 0.0f)
    {
        s.ssri_load = std::max(0.0f, s.ssri_load - dt * 0.9f);
        s.condition_schizo = std::max(0.0f,
                                      s.condition_schizo - dt * 4.0f);
        float adrenaline = read_f(body, "adrenaline");
        write_f(body, "adrenaline", move_to(adrenaline, 0.0f,
                                            dt * 4.0f));
        add_f(body, "energy", -dt * 0.1f);
        add_f(body, "bloodPressure", -dt * 0.15f);
    }
    if (s.med_haloperidol > 0.0f)
    {
        s.ssri_load = std::max(0.0f, s.ssri_load - dt * 0.6f);
        s.condition_schizo = std::max(0.0f,
                                      s.condition_schizo - dt * 8.0f);
        add_f(body, "happiness", -dt * 0.08f);
        float shake = read_f(body, "miscShakeIntensity");
        write_f(body, "miscShakeIntensity", std::max(shake, 0.15f));
        float slowdown = read_f(body, "temporarySlowdown");
        write_f(body, "temporarySlowdown", std::max(slowdown, 0.1f));
    }
    if (s.med_benzo > 0.0f)
    {
        add_f(body, "traumaAmount", -dt * 0.4f);
        write_f(body, "horrifiedLevel", 0.0f);
    }
    if (s.benzo_withdrawal > 10.0f)
    {
        float severity = s.benzo_withdrawal / 80.0f;
        add_f(body, "happiness", -dt * 0.02f * severity);
        float shake = read_f(body, "miscShakeIntensity");
        write_f(body, "miscShakeIntensity",
                std::max(shake, 0.2f * severity));
    }
}

static void update_ssri(uint64_t body, float dt, State& s)
{
    s.ssri_load = clampf(s.ssri_load, 0.0f, 100.0f);
    s.ssri_load = std::max(0.0f, s.ssri_load - dt * 0.12f);
    s.side_timer -= dt;
    if (s.side_timer > 0.0f)
    {
        return;
    }
    s.side_timer = rnd_range(7.0f, 16.0f);
    float rate = s.ssri_load;
    if (s.med_valproate > 0.0f)
    {
        rate *= 0.25f;
    }
    if (s.med_quetiapine > 0.0f)
    {
        rate *= 0.6f;
    }
    if (rate > 8.0f && rnd01() * 140.0f < rate)
    {
        trigger_ssri_effect(body);
    }
}

static void update_exposure(uint64_t body, float dt, State& s)
{
    int32_t corpses = 0;
    if (read_i(body, "corpsesSeen", &corpses))
    {
        if (corpses > s.last_corpses)
        {
            s.exposure_corpse += (corpses - s.last_corpses) * 20.0f;
            s.last_corpses = corpses;
        }
    }
    float bleed = read_f(body, "totalBleedSpeed");
    if (bleed > 0.05f)
    {
        s.exposure_blood += dt * 2.5f;
    }
    float brain = read_f(body, "brainHealth");
    if (brain < 40.0f)
    {
        s.low_brain_time += dt;
    }
    else
    {
        s.low_brain_time = std::max(0.0f, s.low_brain_time - dt);
    }
    s.scan_accum += dt;
    if (s.scan_accum < 2.0f)
    {
        return;
    }
    s.scan_accum = 0.0f;
    int32_t spiders = count_near(body, "SpiderHandler", 14.0f);
    spiders += count_near(body, "CaveTicks", 14.0f);
    if (spiders > 0)
    {
        s.exposure_spider += spiders * 2.2f;
        s.exposure_creature += spiders * 1.1f;
    }
}

static void develop(uint64_t body, State& s)
{
    float trauma = read_f(body, "traumaAmount");
    float brain = read_f(body, "brainHealth");
    if (s.phobia_arachno == 0.0f && s.exposure_spider > 90.0f &&
        rnd01() < 0.02f)
    {
        s.phobia_arachno = 50.0f;
    }
    if (s.condition_ptsd == 0.0f && s.exposure_corpse > 70.0f &&
        rnd01() < 0.015f)
    {
        s.condition_ptsd = 40.0f;
    }
    if (s.condition_cptsd == 0.0f && s.exposure_creature > 70.0f &&
        rnd01() < 0.015f)
    {
        s.condition_cptsd = 40.0f;
    }
    if (s.phobia_humano == 0.0f && s.exposure_guard > 70.0f &&
        rnd01() < 0.02f)
    {
        s.phobia_humano = 45.0f;
    }
    if (s.phobia_hemo == 0.0f && s.exposure_blood > 60.0f &&
        rnd01() < 0.02f)
    {
        s.phobia_hemo = 45.0f;
    }
    if (s.phobia_bush == 0.0f && s.exposure_bush > 50.0f &&
        rnd01() < 0.05f)
    {
        s.phobia_bush = 55.0f;
    }
    if (s.condition_organic == 0.0f && s.low_brain_time > 20.0f &&
        brain < 35.0f && rnd01() < 0.02f)
    {
        s.condition_organic = 45.0f;
    }
    if (s.condition_ocd == 0.0f &&
        (s.ssri_load > 35.0f || trauma > 55.0f) && rnd01() < 0.01f)
    {
        s.condition_ocd = 40.0f;
    }
    if (s.condition_schizo == 0.0f &&
        (s.ssri_load > 60.0f ||
         (brain < 45.0f &&
          (s.condition_ptsd > 0.0f || s.condition_cptsd > 0.0f))) &&
        rnd01() < 0.01f)
    {
        s.condition_schizo = 40.0f;
    }
    if (s.condition_insomnia == 0.0f &&
        (s.condition_ptsd > 0.0f || s.ssri_load > 40.0f ||
         trauma > 70.0f) && rnd01() < 0.008f)
    {
        s.condition_insomnia = 40.0f;
    }
}

static void phobia_frights(uint64_t body, float dt, State& s)
{
    s.fright_accum += dt;
    if (s.fright_accum < 1.0f)
    {
        return;
    }
    s.fright_accum = 0.0f;
    if (s.phobia_arachno > 0.0f)
    {
        if (count_near(body, "SpiderHandler", 12.0f) > 0 ||
            count_near(body, "CaveTicks", 12.0f) > 0)
        {
            scare(body, 25.0f, -2.0f);
        }
    }
    if (s.phobia_humano > 0.0f &&
        count_near(body, "GuardScript", 22.0f) > 0)
    {
        scare(body, 20.0f, -1.5f);
    }
    if (s.phobia_bush > 0.0f &&
        count_near(body, "BushCol", 9.0f) > 0)
    {
        scare(body, 15.0f, -1.0f);
    }
    if (s.phobia_hemo > 0.0f &&
        read_f(body, "totalBleedSpeed") > 0.02f)
    {
        scare(body, 10.0f, -1.5f);
    }
    if (s.phobia_insomno > 0.0f && read_f(body, "energy") < 40.0f)
    {
        scare(body, 12.0f, -1.0f);
    }
}

static void condition_effects(uint64_t body, float dt, State& s)
{
    s.mood_timer -= dt;
    if (s.mood_timer <= 0.0f && s.mood_pending != 0.0f)
    {
        add_f(body, "happiness", s.mood_pending);
        s.mood_pending = 0.0f;
    }
    if (s.freeze_timer > 0.0f)
    {
        s.freeze_timer -= dt;
        write_b(body, "forceNoMovement", true);
        if (s.freeze_timer <= 0.0f)
        {
            write_b(body, "forceNoMovement", false);
            talk(body, "...");
        }
    }
    s.needle_cooldown = std::max(0.0f, s.needle_cooldown - dt);
    s.nap_cooldown = std::max(0.0f, s.nap_cooldown - dt);
    compulsion_decay(body, dt, s);
    s.ocd_timer -= dt;
    s.dereal_timer -= dt;
    s.ptsd_timer -= dt;
    s.schizo_timer -= dt;
    s.panic_timer -= dt;
    s.organic_timer -= dt;

    if (s.condition_ocd > 0.0f && s.ocd_timer <= 0.0f)
    {
        s.ocd_timer = rnd_range(45.0f, 90.0f);
        compulsion_start(body, s);
    }
    if (s.dereal_timer <= 0.0f && s.freeze_timer <= 0.0f)
    {
        s.dereal_timer = rnd_range(100.0f, 220.0f);
        bool prone = read_f(body, "traumaAmount") > 60.0f ||
                     s.condition_ptsd > 0.0f ||
                     s.condition_cptsd > 0.0f;
        if (prone && s.med_quetiapine <= 0.0f)
        {
            s.freeze_timer = rnd_range(1.5f, 3.0f);
            flash_brain();
            talk(body, "This is not real.");
        }
    }
    if ((s.condition_ptsd > 0.0f || s.condition_cptsd > 0.0f) &&
        s.ptsd_timer <= 0.0f)
    {
        s.ptsd_timer = rnd_range(90.0f, 200.0f);
        if (s.med_valproate <= 0.0f || rnd01() < 0.3f)
        {
            hallucinate(body, 2);
            add_f(body, "happiness", -6.0f);
            add_f(body, "traumaAmount", 3.0f);
            flash_brain();
        }
    }
    if (s.condition_schizo > 0.0f && s.schizo_timer <= 0.0f)
    {
        s.schizo_timer = rnd_range(40.0f, 90.0f);
        hallucinate(body, 3);
        add_f(body, "happiness", -4.0f);
    }
    if (s.panic_timer <= 0.0f)
    {
        s.panic_timer = rnd_range(60.0f, 150.0f);
        bool panic = read_f(body, "traumaAmount") > 50.0f ||
                     s.condition_cptsd > 0.0f ||
                     s.benzo_withdrawal > 20.0f;
        if (panic && s.med_benzo <= 0.0f && s.med_quetiapine <= 0.0f)
        {
            add_f(body, "adrenaline", 60.0f);
            add_f(body, "curAdrenaline", 60.0f);
            up::invoke(body, "Jump", nullptr, 0, nullptr);
            float dash = rnd01() < 0.5f ? -9.0f : 9.0f;
            up_value dash_arg = up::value_vec3(dash, 4.0f, 0.0f);
            up::invoke(body, "SetVelocity", &dash_arg, 1, nullptr);
            flash_brain();
            talk(body, "..!!");
            play_2d("harmSting");
        }
    }
    if (s.condition_organic > 0.0f)
    {
        add_f(body, "happiness", -dt * 0.01f *
              (s.condition_organic / 50.0f));
    }
    if (s.condition_organic > 0.0f && s.organic_timer <= 0.0f)
    {
        s.organic_timer = rnd_range(60.0f, 120.0f);
        if (s.med_quetiapine <= 0.0f)
        {
            hit_blocks(body, 2, 20.0f);
            add_f(body, "adrenaline", 20.0f);
        }
    }
    if (s.condition_insomnia > 0.0f)
    {
        float bad = read_f(body, "badSleepAmount");
        write_f(body, "badSleepAmount", std::max(bad, 2.0f));
    }
}

void tick(float dt)
{
    uint64_t body = current_body();
    if (body == 0)
    {
        return;
    }
    if (!body_alive(body))
    {
        up::release(body);
        return;
    }
    int32_t body_id = up::instance_id(body);
    if (body_id != 0 && body_id != g_state.body_instance)
    {
        reset_state(body_id);
    }
    if (world_busy())
    {
        up::release(body);
        return;
    }
    State& s = g_state;
    update_meds(body, dt, s);
    update_ssri(body, dt, s);
    update_exposure(body, dt, s);
    s.second_accum += dt;
    if (s.second_accum >= 1.0f)
    {
        s.second_accum = 0.0f;
        develop(body, s);
        phobia_frights(body, 1.0f, s);
    }
    condition_effects(body, dt, s);
    up::release(body);
}

void on_take_dose(uint64_t antidepressants, float dose)
{
    uint64_t body = up::obj_field(antidepressants, "body");
    if (body != 0)
    {
        int32_t body_id = up::instance_id(body);
        if (g_state.body_instance == 0)
        {
            reset_state(body_id);
        }
        if (body_id == g_state.body_instance)
        {
            g_state.ssri_load = clampf(g_state.ssri_load + dose * 0.06f,
                                       0.0f, 100.0f);
        }
    }
    up::release(body);
}

static bool is_drug(const std::string& id)
{
    return id == "psy_valproate" || id == "psy_quetiapine" ||
           id == "psy_benzo" || id == "psy_haloperidol";
}

static void apply_drug(uint64_t body, const std::string& id, float ml)
{
    State& s = g_state;
    if (id == "psy_valproate")
    {
        s.med_valproate += ml * 6.0f;
        s.ssri_load = std::max(0.0f, s.ssri_load - ml * 0.5f);
    }
    else if (id == "psy_quetiapine")
    {
        s.med_quetiapine += ml * 9.0f;
        s.ssri_load = std::max(0.0f, s.ssri_load - ml * 0.4f);
        s.condition_schizo = std::max(0.0f,
                                      s.condition_schizo - ml * 0.5f);
    }
    else if (id == "psy_benzo")
    {
        s.med_benzo += ml * 9.0f;
        s.benzo_uses += 1.0f;
        s.benzo_withdrawal += ml * 0.6f;
        add_f(body, "traumaAmount", -ml * 0.8f);
        add_f(body, "happiness", ml * 0.5f);
        add_f(body, "adrenaline", -ml * 1.5f);
        write_f(body, "horrifiedLevel", 0.0f);
        uint64_t pills = up::component(body, "SleepingPills");
        if (pills != 0)
        {
            add_f(pills, "amount", ml * 10.0f);
            up::release(pills);
        }
    }
    else if (id == "psy_haloperidol")
    {
        s.med_haloperidol += ml * 15.0f;
        s.condition_schizo = std::max(0.0f,
                                      s.condition_schizo - ml);
        add_f(body, "happiness", -ml * 0.05f);
    }
    play_2d("pills");
}

static void clear_all_conditions()
{
    State& s = g_state;
    s.ssri_load = 0.0f;
    s.condition_ocd = 0.0f;
    s.condition_ptsd = 0.0f;
    s.condition_cptsd = 0.0f;
    s.condition_organic = 0.0f;
    s.condition_schizo = 0.0f;
    s.condition_insomnia = 0.0f;
    s.phobia_arachno = 0.0f;
    s.phobia_insomno = 0.0f;
    s.phobia_hemo = 0.0f;
    s.phobia_humano = 0.0f;
    s.phobia_bush = 0.0f;
    s.med_valproate = 0.0f;
    s.med_quetiapine = 0.0f;
    s.med_haloperidol = 0.0f;
    s.med_benzo = 0.0f;
    s.benzo_withdrawal = 0.0f;
    s.exposure_spider = 0.0f;
    s.exposure_corpse = 0.0f;
    s.exposure_creature = 0.0f;
    s.exposure_guard = 0.0f;
    s.exposure_blood = 0.0f;
    s.exposure_bush = 0.0f;
    s.insomnia_fails = 0;
    s.compulsion_kind = 0;
    s.compulsion_timer = 0.0f;
    s.freeze_timer = 0.0f;
    s.mood_pending = 0.0f;
}

static bool is_player(uint64_t body)
{
    if (body == 0)
    {
        return false;
    }
    int32_t body_id = up::instance_id(body);
    if (g_state.body_instance == 0)
    {
        reset_state(body_id);
    }
    return body_id == g_state.body_instance;
}

static void call_liquid_delegate(uint64_t type, const char* field,
                                 float ml, uint64_t argument)
{
    uint64_t callback = up::obj_field(type, field);
    if (callback == 0)
    {
        return;
    }
    up_value args[2];
    args[0] = up::value_f32(ml);
    args[1] = up::value_obj(argument);
    up::invoke(callback, "Invoke", args, 2, nullptr);
    up::release(callback);
}

bool on_drink(uint64_t container, uint64_t body, float amount,
              const char* sound)
{
    if (!is_player(body))
    {
        return false;
    }
    up_value amount_arg = up::value_f32(amount);
    up_value drain_result;
    std::memset(&drain_result, 0, sizeof(drain_result));
    if (!up::invoke(container, "CalculateDrain", &amount_arg, 1,
                    &drain_result))
    {
        return false;
    }
    uint64_t drain = drain_result.type == UP_VAL_OBJ
                     ? drain_result.as.obj : 0;
    if (drain == 0)
    {
        return false;
    }
    uint64_t stack = up::obj_field(container, "stack");
    uint64_t registry = up::obj_static("Liquids", "Registry");
    int32_t count = up::list_count(drain);
    for (int32_t i = 0; i < count; i++)
    {
        float ml = up::list_get_f32(drain, i);
        if (ml <= 0.0f)
        {
            continue;
        }
        uint64_t entry = up::list_get(stack, i);
        std::string liquid;
        if (entry != 0)
        {
            up::str_field(entry, "liquidId", &liquid);
        }
        if (is_drug(liquid))
        {
            apply_drug(body, liquid, ml);
        }
        else
        {
            if (liquid == "braingrow" || liquid == "mindwipe")
            {
                clear_all_conditions();
            }
            uint64_t type = registry != 0
                            ? up::dict_get(registry, liquid.c_str())
                            : 0;
            if (type != 0)
            {
                call_liquid_delegate(type, "onDrink", ml, body);
                up::release(type);
            }
        }
        up::release(entry);
    }
    up_value drain_arg = up::value_obj(drain);
    up::invoke(container, "Drain", &drain_arg, 1, nullptr);
    if (sound != nullptr)
    {
        play_at_body(body, sound);
    }
    up::release(drain);
    up::release(stack);
    up::release(registry);
    return true;
}

bool on_inject(uint64_t container, uint64_t limb, float amount)
{
    uint64_t body = up::obj_field(limb, "body");
    if (!is_player(body))
    {
        up::release(body);
        return false;
    }
    up_value amount_arg = up::value_f32(amount);
    up_value drain_result;
    std::memset(&drain_result, 0, sizeof(drain_result));
    if (!up::invoke(container, "CalculateDrain", &amount_arg, 1,
                    &drain_result))
    {
        up::release(body);
        return false;
    }
    uint64_t drain = drain_result.type == UP_VAL_OBJ
                     ? drain_result.as.obj : 0;
    if (drain == 0)
    {
        up::release(body);
        return false;
    }
    uint64_t stack = up::obj_field(container, "stack");
    uint64_t registry = up::obj_static("Liquids", "Registry");
    int32_t count = up::list_count(drain);
    for (int32_t i = 0; i < count; i++)
    {
        float ml = up::list_get_f32(drain, i);
        if (ml <= 0.0f)
        {
            continue;
        }
        uint64_t entry = up::list_get(stack, i);
        std::string liquid;
        if (entry != 0)
        {
            up::str_field(entry, "liquidId", &liquid);
        }
        if (is_drug(liquid))
        {
            apply_drug(body, liquid, ml);
        }
        else
        {
            uint64_t type = registry != 0
                            ? up::dict_get(registry, liquid.c_str())
                            : 0;
            if (type != 0)
            {
                float sickness = 0.0f;
                bool injectable = false;
                up::f32_field(type, "injectionSickness", &sickness);
                up::bool_field(type, "injectable", &injectable);
                if (sickness > 0.0f)
                {
                    add_f(body, "sicknessAmount", sickness * 0.35f * ml);
                }
                add_f(body, "bloodViscosity", -sickness * 0.1f);
                if (injectable)
                {
                    call_liquid_delegate(type, "onHealthUse", ml, limb);
                }
                up::release(type);
            }
        }
        up::release(entry);
    }
    up_value drain_arg = up::value_obj(drain);
    up::invoke(container, "Drain", &drain_arg, 1, nullptr);
    up::release(drain);
    up::release(stack);
    up::release(registry);
    up::release(body);
    return true;
}

static void lobotomy(uint64_t body, uint64_t item)
{
    State& s = g_state;
    if (s.needle_cooldown > 0.0f)
    {
        talk(body, "...not again.");
        return;
    }
    s.needle_cooldown = 90.0f;
    add_f(body, "brainHealth", -rnd_range(25.0f, 40.0f));
    add_f(body, "shock", 60.0f);
    add_f(body, "happiness", 10.0f);
    write_f(body, "traumaAmount", 0.0f);
    write_f(body, "horrifiedLevel", 0.0f);
    s.ssri_load = 0.0f;
    s.condition_ocd = 0.0f;
    s.condition_ptsd = std::max(0.0f, s.condition_ptsd - 20.0f);
    s.condition_schizo = std::max(0.0f, s.condition_schizo - 30.0f);
    s.condition_organic = 100.0f;
    uint64_t skills = up::obj_field(body, "skills");
    if (skills != 0)
    {
        int32_t intel = 0;
        if (read_i(skills, "INT", &intel))
        {
            up::set_i32(skills, "INT", intel > 3 ? intel - 3 : 0);
        }
        up::release(skills);
    }
    uint64_t limbs = up::obj_field(body, "limbs");
    if (limbs != 0)
    {
        uint64_t head = up::array_get(limbs, 0);
        if (head != 0)
        {
            add_f(head, "pain", 40.0f);
            add_f(head, "bleedAmount", 20.0f);
            up::release(head);
        }
        up::release(limbs);
    }
    float condition = 1.0f;
    up::f32_field(item, "condition", &condition);
    up::set_f32(item, "condition", std::max(0.0f, condition - 0.5f));
    play_at_body(body, "lobotomy");
    play_at_body(body, "selfharm");
    up::invoke(body, "Ragdoll", nullptr, 0, nullptr);
    talk(body, "...");
}

bool on_use_item(uint64_t body, uint64_t item)
{
    int32_t body_id = up::instance_id(body);
    if (g_state.body_instance == 0)
    {
        reset_state(body_id);
    }
    else if (body_id != g_state.body_instance)
    {
        return false;
    }
    std::string id;
    if (!up::str_field(item, "id", &id) || id != "psy_needle")
    {
        return false;
    }
    lobotomy(body, item);
    return true;
}

static uint64_t hand_item(uint64_t body)
{
    int32_t slot = 0;
    if (!read_i(body, "handSlot", &slot))
    {
        return 0;
    }
    up_value arg = up::value_i32(slot);
    up_value result;
    std::memset(&result, 0, sizeof(result));
    up::invoke(body, "GetItem", &arg, 1, &result);
    return result.type == UP_VAL_OBJ ? result.as.obj : 0;
}

bool on_use_item_in_hand(uint64_t body)
{
    int32_t body_id = up::instance_id(body);
    if (g_state.body_instance == 0)
    {
        reset_state(body_id);
    }
    else if (body_id != g_state.body_instance)
    {
        return false;
    }
    if (g_state.compulsion_kind == 3)
    {
        g_state.compulsion_kind = 0;
        g_state.compulsion_timer = 0.0f;
        add_f(body, "happiness", 4.0f);
        add_f(body, "traumaAmount", -2.0f);
    }
    uint64_t item = hand_item(body);
    if (item == 0)
    {
        return false;
    }
    std::string id;
    bool handled = up::str_field(item, "id", &id) &&
                   id == "psy_needle";
    if (handled)
    {
        lobotomy(body, item);
    }
    up::release(item);
    return handled;
}

bool on_take_nap(uint64_t body)
{
    int32_t body_id = up::instance_id(body);
    if (g_state.body_instance == 0)
    {
        reset_state(body_id);
    }
    else if (body_id != g_state.body_instance)
    {
        return false;
    }
    State& s = g_state;
    if (s.nap_cooldown > 0.0f)
    {
        return false;
    }
    float chance = 0.0f;
    if (s.condition_insomnia > 0.0f)
    {
        chance = 0.7f;
    }
    if (s.phobia_insomno > 0.0f)
    {
        chance += 0.15f;
    }
    if (chance <= 0.0f || rnd01() > chance)
    {
        return false;
    }
    s.nap_cooldown = 25.0f;
    s.insomnia_fails++;
    if (s.insomnia_fails >= 3)
    {
        s.phobia_insomno = std::max(s.phobia_insomno, 40.0f);
    }
    add_f(body, "energy", -6.0f);
    add_f(body, "happiness", -3.0f);
    add_f(body, "badSleepAmount", 20.0f);
    talk(body, "I can't sleep.");
    play_2d("stretch");
    return true;
}

void on_drop_item(uint64_t body)
{
    (void)body;
    if (g_state.compulsion_kind == 1)
    {
        g_state.compulsion_kind = 0;
        g_state.compulsion_timer = 0.0f;
        add_f(body, "happiness", 4.0f);
        add_f(body, "traumaAmount", -2.0f);
    }
}

void on_switch_hands(uint64_t body)
{
    if (g_state.compulsion_kind == 2)
    {
        g_state.compulsion_kind = 0;
        g_state.compulsion_timer = 0.0f;
        add_f(body, "happiness", 4.0f);
        add_f(body, "traumaAmount", -2.0f);
    }
}

void on_spider_bite(uint64_t limb)
{
    (void)limb;
    g_state.exposure_creature += 15.0f;
    g_state.exposure_spider += 8.0f;
}

void on_guard_died(uint64_t guard)
{
    bool by_player = false;
    read_b(guard, "hurtByPlayer", &by_player);
    if (by_player)
    {
        g_state.exposure_corpse += 35.0f;
        g_state.exposure_guard += 10.0f;
    }
}

void on_guard_attacked(uint64_t guard)
{
    uint64_t body = current_body();
    if (body == 0)
    {
        return;
    }
    float x = 0.0f;
    float y = 0.0f;
    float bx = 0.0f;
    float by = 0.0f;
    if (position_of(guard, &x, &y) && position_of(body, &bx, &by))
    {
        float dx = x - bx;
        float dy = y - by;
        if (dx * dx + dy * dy < 400.0f)
        {
            g_state.exposure_guard += 15.0f;
        }
    }
    up::release(body);
}

void on_mine_pressed(uint64_t mine)
{
    uint64_t body = current_body();
    if (body == 0)
    {
        return;
    }
    float x = 0.0f;
    float y = 0.0f;
    float bx = 0.0f;
    float by = 0.0f;
    if (position_of(mine, &x, &y) && position_of(body, &bx, &by))
    {
        float dx = x - bx;
        float dy = y - by;
        if (dx * dx + dy * dy < 64.0f &&
            count_near(body, "BushCol", 10.0f) > 0)
        {
            g_state.mine_bush_pending = true;
        }
    }
    up::release(body);
}

void on_mine_update(uint64_t mine)
{
    if (!g_state.mine_bush_pending)
    {
        return;
    }
    bool pressed = false;
    float elapsed = 0.0f;
    read_b(mine, "pressed", &pressed);
    up::f32_field(mine, "timeSincePressed", &elapsed);
    if (!pressed || elapsed < 0.8f)
    {
        return;
    }
    g_state.mine_bush_pending = false;
    g_state.exposure_bush = 100.0f;
    g_state.phobia_bush = std::max(g_state.phobia_bush, 55.0f);
    uint64_t body = current_body();
    if (body != 0)
    {
        add_f(body, "traumaAmount", 20.0f);
        scare(body, 40.0f, -5.0f);
        up::release(body);
    }
}

void compulsion_start(uint64_t body, State& s)
{
    s.compulsion_kind = 1 + static_cast<int32_t>(rnd01() * 3.0f);
    s.compulsion_timer = 15.0f;
    if (s.compulsion_kind == 1)
    {
        talk(body, "I must drop something.");
    }
    else if (s.compulsion_kind == 2)
    {
        talk(body, "I must switch hands.");
    }
    else
    {
        talk(body, "I must use it. Now.");
    }
}

void compulsion_decay(uint64_t body, float dt, State& s)
{
    if (s.compulsion_kind == 0)
    {
        return;
    }
    s.compulsion_timer -= dt;
    if (s.compulsion_timer > 0.0f)
    {
        return;
    }
    s.compulsion_kind = 0;
    add_f(body, "happiness", -15.0f);
    add_f(body, "traumaAmount", 4.0f);
    s.condition_ocd = clampf(s.condition_ocd + 4.0f, 0.0f, 100.0f);
    talk(body, "...");
}

static void moodle(uint64_t manager, int32_t intensity,
                   const char* icon, const char* name,
                   const char* desc, bool critical)
{
    up_value args[6];
    args[0] = up::value_i32(intensity);
    args[1] = up::value_str(icon);
    args[2] = up::value_str(name);
    args[3] = up::value_str(desc);
    args[4] = up::value_bool(critical);
    args[5] = up::value_bool(false);
    up::invoke(manager, "AddMoodle", args, 6, nullptr);
}

void add_moodles(uint64_t manager)
{
    uint64_t body = current_body();
    if (body == 0 || !body_alive(body))
    {
        up::release(body);
        return;
    }
    State& s = g_state;
    if (s.condition_ocd > 0.0f)
    {
        moodle(manager, 1, "focused", "OCD",
               "Intrusive urges. Ignoring them hurts.", false);
    }
    if (s.freeze_timer > 0.0f)
    {
        moodle(manager, 2, "confused", "Derealization",
               "The body will not move. Nothing feels real.", true);
    }
    if (s.condition_ptsd > 0.0f)
    {
        moodle(manager, 1, "horrified", "PTSD",
               "Flashbacks of the dead.", false);
    }
    if (s.condition_cptsd > 0.0f)
    {
        moodle(manager, 3, "horrified", "C-PTSD",
               "The creatures are never far away.", true);
    }
    if (s.condition_organic > 0.0f)
    {
        moodle(manager, 2, "concussion",
               "Organic personality disorder",
               "A broken brain. Antidepressants will not fix this.",
               false);
    }
    if (s.condition_schizo > 0.0f)
    {
        moodle(manager, 2, "impendingdoom", "Schizophrenia",
               "Sounds that are not there. Something behind you.", true);
    }
    if (s.condition_insomnia > 0.0f)
    {
        moodle(manager, 2, "tired", "Insomnia",
               "Exhausted, but sleep will not come.", false);
    }
    if (s.phobia_arachno > 0.0f)
    {
        moodle(manager, 3, "impendingdoom", "Arachnophobia",
               "Ticks and shadows. Every leg is a threat.", false);
    }
    if (s.phobia_insomno > 0.0f)
    {
        moodle(manager, 1, "badsleep", "Insomnophobia",
               "The fear of never sleeping again.", false);
    }
    if (s.phobia_hemo > 0.0f)
    {
        moodle(manager, 3, "trauma", "Hemophobia",
               "Blood. Your own blood.", false);
    }
    if (s.phobia_humano > 0.0f)
    {
        moodle(manager, 2, "horrified", "Humanoidophobia",
               "The survivors are the worst monsters.", false);
    }
    if (s.phobia_bush > 0.0f)
    {
        moodle(manager, 1, "confused", "Bushophobia",
               "Something waits in the bushes.", false);
    }
    up::release(body);
}

}
