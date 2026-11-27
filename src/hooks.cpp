#include "hooks.hpp"

#include "psych.hpp"
#include "registry.hpp"
#include "up_helpers.hpp"

#include <cstring>
#include <string>

namespace hooks {

static bool before(const up_hook_frame* frame)
{
    return frame != nullptr && frame->phase == UP_HOOK_BEFORE;
}

static void keep_original(up_hook_frame* frame)
{
    frame->skip_original = 1;
}

static void skip_current(up_hook_frame* frame)
{
    frame->skip_original = 0;
}

static int32_t UP_CDECL on_moodles(up_hook_frame* frame)
{
    if (before(frame))
    {
        keep_original(frame);
        if (frame->instance != 0)
        {
            psych::add_moodles(frame->instance);
        }
    }
    return 0;
}

static int32_t UP_CDECL on_take_dose(up_hook_frame* frame)
{
    if (before(frame))
    {
        keep_original(frame);
        if (frame->instance != 0 && frame->argc >= 1 &&
            frame->args[0].type == UP_VAL_F32)
        {
            psych::on_take_dose(frame->instance, frame->args[0].as.f32);
        }
    }
    return 0;
}

static int32_t UP_CDECL on_drink(up_hook_frame* frame)
{
    if (before(frame))
    {
        keep_original(frame);
        if (frame->instance != 0 && frame->argc >= 2 &&
            frame->args[0].type == UP_VAL_OBJ &&
            frame->args[1].type == UP_VAL_F32)
        {
            const char* sound = "drink";
            if (frame->argc >= 3 && frame->args[2].type == UP_VAL_STR &&
                frame->args[2].str != nullptr)
            {
                sound = frame->args[2].str;
            }
            if (psych::on_drink(frame->instance, frame->args[0].as.obj,
                                frame->args[1].as.f32, sound))
            {
                skip_current(frame);
            }
        }
    }
    return 0;
}

static int32_t UP_CDECL on_inject(up_hook_frame* frame)
{
    if (before(frame))
    {
        keep_original(frame);
        if (frame->instance != 0 && frame->argc >= 2 &&
            frame->args[0].type == UP_VAL_OBJ &&
            frame->args[1].type == UP_VAL_F32 &&
            psych::on_inject(frame->instance, frame->args[0].as.obj,
                             frame->args[1].as.f32))
        {
            skip_current(frame);
        }
    }
    return 0;
}

static int32_t UP_CDECL on_use_item(up_hook_frame* frame)
{
    if (before(frame))
    {
        keep_original(frame);
        if (frame->instance != 0 && frame->argc >= 1 &&
            frame->args[0].type == UP_VAL_OBJ &&
            psych::on_use_item(frame->instance, frame->args[0].as.obj))
        {
            skip_current(frame);
        }
    }
    return 0;
}

static int32_t UP_CDECL on_use_in_hand(up_hook_frame* frame)
{
    if (before(frame))
    {
        keep_original(frame);
        if (frame->instance != 0 &&
            psych::on_use_item_in_hand(frame->instance))
        {
            skip_current(frame);
        }
    }
    return 0;
}

static int32_t UP_CDECL on_take_nap(up_hook_frame* frame)
{
    if (before(frame))
    {
        keep_original(frame);
        if (frame->instance != 0 &&
            psych::on_take_nap(frame->instance))
        {
            skip_current(frame);
        }
    }
    return 0;
}

static int32_t UP_CDECL on_drop_item(up_hook_frame* frame)
{
    if (before(frame))
    {
        keep_original(frame);
        if (frame->instance != 0)
        {
            psych::on_drop_item(frame->instance);
        }
    }
    return 0;
}

static int32_t UP_CDECL on_switch_hands(up_hook_frame* frame)
{
    if (before(frame))
    {
        keep_original(frame);
        if (frame->instance != 0)
        {
            psych::on_switch_hands(frame->instance);
        }
    }
    return 0;
}

static int32_t UP_CDECL on_spider_bite(up_hook_frame* frame)
{
    if (before(frame))
    {
        keep_original(frame);
        if (frame->argc >= 1 && frame->args[0].type == UP_VAL_OBJ)
        {
            psych::on_spider_bite(frame->args[0].as.obj);
        }
    }
    return 0;
}

static int32_t UP_CDECL on_guard_died(up_hook_frame* frame)
{
    if (before(frame))
    {
        keep_original(frame);
        if (frame->instance != 0)
        {
            psych::on_guard_died(frame->instance);
        }
    }
    return 0;
}

static int32_t UP_CDECL on_guard_attack(up_hook_frame* frame)
{
    if (before(frame))
    {
        keep_original(frame);
        if (frame->instance != 0)
        {
            psych::on_guard_attacked(frame->instance);
        }
    }
    return 0;
}

static int32_t UP_CDECL on_mine_pressed(up_hook_frame* frame)
{
    if (before(frame))
    {
        keep_original(frame);
        if (frame->instance != 0 && frame->argc >= 1 &&
            frame->args[0].type == UP_VAL_OBJ)
        {
            psych::on_mine_pressed(frame->instance,
                                   frame->args[0].as.obj);
        }
    }
    return 0;
}

static int32_t UP_CDECL on_scream(up_hook_frame* frame)
{
    if (before(frame))
    {
        keep_original(frame);
        if (frame->instance != 0)
        {
            psych::on_scream(frame->instance);
        }
    }
    return 0;
}

static int32_t UP_CDECL on_bush_touched(up_hook_frame* frame)
{
    if (before(frame))
    {
        keep_original(frame);
        if (frame->argc >= 1 && frame->args[0].type == UP_VAL_OBJ)
        {
            psych::on_bush_touched(frame->args[0].as.obj);
        }
    }
    return 0;
}

static int32_t UP_CDECL on_item_visible(up_hook_frame* frame)
{
    if (before(frame))
    {
        keep_original(frame);
        if (frame->instance != 0)
        {
            psych::on_item_visible(frame->instance);
        }
    }
    return 0;
}

static int32_t UP_CDECL on_building_health(up_hook_frame* frame)
{
    if (before(frame))
    {
        keep_original(frame);
        if (frame->instance != 0 && frame->argc >= 1 &&
            frame->args[0].type == UP_VAL_F32)
        {
            float old_health = 0.0f;
            up_value current;
            std::memset(&current, 0, sizeof(current));
            if (up::get_field(frame->instance, "health", &current) &&
                current.type == UP_VAL_F32)
            {
                old_health = current.as.f32;
            }
            psych::on_building_health(frame->instance, old_health,
                                      frame->args[0].as.f32);
        }
    }
    return 0;
}

static void set_result_object(up_hook_frame* frame, uint64_t value)
{
    if (frame->result == nullptr)
    {
        return;
    }
    frame->result->type = UP_VAL_OBJ;
    frame->result->size = static_cast<uint32_t>(sizeof(up_value));
    frame->result->as.obj = value;
    frame->result->str = nullptr;
}

static int32_t UP_CDECL on_resource_load(up_hook_frame* frame)
{
    if (frame->phase != UP_HOOK_AFTER || frame->argc < 1 ||
        frame->args[0].type != UP_VAL_STR)
    {
        return 0;
    }
    bool empty = frame->result == nullptr ||
                 frame->result->type == UP_VAL_NIL ||
                 (frame->result->type == UP_VAL_OBJ &&
                  frame->result->as.obj == 0);
    if (!empty)
    {
        return 0;
    }
    const char* path = frame->args[0].str;
    uint64_t prefab = registry::virtual_prefab(path);
    if (prefab != 0)
    {
        registry::note_virtual_load(path);
        set_result_object(frame, prefab);
    }
    return 0;
}

static int32_t UP_CDECL on_resource_load_typed(up_hook_frame* frame)
{
    if (frame->phase != UP_HOOK_AFTER || frame->argc < 1 ||
        frame->args[0].type != UP_VAL_STR)
    {
        return 0;
    }
    bool empty = frame->result == nullptr ||
                 frame->result->type == UP_VAL_NIL ||
                 (frame->result->type == UP_VAL_OBJ &&
                  frame->result->as.obj == 0);
    if (!empty)
    {
        return 0;
    }
    uint64_t prefab = registry::virtual_prefab(frame->args[0].str);
    if (prefab != 0)
    {
        if (frame->argc >= 2 && frame->args[1].type == UP_VAL_OBJ)
        {
            std::string type_name;
            if (up::str_field(frame->args[1].as.obj, "FullName",
                              &type_name) &&
                type_name == "UnityEngine.Object")
            {
                registry::note_virtual_load(frame->args[0].str);
            }
        }
        set_result_object(frame, prefab);
    }
    return 0;
}

static int32_t UP_CDECL on_item_awake(up_hook_frame* frame)
{
    if (!before(frame))
    {
        return 0;
    }
    keep_original(frame);
    if (frame->instance == 0)
    {
        return 0;
    }
    std::string pending;
    std::string expected;
    if (!registry::take_pending_item_id(&pending, &expected))
    {
        return 0;
    }
    std::string current;
    if (!up::str_field(frame->instance, "id", &current))
    {
        return 0;
    }
    if (current == expected)
    {
        up::set_str(frame->instance, "id", pending.c_str());
    }
    return 0;
}

struct HookSpec {
    const char* type;
    const char* method;
    const char* params;
    uint32_t flags;
    up_hook_fn callback;
    const char* label;
};

static bool install(const HookSpec& spec)
{
    if (up::host == nullptr)
    {
        return false;
    }
    int32_t id = up::host->hook(up::host->user, spec.type, spec.method,
                                spec.params, spec.flags,
                                spec.callback, nullptr);
    if (id <= 0)
    {
        char buffer[256];
        buffer[0] = '\0';
        up::host->last_error(up::host->user, buffer,
                            static_cast<int32_t>(sizeof(buffer)));
        std::string message = "hook failed: ";
        message += spec.label;
        message += " (";
        message += buffer;
        message += ")";
        up::logw(message.c_str());
        return false;
    }
    return true;
}

bool install_all(up_host_api* host)
{
    up::host = host;
    const HookSpec specs[] = {
        {
            "MoodleManager, Assembly-CSharp", "AddAllMoodles", "",
            UP_HOOK_BEFORE, on_moodles, "moodles"
        },
        {
            "Antidepressants, Assembly-CSharp", "TakeDose", "System.Single",
            UP_HOOK_BEFORE, on_take_dose, "take dose"
        },
        {
            "WaterContainerItem, Assembly-CSharp", "Drink",
            "Body,System.Single,System.String",
            UP_HOOK_BEFORE, on_drink, "drink"
        },
        {
            "WaterContainerItem, Assembly-CSharp", "Inject",
            "Limb,System.Single",
            UP_HOOK_BEFORE, on_inject, "inject"
        },
        {
            "Body, Assembly-CSharp", "UseItem", "Item",
            UP_HOOK_BEFORE, on_use_item, "use item"
        },
        {
            "Body, Assembly-CSharp", "UseItemInHand", "",
            UP_HOOK_BEFORE, on_use_in_hand, "use in hand"
        },
        {
            "Body, Assembly-CSharp", "TakeANap", "",
            UP_HOOK_BEFORE, on_take_nap, "take nap"
        },
        {
            "Body, Assembly-CSharp", "DropItem", "System.Int32",
            UP_HOOK_BEFORE, on_drop_item, "drop item"
        },
        {
            "Body, Assembly-CSharp", "SwitchHands", "",
            UP_HOOK_BEFORE, on_switch_hands, "switch hands"
        },
        {
            "SpiderHandler, Assembly-CSharp", "DamageLimb", "Limb",
            UP_HOOK_BEFORE, on_spider_bite, "spider bite"
        },
        {
            "SpiderHandlerTBE, Assembly-CSharp", "DamageLimb", "Limb",
            UP_HOOK_BEFORE, on_spider_bite, "spider bite tbe"
        },
        {
            "GuardScript, Assembly-CSharp", "Die", "",
            UP_HOOK_BEFORE, on_guard_died, "guard died"
        },
        {
            "GuardScript, Assembly-CSharp", "Fire", "",
            UP_HOOK_BEFORE, on_guard_attack, "guard fire"
        },
        {
            "GuardScript, Assembly-CSharp", "MeleeAttack", "",
            UP_HOOK_BEFORE, on_guard_attack, "guard melee"
        },
        {
            "MineScript, Assembly-CSharp", "OnCollisionEnter2D",
            "Collision2D",
            UP_HOOK_BEFORE, on_mine_pressed, "mine press"
        },
        {
            "Body, Assembly-CSharp", "Scream", "",
            UP_HOOK_BEFORE, on_scream, "fall scream"
        },
        {
            "BushCol, Assembly-CSharp", "OnTriggerEnter2D", "Collider2D",
            UP_HOOK_BEFORE, on_bush_touched, "bush touch"
        },
        {
            "Item, Assembly-CSharp", "OnWillRenderObject", "",
            UP_HOOK_BEFORE, on_item_visible, "item visible"
        },
        {
            "BuildingEntity, Assembly-CSharp", "set_health",
            "System.Single",
            UP_HOOK_BEFORE, on_building_health, "building health"
        },
        {
            "UnityEngine.Resources, UnityEngine.CoreModule", "Load",
            "System.String",
            UP_HOOK_AFTER, on_resource_load, "resource load"
        },
        {
            "UnityEngine.Resources, UnityEngine.CoreModule", "Load",
            "System.String,System.Type",
            UP_HOOK_AFTER, on_resource_load_typed,
            "resource load typed"
        },
        {
            "Item, Assembly-CSharp", "Awake", "",
            UP_HOOK_BEFORE, on_item_awake, "item awake"
        }
    };
    bool ok = true;
    for (const HookSpec& spec : specs)
    {
        ok = install(spec) && ok;
    }
    up::logi(ok ? "Psychiatry: hooks installed"
                : "Psychiatry: some hooks failed");
    return ok;
}

}
