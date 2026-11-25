#include "registry.hpp"

#include "up_helpers.hpp"

#include <cstring>
#include <string>

namespace registry {

using namespace up;

struct Ingredient {
    const char* id;
    bool liquid;
    float amount;
};

struct DrugSpec {
    const char* id;
    const char* name;
    const char* description;
    float red;
    float green;
    float blue;
    int32_t int_requirement;
    float result_ml;
};

static const DrugSpec kDrugs[] = {
    {
        "psy_valproate",
        "Valproic Acid",
        "A mood stabiliser. Smooths the violent highs and lows of a "
        "damaged mind and blunts antidepressant side effects.",
        0.92f, 0.90f, 0.60f,
        8,
        100.0f
    },
    {
        "psy_quetiapine",
        "Quetiapine",
        "An atypical antipsychotic. Dulls hallucinations, aggression, "
        "panic and hyperactivity.",
        1.00f, 0.70f, 0.55f,
        10,
        100.0f
    },
    {
        "psy_benzo",
        "Benzodiazepine",
        "A fast anxiolytic. Erases fear and terror at once, but it "
        "builds dependence and withdrawal.",
        0.75f, 0.90f, 1.00f,
        9,
        100.0f
    },
    {
        "psy_haloperidol",
        "Haloperidol",
        "An old, harsh antipsychotic. Silences psychosis at the cost "
        "of tremors, stiffness and flatness.",
        0.70f, 0.95f, 0.80f,
        12,
        100.0f
    }
};

static const Ingredient kValproateRecipe[] = {
    { "antidepressants", true, 100.0f },
    { "biochem", true, 20.0f },
    { "flammablepowder", false, 0.0f }
};

static const Ingredient kQuetiapineRecipe[] = {
    { "antidepressants", true, 100.0f },
    { "soothingcrystalshard", false, 0.0f },
    { "flammablepowder", false, 0.0f }
};

static const Ingredient kBenzoRecipe[] = {
    { "antidepressants", true, 100.0f },
    { "biochem", true, 50.0f },
    { "water", true, 50.0f }
};

static const Ingredient kHaloperidolRecipe[] = {
    { "antidepressants", true, 100.0f },
    { "biochem", true, 30.0f },
    { "soothingcrystalshard", false, 0.0f }
};

static const Ingredient kNeedleRecipe[] = {
    { "scraptube", false, 0.0f },
    { "scrapmetal", false, 0.0f },
    { "scrapmetal", false, 0.0f }
};

static const char* const kDrugIds[] = {
    "psy_valproate",
    "psy_quetiapine",
    "psy_benzo",
    "psy_haloperidol"
};

static std::string g_pending_id;
static std::string g_pending_base;
static uint64_t g_prefab = 0;
static int32_t g_locale_instance = 0;

static uint64_t liquid_type(uint64_t registry, const char* id,
                            float red, float green, float blue)
{
    if (dict_has(registry, id))
    {
        return dict_get(registry, id);
    }
    uint64_t water = dict_get(registry, "water");
    if (water == 0)
    {
        return 0;
    }
    uint64_t created = clone_object(water);
    release(water);
    if (created == 0)
    {
        return 0;
    }
    set_str(created, "localeName", id);
    up_value color = up::value_vec4(red, green, blue, 1.0f);
    set_field(created, "color", &color);
    set_f32(created, "valuePerLiter", 120.0f);
    uint64_t shared = obj_field(created, "qualities");
    if (shared != 0)
    {
        uint64_t empty = new_empty_like(shared);
        if (empty != 0)
        {
            up_value value = up::value_obj(empty);
            set_field(created, "qualities", &value);
            release(empty);
        }
        release(shared);
    }
    dict_add(registry, id, created);
    return created;
}

static uint64_t find_recipe(uint64_t recipes, const char* result_id)
{
    int32_t count = list_count(recipes);
    for (int32_t i = 0; i < count; i++)
    {
        uint64_t recipe = list_get(recipes, i);
        if (recipe == 0)
        {
            continue;
        }
        uint64_t result = obj_field(recipe, "result");
        std::string id;
        bool match = result != 0 &&
                     str_field(result, "id", &id) &&
                     id == result_id;
        release(result);
        if (match)
        {
            return recipe;
        }
        release(recipe);
    }
    return 0;
}

static bool set_ingredient(uint64_t item, const Ingredient& spec,
                           const char* result_id)
{
    set_bool(item, "specific", true);
    set_str(item, "specificId", spec.id);
    set_bool(item, "isLiquid", spec.liquid);
    set_f32(item, "minimumCondition", spec.amount);
    set_bool(item, "destroyItem", true);
    set_str(item, "ignoredId", result_id);
    return true;
}

static bool add_recipe(uint64_t recipes, uint64_t base,
                       const char* result_id, bool liquid,
                       float result_condition, int32_t int_requirement,
                       const Ingredient* ingredients, int32_t count,
                       const int32_t* sources)
{
    uint64_t base_items = obj_field(base, "items");
    uint64_t base_result = obj_field(base, "result");
    if (base_items == 0 || base_result == 0)
    {
        release(base_items);
        release(base_result);
        return false;
    }
    uint64_t recipe = clone_object(base);
    uint64_t result = clone_object(base_result);
    uint64_t items = new_empty_like(base_items);
    bool good = recipe != 0 && result != 0 && items != 0;
    if (good)
    {
        set_str(result, "id", result_id);
        set_bool(result, "isLiquid", liquid);
        set_f32(result, "resultCondition", result_condition);
        up_value result_value = up::value_obj(result);
        set_field(recipe, "result", &result_value);
        for (int32_t i = 0; i < count; i++)
        {
            uint64_t source = list_get(base_items, sources[i]);
            uint64_t copy = source != 0 ? clone_object(source) : 0;
            release(source);
            if (copy == 0)
            {
                good = false;
                break;
            }
            set_ingredient(copy, ingredients[i], result_id);
            list_add(items, copy);
            release(copy);
        }
        if (good)
        {
            up_value items_value = up::value_obj(items);
            set_field(recipe, "items", &items_value);
            set_i32(recipe, "INT", int_requirement);
            set_i32(recipe, "category", 2);
            list_add(recipes, recipe);
            set_i32(recipe, "index", list_count(recipes) - 1);
        }
    }
    release(base_items);
    release(base_result);
    release(recipe);
    release(result);
    release(items);
    return good;
}

static void register_drugs()
{
    uint64_t registry = obj_static("Liquids", "Registry");
    if (registry == 0)
    {
        return;
    }
    for (const DrugSpec& spec : kDrugs)
    {
        uint64_t created = liquid_type(registry, spec.id, spec.red,
                                       spec.green, spec.blue);
        release(created);
    }
    release(registry);
}

static void register_item()
{
    uint64_t items = obj_static("Item", "GlobalItems");
    if (items == 0)
    {
        return;
    }
    if (!dict_has(items, "psy_needle"))
    {
        uint64_t tweezers = dict_get(items, "tweezers");
        uint64_t needle = tweezers != 0 ? clone_object(tweezers) : 0;
        release(tweezers);
        if (needle != 0)
        {
            set_str(needle, "fullName", "Lobotomy Needle");
            set_str(needle, "description",
                    "A thin needle meant to part the lobes of a brain. "
                    "Absurd to use on yourself, yet the pain ends.");
            set_bool(needle, "usable", true);
            set_bool(needle, "usableWithLMB", true);
            set_bool(needle, "usableOnLimb", false);
            set_f32(needle, "weight", 0.1f);
            set_i32(needle, "value", 10);
            dict_add(items, "psy_needle", needle);
            release(needle);
        }
    }
    release(items);
}

static void register_recipes()
{
    uint64_t recipes = obj_static("Recipes", "recipes");
    if (recipes == 0)
    {
        return;
    }
    uint64_t base = find_recipe(recipes, "antidepressants");
    if (base == 0)
    {
        release(recipes);
        return;
    }
    const int32_t sources[3] = { 0, 1, 2 };
    const int32_t solid_sources[3] = { 0, 2, 0 };
    const Ingredient* specs[4] = {
        kValproateRecipe,
        kQuetiapineRecipe,
        kBenzoRecipe,
        kHaloperidolRecipe
    };
    for (int32_t i = 0; i < 4; i++)
    {
        std::string result_id = kDrugs[i].id;
        add_recipe(recipes, base, result_id.c_str(), true,
                   kDrugs[i].result_ml, kDrugs[i].int_requirement,
                   specs[i], 3, sources);
    }
    add_recipe(recipes, base, "psy_needle", false, 1.0f, 14,
               kNeedleRecipe, 3, solid_sources);
    release(base);
    release(recipes);
}

static void register_locale()
{
    up_value probe = up::value_str("water");
    invoke_static("Locale", "GetOther", &probe, 1, nullptr);
    uint64_t language = obj_static("Locale", "currentLang");
    if (language == 0)
    {
        return;
    }
    int32_t id = hash_code(language);
    uint64_t main = obj_field(language, "main");
    uint64_t other = obj_field(language, "other");
    if (main != 0)
    {
        dict_set_string(main, "psy_needle", "Lobotomy Needle");
        dict_set_string(
            main, "psy_needledsc",
            "A thin needle meant to part the lobes of a brain. "
            "Absurd to use on yourself, yet the pain ends.");
    }
    if (other != 0)
    {
        for (const DrugSpec& spec : kDrugs)
        {
            dict_set_string(other, spec.id, spec.name);
            std::string key = std::string(spec.id) + "dsc";
            dict_set_string(other, key.c_str(), spec.description);
        }
    }
    g_locale_instance = id;
    release(main);
    release(other);
    release(language);
}

static void refresh_locale()
{
    uint64_t language = obj_static("Locale", "currentLang");
    if (language == 0)
    {
        return;
    }
    int32_t id = hash_code(language);
    release(language);
    if (id != 0 && id != g_locale_instance)
    {
        register_locale();
    }
}

bool ensure()
{
    uint64_t items = obj_static("Item", "GlobalItems");

    if (items == 0)
    {
        return false;
    }
    bool ready = dict_has(items, "antidepressants") &&
                 dict_has(items, "craftingbottle") &&
                 dict_has(items, "tweezers");
    bool needle = dict_has(items, "psy_needle");
    release(items);
    if (!ready)
    {
        return false;
    }
    if (!needle)
    {
        register_drugs();
        register_item();
        register_recipes();
        register_locale();
        up::logi("Psychiatry: registry ready");
    }
    refresh_locale();
    return true;
}

void note_virtual_load(const char* path)
{
    if (path == nullptr)
    {
        return;
    }
    if (std::strcmp(path, "psy_needle") == 0)
    {
        g_pending_id = path;
        g_pending_base = "tweezers";
    }
}

bool take_pending_item_id(std::string* out, std::string* expected)
{
    if (g_pending_id.empty())
    {
        return false;
    }
    *out = g_pending_id;
    *expected = g_pending_base;
    g_pending_id.clear();
    g_pending_base.clear();
    return true;
}

uint64_t virtual_prefab(const char* path)
{
    if (path == nullptr || std::strcmp(path, "psy_needle") != 0)
    {
        return 0;
    }
    if (g_prefab != 0)
    {
        return g_prefab;
    }
    up_value arg = up::value_str("tweezers");
    up_value result;
    std::memset(&result, 0, sizeof(result));
    if (!invoke_static("UnityEngine.Resources", "Load", &arg, 1,
                       &result))
    {
        return 0;
    }
    uint64_t handle = result.type == UP_VAL_OBJ ? result.as.obj : 0;
    if (handle != 0 && up::host != nullptr)
    {
        up::host->retain(up::host->user, handle);
        up::host->release(up::host->user, handle);
    }
    g_prefab = handle;
    return g_prefab;
}

}
