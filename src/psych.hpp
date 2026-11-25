#pragma once

#include <cstdint>

namespace psych {

struct State {
    int32_t body_instance = 0;
    float ssri_load = 0.0f;
    float side_timer = 6.0f;
    float mood_pending = 0.0f;
    float mood_timer = 0.0f;
    float second_accum = 0.0f;
    float scan_accum = 0.0f;
    float fright_accum = 0.0f;
    float exposure_spider = 0.0f;
    float exposure_corpse = 0.0f;
    float exposure_creature = 0.0f;
    float exposure_guard = 0.0f;
    float exposure_blood = 0.0f;
    float exposure_bush = 0.0f;
    int32_t last_corpses = 0;
    float low_brain_time = 0.0f;
    float condition_ocd = 0.0f;
    float condition_ptsd = 0.0f;
    float condition_cptsd = 0.0f;
    float condition_organic = 0.0f;
    float condition_schizo = 0.0f;
    float condition_insomnia = 0.0f;
    float phobia_arachno = 0.0f;
    float phobia_insomno = 0.0f;
    float phobia_hemo = 0.0f;
    float phobia_humano = 0.0f;
    float phobia_bush = 0.0f;
    float med_valproate = 0.0f;
    float med_quetiapine = 0.0f;
    float med_haloperidol = 0.0f;
    float med_benzo = 0.0f;
    float benzo_withdrawal = 0.0f;
    float benzo_uses = 0.0f;
    float freeze_timer = 0.0f;
    float compulsion_timer = 0.0f;
    int32_t compulsion_kind = 0;
    float compulsion_cooldown = 0.0f;
    int32_t insomnia_fails = 0;
    float ocd_timer = 60.0f;
    float dereal_timer = 90.0f;
    float ptsd_timer = 120.0f;
    float schizo_timer = 60.0f;
    float panic_timer = 90.0f;
    float organic_timer = 60.0f;
    float needle_cooldown = 0.0f;
    float nap_cooldown = 0.0f;
    bool mine_bush_pending = false;
};

uint64_t current_body();

bool body_alive(uint64_t body);

void reset_state(int32_t instance);

void tick(float dt);

void on_take_dose(uint64_t antidepressants, float dose);

bool on_drink(uint64_t container, uint64_t body, float amount,
              const char* sound);

bool on_inject(uint64_t container, uint64_t limb, float amount);

bool on_use_item(uint64_t body, uint64_t item);

bool on_use_item_in_hand(uint64_t body);

bool on_take_nap(uint64_t body);

void on_drop_item(uint64_t body);

void on_switch_hands(uint64_t body);

void on_spider_bite(uint64_t limb);

void on_guard_died(uint64_t guard);

void on_guard_attacked(uint64_t guard);

void on_mine_pressed(uint64_t mine);

void on_mine_update(uint64_t mine);

void add_moodles(uint64_t manager);

}
