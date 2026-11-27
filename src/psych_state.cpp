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

static bool read_grounded(uint64_t body)
{
    bool value = false;
    up::bool_field(body, "grounded", &value);
    return value;
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
    (void)damage;
    for (int32_t i = 0; i < count; i++)
    {
        up::invoke(body, "DoUnarmedAttack", nullptr, 0, nullptr);
    }
    play_2d("BSSwing1");
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





static bool any_limb_broken(uint64_t body)
{
    uint64_t limbs = up::obj_field(body, "limbs");
    if (limbs == 0)
    {
        return false;
    }
    bool broken = false;
    int32_t count = up::array_length(limbs);
    for (int32_t i = 0; i < count; i++)
    {
        uint64_t limb = up::array_get(limbs, i);
        if (limb == 0)
        {
            continue;
        }
        bool value = false;
        if (up::bool_field(limb, "broken", &value) && value)
        {
            broken = true;
        }
        up::release(limb);
    }
    up::release(limbs);
    return broken;
}

static void scare(uint64_t body, float adrenaline, float mood)
{
    if (g_state.med_propranolol > 0.0f)
    {
        adrenaline *= 0.25f;
        mood *= 0.6f;
    }
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

static void reduce_phobias(State& s, float amount);

static void update_meds(uint64_t body, float dt, State& s)
{
    s.med_valproate = std::max(0.0f, s.med_valproate - dt);
    s.med_quetiapine = std::max(0.0f, s.med_quetiapine - dt);
    s.med_haloperidol = std::max(0.0f, s.med_haloperidol - dt);
    s.med_venlafaxine = std::max(0.0f, s.med_venlafaxine - dt);
    s.med_propranolol = std::max(0.0f, s.med_propranolol - dt);
    s.med_hydroxyzine = std::max(0.0f, s.med_hydroxyzine - dt);
    s.med_pregabalin = std::max(0.0f, s.med_pregabalin - dt);
    bool had_benzo = s.med_benzo > 0.0f;
    s.med_benzo = std::max(0.0f, s.med_benzo - dt);
    if (had_benzo && s.med_benzo <= 0.0f)
    {
        s.benzo_withdrawal += 6.0f;
    }
    s.benzo_withdrawal = std::max(0.0f, s.benzo_withdrawal - dt * 0.05f);

    s.medicine_dose = std::max(0.0f, s.medicine_dose - dt * 3.0f);
    s.narcotic_dose = std::max(0.0f, s.narcotic_dose - dt * 3.0f);
    s.medicine_cooldown = std::max(0.0f, s.medicine_cooldown - dt);
    s.narcotic_cooldown = std::max(0.0f, s.narcotic_cooldown - dt);
    int32_t overdose = 0;
    read_i(body, "overdoseIndex", &overdose);
    float opiate = 0.0f;
    uint64_t painkillers = up::component(body, "Painkillers");
    if (painkillers != 0)
    {
        up::f32_field(painkillers, "actualOpiateReception", &opiate);
        up::release(painkillers);
    }
    bool medicine_overdose = s.medicine_dose > 200.0f ||
                             (overdose > 0 && opiate <= 60.0f);
    if (medicine_overdose && s.medicine_cooldown <= 0.0f)
    {
        s.exposure_pharma += 60.0f;
        s.medicine_cooldown = 90.0f;
    }
    bool narcotic_overdose = s.narcotic_dose > 150.0f || opiate > 80.0f ||
                             (overdose > 0 && opiate > 60.0f);
    if (narcotic_overdose && s.narcotic_cooldown <= 0.0f)
    {
        s.exposure_narco += 60.0f;
        s.narcotic_cooldown = 90.0f;
    }

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
    if (s.med_venlafaxine > 0.0f)
    {
        reduce_phobias(s, dt * 0.8f);
        add_f(body, "traumaAmount", -dt * 0.05f);
    }
    if (s.med_pregabalin > 0.0f)
    {
        reduce_phobias(s, dt * 0.6f);
        add_f(body, "traumaAmount", -dt * 0.05f);
    }
    if (s.med_propranolol > 0.0f)
    {
        float rate = read_f(body, "heartRate");
        write_f(body, "heartRate", move_to(rate, 70.0f, dt * 3.0f));
        float adrenaline = read_f(body, "adrenaline");
        write_f(body, "adrenaline", move_to(adrenaline, 0.0f,
                                            dt * 4.0f));
        float current = read_f(body, "curAdrenaline");
        write_f(body, "curAdrenaline", move_to(current, 0.0f,
                                               dt * 4.0f));
    }
    if (s.med_hydroxyzine > 0.0f)
    {
        add_f(body, "traumaAmount", -dt * 0.15f);
        add_f(body, "energy", -dt * 0.04f);
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
            int32_t fresh = corpses - s.last_corpses;
            s.exposure_corpse += fresh * 20.0f;
            s.exposure_necro += fresh * 8.0f;
            s.last_corpses = corpses;
            if (s.phobia_necro > 0.0f)
            {
                scare(body, 15.0f, -1.5f);
            }
        }
    }
    float bleed = read_f(body, "totalBleedSpeed");
    if (bleed > 0.05f)
    {
        s.exposure_blood += dt * 2.5f;
    }
    float dirt = read_f(body, "dirtyness");
    if (dirt > 50.0f)
    {
        s.exposure_dirt += dt * 1.2f;
    }
    else
    {
        s.exposure_dirt = std::max(0.0f, s.exposure_dirt - dt * 0.5f);
    }
    float hunger = read_f(body, "hunger");
    if (hunger > 105.0f)
    {
        s.exposure_food += dt * 1.5f;
    }
    else
    {
        s.exposure_food = std::max(0.0f, s.exposure_food - dt * 0.5f);
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
    bool in_water = false;
    read_b(body, "inWater", &in_water);
    bool in_oil = in_water && dirt > s.last_dirt + 5.0f;
    s.in_water = in_water && !in_oil;
    s.in_oil = in_oil;
    if (in_water)
    {
        if (in_oil)
        {
            s.exposure_oil += 10.0f;
        }
        else
        {
            s.exposure_water += 8.0f;
        }
    }
    s.last_dirt = dirt;
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
    if (s.phobia_anthropo == 0.0f && s.exposure_anthropo > 70.0f &&
        rnd01() < 0.02f)
    {
        s.phobia_anthropo = 45.0f;
    }
    if (s.phobia_hemo == 0.0f && s.exposure_blood > 60.0f &&
        rnd01() < 0.02f)
    {
        s.phobia_hemo = 45.0f;
    }
    if (s.phobia_botan == 0.0f && s.exposure_botan > 50.0f &&
        rnd01() < 0.05f)
    {
        s.phobia_botan = 55.0f;
    }
    if (s.phobia_acropho == 0.0f && s.exposure_fall > 60.0f &&
        rnd01() < 0.02f)
    {
        s.phobia_acropho = 50.0f;
    }
    if (s.phobia_pharma == 0.0f && s.exposure_pharma > 40.0f &&
        rnd01() < 0.02f)
    {
        s.phobia_pharma = 45.0f;
    }
    if (s.phobia_narco == 0.0f && s.exposure_narco > 40.0f &&
        rnd01() < 0.02f)
    {
        s.phobia_narco = 45.0f;
    }
    if (s.phobia_tsibo == 0.0f && s.exposure_food > 60.0f &&
        rnd01() < 0.02f)
    {
        s.phobia_tsibo = 45.0f;
    }
    if (s.phobia_abluto == 0.0f && s.exposure_dirt > 60.0f &&
        rnd01() < 0.02f)
    {
        s.phobia_abluto = 45.0f;
    }
    if (s.phobia_necro == 0.0f && s.exposure_necro > 80.0f &&
        rnd01() < 0.02f)
    {
        s.phobia_necro = 45.0f;
    }
    if (s.phobia_splanch == 0.0f && s.exposure_organs > 40.0f &&
        rnd01() < 0.02f)
    {
        s.phobia_splanch = 50.0f;
    }
    if (s.phobia_aqua == 0.0f && s.exposure_water > 40.0f &&
        rnd01() < 0.05f)
    {
        s.phobia_aqua = 45.0f;
    }
    if (s.phobia_petro == 0.0f && s.exposure_oil > 30.0f &&
        rnd01() < 0.05f)
    {
        s.phobia_petro = 45.0f;
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
    if (s.phobia_hemo > 0.0f &&
        read_f(body, "totalBleedSpeed") > 0.02f)
    {
        scare(body, 10.0f, -1.5f);
    }
    if (s.phobia_insomno > 0.0f && read_f(body, "energy") < 40.0f)
    {
        scare(body, 12.0f, -1.0f);
    }
    if (s.phobia_tsibo > 0.0f && read_f(body, "hunger") > 100.0f)
    {
        scare(body, 10.0f, -1.0f);
    }
    if (s.phobia_abluto > 0.0f && read_f(body, "dirtyness") > 50.0f)
    {
        scare(body, 10.0f, -1.0f);
    }
    if (s.phobia_acropho > 0.0f && !read_grounded(body) &&
        s.fall_time > 1.0f)
    {
        scare(body, 20.0f, -1.5f);
    }
    if (s.phobia_aqua > 0.0f && s.in_water)
    {
        scare(body, 18.0f, -1.5f);
    }
    if (s.phobia_petro > 0.0f && s.in_oil)
    {
        scare(body, 18.0f, -1.5f);
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
    s.bush_touch_timer = std::max(0.0f, s.bush_touch_timer - dt);
    s.organ_timer = std::max(0.0f, s.organ_timer - dt);
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
        if (panic && s.med_benzo <= 0.0f && s.med_quetiapine <= 0.0f &&
            s.med_hydroxyzine <= 0.0f && s.med_pregabalin <= 0.0f)
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

static void update_fall(uint64_t body, float dt, State& s)
{
    bool grounded = read_grounded(body);
    if (!grounded)
    {
        s.fall_time += dt;
    }
    if (grounded && !s.was_grounded)
    {
        if (s.fall_time > 0.7f)
        {
            bool broken = any_limb_broken(body);
            s.exposure_fall += s.fall_time * 20.0f;
            if (broken)
            {
                s.exposure_fall += 40.0f;
                s.phobia_acropho = std::max(s.phobia_acropho, 45.0f);
            }
            if (s.fall_scream)
            {
                s.exposure_fall += 20.0f;
            }
            if (s.phobia_acropho > 0.0f &&
                (broken || s.fall_scream))
            {
                s.freeze_timer = std::max(s.freeze_timer,
                                          rnd_range(2.0f, 5.0f));
                up::invoke(body, "Ragdoll", nullptr, 0, nullptr);
                talk(body, "I can't get up...");
            }
        }
        s.fall_time = 0.0f;
        s.fall_scream = false;
    }
    s.was_grounded = grounded;
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
    update_fall(body, dt, s);
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
            g_state.medicine_dose += dose * 0.2f;
        }
    }
    up::release(body);
}

static const char* const kMedicineIds[] = {
    "antidepressants", "antibiotics", "braingrow", "mindwipe",
    "sleepingpills", "painkillers", "naloxone", "naltrexone",
    "amiodarone", "vasopressin", "sodiumnitroprusside", "ceftriaxone",
    "keratinbooster", "disinfectant", "paincream", "woundglue",
    "bloodcoagulant", "combatpen", "streptokinase", "saline",
    "ringersolution", "bloodbag", "bloodbaghuman", "antiserum",
    "antirad", "antivenom", "psy_valproate", "psy_quetiapine",
    "psy_haloperidol", "psy_venlafaxine", "psy_propranolol",
    "psy_hydroxyzine", "psy_pregabalin", "diazepam", "flumazenil",
    "propranolol", "phenobarbital", "chloralhydrate",
    "sodiumoxybate", "fowlerssolution"
};

static const char* const kNarcoticIds[] = {
    "morphine", "heroin", "fentanyl", "opium", "methadone",
    "oxycodone", "codeine", "tramadol", "buprenorphine",
    "desomorphine", "carfentanil", "alphapvp", "mephedrone",
    "cocaine", "pervitin", "ketamine", "kinetic", "liquidcannabis",
    "lsd", "psilocybin", "modafinil", "nicotine", "oxylitin",
    "scopolamine", "psy_mdma", "psy_psilocybin", "mdma"
};

static bool in_list(const char* const* list, size_t count,
                    const std::string& id)
{
    for (size_t i = 0; i < count; i++)
    {
        if (id == list[i])
        {
            return true;
        }
    }
    return false;
}

static bool is_medicine(const std::string& id)
{
    return in_list(kMedicineIds,
                   sizeof(kMedicineIds) / sizeof(kMedicineIds[0]), id);
}

static bool is_narcotic(const std::string& id)
{
    return in_list(kNarcoticIds,
                   sizeof(kNarcoticIds) / sizeof(kNarcoticIds[0]), id);
}

static bool is_benzo(const std::string& id)
{
    return id == "psy_benzo" || id == "diazepam";
}

static bool is_drug(const std::string& id)
{
    return id == "psy_valproate" || id == "psy_quetiapine" ||
           id == "psy_benzo" || id == "psy_haloperidol" ||
           id == "psy_venlafaxine" || id == "psy_propranolol" ||
           id == "psy_hydroxyzine" || id == "psy_pregabalin" ||
           id == "psy_mdma" || id == "psy_psilocybin";
}

static void clear_phobias(State& s)
{
    s.phobia_arachno = 0.0f;
    s.phobia_insomno = 0.0f;
    s.phobia_hemo = 0.0f;
    s.phobia_anthropo = 0.0f;
    s.phobia_botan = 0.0f;
    s.phobia_acropho = 0.0f;
    s.phobia_pharma = 0.0f;
    s.phobia_narco = 0.0f;
    s.phobia_tsibo = 0.0f;
    s.phobia_abluto = 0.0f;
    s.phobia_necro = 0.0f;
    s.phobia_splanch = 0.0f;
    s.phobia_aqua = 0.0f;
    s.phobia_petro = 0.0f;
}

static void reduce_phobias(State& s, float amount)
{
    s.phobia_arachno = std::max(0.0f, s.phobia_arachno - amount);
    s.phobia_insomno = std::max(0.0f, s.phobia_insomno - amount);
    s.phobia_hemo = std::max(0.0f, s.phobia_hemo - amount);
    s.phobia_anthropo = std::max(0.0f, s.phobia_anthropo - amount);
    s.phobia_botan = std::max(0.0f, s.phobia_botan - amount);
    s.phobia_acropho = std::max(0.0f, s.phobia_acropho - amount);
    s.phobia_pharma = std::max(0.0f, s.phobia_pharma - amount);
    s.phobia_narco = std::max(0.0f, s.phobia_narco - amount);
    s.phobia_tsibo = std::max(0.0f, s.phobia_tsibo - amount);
    s.phobia_abluto = std::max(0.0f, s.phobia_abluto - amount);
    s.phobia_necro = std::max(0.0f, s.phobia_necro - amount);
    s.phobia_splanch = std::max(0.0f, s.phobia_splanch - amount);
    s.phobia_aqua = std::max(0.0f, s.phobia_aqua - amount);
    s.phobia_petro = std::max(0.0f, s.phobia_petro - amount);
}

static void apply_benzo(uint64_t body, float ml)
{
    State& s = g_state;
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

static void note_intake(uint64_t body, const std::string& id, float ml)
{
    State& s = g_state;
    if (is_medicine(id))
    {
        s.medicine_dose += ml;
    }
    else if (is_narcotic(id))
    {
        s.narcotic_dose += ml;
    }
    if (id == "diazepam")
    {
        apply_benzo(body, ml);
    }
    if (s.phobia_pharma > 0.0f && is_medicine(id))
    {
        scare(body, 15.0f, -1.5f);
    }
    if (s.phobia_narco > 0.0f && is_narcotic(id))
    {
        scare(body, 15.0f, -1.5f);
    }
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
        apply_benzo(body, ml);
    }
    else if (id == "psy_haloperidol")
    {
        s.med_haloperidol += ml * 15.0f;
        s.condition_schizo = std::max(0.0f,
                                      s.condition_schizo - ml);
        add_f(body, "happiness", -ml * 0.05f);
    }
    else if (id == "psy_venlafaxine")
    {
        s.med_venlafaxine += ml * 6.0f;
        s.ssri_load = clampf(s.ssri_load + ml * 0.08f, 0.0f, 100.0f);
        add_f(body, "traumaAmount", -ml * 0.1f);
    }
    else if (id == "psy_propranolol")
    {
        s.med_propranolol += ml * 8.0f;
        add_f(body, "heartRate", -ml * 0.2f);
    }
    else if (id == "psy_hydroxyzine")
    {
        s.med_hydroxyzine += ml * 8.0f;
        add_f(body, "traumaAmount", -ml * 0.2f);
        add_f(body, "energy", -ml * 0.05f);
    }
    else if (id == "psy_pregabalin")
    {
        s.med_pregabalin += ml * 10.0f;
        add_f(body, "traumaAmount", -ml * 0.1f);
    }
    else if (id == "psy_mdma")
    {
        clear_phobias(s);
        s.ssri_load = clampf(s.ssri_load + 35.0f, 0.0f, 100.0f);
        add_f(body, "sicknessAmount", 40.0f);
        add_f(body, "temperature", 1.5f);
        add_f(body, "adrenaline", 40.0f);
        add_f(body, "happiness", 15.0f);
        schedule_mood(-20.0f, 10.0f);
        vomit(body);
        hallucinate(body, 2);
        talk(body, "Everything is... too much.");
        play_2d("heartbeat");
    }
    else if (id == "psy_psilocybin")
    {
        clear_phobias(s);
        add_f(body, "sicknessAmount", 30.0f);
        add_f(body, "happiness", -10.0f);
        schedule_mood(20.0f, 12.0f);
        camera_field("bonusAbber", 3.0f);
        hallucinate(body, 3);
        if (rnd01() < 0.6f)
        {
            vomit(body);
        }
        talk(body, "The walls are breathing.");
        play_2d("heartbeat");
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
    clear_phobias(s);
    s.med_valproate = 0.0f;
    s.med_quetiapine = 0.0f;
    s.med_haloperidol = 0.0f;
    s.med_benzo = 0.0f;
    s.med_venlafaxine = 0.0f;
    s.med_propranolol = 0.0f;
    s.med_hydroxyzine = 0.0f;
    s.med_pregabalin = 0.0f;
    s.benzo_withdrawal = 0.0f;
    s.exposure_spider = 0.0f;
    s.exposure_corpse = 0.0f;
    s.exposure_creature = 0.0f;
    s.exposure_anthropo = 0.0f;
    s.exposure_blood = 0.0f;
    s.exposure_botan = 0.0f;
    s.exposure_fall = 0.0f;
    s.exposure_pharma = 0.0f;
    s.exposure_narco = 0.0f;
    s.exposure_food = 0.0f;
    s.exposure_dirt = 0.0f;
    s.exposure_necro = 0.0f;
    s.exposure_organs = 0.0f;
    s.exposure_water = 0.0f;
    s.exposure_oil = 0.0f;
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
        note_intake(body, liquid, ml);
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
        note_intake(body, liquid, ml);
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
    g_state.exposure_creature += 15.0f;
    g_state.exposure_spider += 8.0f;
    if (g_state.phobia_arachno > 0.0f)
    {
        uint64_t body = up::obj_field(limb, "body");
        if (body != 0)
        {
            scare(body, 25.0f, -2.0f);
            up::release(body);
        }
    }
}

void on_guard_died(uint64_t guard)
{
    bool by_player = false;
    read_b(guard, "hurtByPlayer", &by_player);
    if (by_player)
    {
        g_state.exposure_corpse += 35.0f;
        g_state.exposure_anthropo += 10.0f;
    }
}

void on_guard_attacked(uint64_t guard)
{
    (void)guard;
    g_state.exposure_anthropo += 15.0f;
    if (g_state.phobia_anthropo > 0.0f)
    {
        uint64_t body = current_body();
        if (body != 0)
        {
            scare(body, 20.0f, -1.5f);
            up::release(body);
        }
    }
}

static bool object_is_player(uint64_t obj)
{
    if (obj == 0)
    {
        return false;
    }
    uint64_t game_object = up::obj_field(obj, "gameObject");
    if (game_object == 0)
    {
        return false;
    }
    bool result = false;
    uint64_t limb = up::component(game_object, "Limb");
    if (limb != 0)
    {
        uint64_t body = up::obj_field(limb, "body");
        if (body != 0)
        {
            result = up::instance_id(body) == g_state.body_instance;
            up::release(body);
        }
        up::release(limb);
    }
    else
    {
        uint64_t body = up::component(game_object, "Body");
        if (body != 0)
        {
            result = up::instance_id(body) == g_state.body_instance;
            up::release(body);
        }
    }
    up::release(game_object);
    return result;
}

void on_building_health(uint64_t building, float old_health,
                        float new_health)
{
    if (new_health >= old_health)
    {
        return;
    }
    bool humanoid = false;
    uint64_t trader = up::component(building, "TraderScript");
    if (trader != 0)
    {
        humanoid = true;
        up::release(trader);
    }
    if (!humanoid)
    {
        uint64_t guard = up::component(building, "GuardScript");
        if (guard != 0)
        {
            humanoid = true;
            up::release(guard);
        }
    }
    if (!humanoid)
    {
        return;
    }
    State& s = g_state;
    s.exposure_anthropo += 6.0f;
    s.exposure_corpse += 2.0f;
    bool killed = new_health < 0.5f && old_health >= 0.5f;
    if (killed)
    {
        s.exposure_corpse += 35.0f;
        s.exposure_anthropo += 15.0f;
    }
    if (s.phobia_anthropo > 0.0f && (killed || rnd01() < 0.15f))
    {
        uint64_t body = current_body();
        if (body != 0)
        {
            scare(body, 20.0f, -1.5f);
            up::release(body);
        }
    }
}

void on_scream(uint64_t body)
{
    if (!read_grounded(body) &&
        up::instance_id(body) == g_state.body_instance)
    {
        g_state.fall_scream = true;
    }
}

void on_bush_touched(uint64_t collider)
{
    if (!object_is_player(collider))
    {
        return;
    }
    g_state.bush_touch_timer = 20.0f;
    if (g_state.phobia_botan > 0.0f)
    {
        uint64_t body = current_body();
        if (body != 0)
        {
            scare(body, 15.0f, -1.0f);
            up::release(body);
        }
    }
}

void on_mine_pressed(uint64_t mine, uint64_t collision)
{
    (void)mine;
    uint64_t collider = up::obj_field(collision, "collider");
    bool mine_hit = object_is_player(collider);
    up::release(collider);
    if (!mine_hit || g_state.bush_touch_timer <= 0.0f)
    {
        return;
    }
    g_state.bush_touch_timer = 0.0f;
    g_state.exposure_botan += 100.0f;
    g_state.phobia_botan = std::max(g_state.phobia_botan, 55.0f);
    uint64_t body = current_body();
    if (body != 0)
    {
        add_f(body, "traumaAmount", 20.0f);
        scare(body, 40.0f, -5.0f);
        up::release(body);
    }
}

void on_item_visible(uint64_t item)
{
    std::string id;
    if (!up::str_field(item, "id", &id) || id != "internalorgans")
    {
        return;
    }
    State& s = g_state;
    if (s.organ_timer > 0.0f)
    {
        return;
    }
    s.organ_timer = 2.0f;
    s.exposure_organs += 3.0f;
    if (s.phobia_splanch > 0.0f)
    {
        uint64_t body = current_body();
        if (body != 0)
        {
            scare(body, 15.0f, -1.5f);
            up::release(body);
        }
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
    if (s.phobia_anthropo > 0.0f)
    {
        moodle(manager, 2, "horrified", "Anthropomorphophobia",
               "The survivors are the worst monsters.", false);
    }
    if (s.phobia_botan > 0.0f)
    {
        moodle(manager, 1, "confused", "Botanophobia",
               "Something waits in the bushes.", false);
    }
    if (s.phobia_acropho > 0.0f)
    {
        moodle(manager, 1, "impendingdoom", "Acrophobia",
               "The ground is too far away. Falling means lying "
               "there afterwards.", false);
    }
    if (s.phobia_pharma > 0.0f)
    {
        moodle(manager, 1, "sick", "Pharmacophobia",
               "Medicine is poison. Every dose is a risk.", false);
    }
    if (s.phobia_narco > 0.0f)
    {
        moodle(manager, 2, "sick", "Narcophobia",
               "Drugs took something from you. Never again.", false);
    }
    if (s.phobia_tsibo > 0.0f)
    {
        moodle(manager, 0, "sick", "Tsibophobia",
               "Food is a threat. Eating too much is terrifying.", false);
    }
    if (s.phobia_abluto > 0.0f)
    {
        moodle(manager, 0, "dirty", "Ablutophobia",
               "Being dirty feels unbearable.", false);
    }
    if (s.phobia_necro > 0.0f)
    {
        moodle(manager, 2, "impendingdoom", "Necrophobia",
               "The dead are watching.", false);
    }
    if (s.phobia_splanch > 0.0f)
    {
        moodle(manager, 1, "trauma", "Splanchnophobia",
               "Organs. You can still feel them.", false);
    }
    if (s.phobia_aqua > 0.0f)
    {
        moodle(manager, 1, "wet", "Aquaphobia",
               "The water wants to close over your head.", false);
    }
    if (s.phobia_petro > 0.0f)
    {
        moodle(manager, 1, "wet", "Petrophobia",
               "The black slick clings and will not wash off.", false);
    }
    up::release(body);
}

}
