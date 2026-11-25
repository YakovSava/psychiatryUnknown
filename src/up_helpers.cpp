#include "up_helpers.hpp"

#include <cstring>
#include <map>
#include <string>

namespace up {

up_host_api* host = nullptr;

static void log_line(int32_t level, const char* message)
{
    if (host != nullptr && host->log != nullptr)
    {
        host->log(host->user, level, message);
    }
}

void logi(const char* message)
{
    log_line(UP_LOG_INFO, message);
}

void logw(const char* message)
{
    log_line(UP_LOG_WARN, message);
}

void loge(const char* message)
{
    log_line(UP_LOG_ERROR, message);
}

static up_value make_value(uint32_t type)
{
    up_value value;
    std::memset(&value, 0, sizeof(value));
    value.type = type;
    value.size = static_cast<uint32_t>(sizeof(up_value));
    return value;
}

up_value value_nil()
{
    return make_value(UP_VAL_NIL);
}

up_value value_bool(bool value)
{
    up_value result = make_value(UP_VAL_BOOL);
    result.as.i32 = value ? 1 : 0;
    return result;
}

up_value value_i32(int32_t value)
{
    up_value result = make_value(UP_VAL_I32);
    result.as.i32 = value;
    return result;
}

up_value value_f32(float value)
{
    up_value result = make_value(UP_VAL_F32);
    result.as.f32 = value;
    return result;
}

up_value value_str(const char* value)
{
    up_value result = make_value(UP_VAL_STR);
    result.str = value;
    return result;
}

up_value value_obj(uint64_t value)
{
    up_value result = make_value(UP_VAL_OBJ);
    result.as.obj = value;
    return result;
}

up_value value_vec3(float x, float y, float z)
{
    up_value result = make_value(UP_VAL_VEC3);
    result.as.vec3.x = x;
    result.as.vec3.y = y;
    result.as.vec3.z = z;
    return result;
}

up_value value_vec4(float x, float y, float z, float w)
{
    up_value result = make_value(UP_VAL_VEC4);
    result.as.vec4.x = x;
    result.as.vec4.y = y;
    result.as.vec4.z = z;
    result.as.vec4.w = w;
    return result;
}

static const char* const kAssemblySuffixes[] = {
    ", Assembly-CSharp",
    ", UnityEngine.CoreModule",
    ", UnityEngine",
    ", mscorlib"
};

static bool has_assembly(const char* type)
{
    return std::strchr(type, ',') != nullptr;
}

template <typename Call>
static bool with_suffixes(const char* type, Call call)
{
    if (call(type))
    {
        return true;
    }
    if (has_assembly(type))
    {
        return false;
    }
    for (const char* suffix : kAssemblySuffixes)
    {
        std::string qualified = std::string(type) + suffix;
        if (call(qualified.c_str()))
        {
            return true;
        }
    }
    return false;
}

bool get_static(const char* type, const char* name, up_value* out)
{
    if (host == nullptr)
    {
        return false;
    }
    return with_suffixes(type, [&](const char* candidate) {
        return host->get_static_field(host->user, candidate, name,
                                      out) == 0;
    });
}

bool get_field(uint64_t obj, const char* name, up_value* out)
{
    if (host == nullptr || obj == 0)
    {
        return false;
    }
    return host->get_field(host->user, obj, name, out) == 0;
}

bool set_field(uint64_t obj, const char* name, const up_value* value)
{
    if (host == nullptr || obj == 0)
    {
        return false;
    }
    return host->set_field(host->user, obj, name, value) == 0;
}

bool invoke(uint64_t obj, const char* method, const up_value* args,
            int32_t argc, up_value* result)
{
    if (host == nullptr || obj == 0)
    {
        return false;
    }
    return host->invoke(host->user, obj, method, args, argc, result) == 0;
}

bool invoke_static(const char* type, const char* method,
                   const up_value* args, int32_t argc,
                   up_value* result)
{
    if (host == nullptr)
    {
        return false;
    }
    return with_suffixes(type, [&](const char* candidate) {
        return host->invoke_static(host->user, candidate, method, args,
                                   argc, result) == 0;
    });
}

void release(uint64_t obj)
{
    if (host != nullptr && obj != 0)
    {
        host->release(host->user, obj);
    }
}

uint64_t obj_static(const char* type, const char* name)
{
    up_value result;
    std::memset(&result, 0, sizeof(result));
    if (!get_static(type, name, &result))
    {
        return 0;
    }
    return result.type == UP_VAL_OBJ ? result.as.obj : 0;
}

uint64_t obj_field(uint64_t obj, const char* name)
{
    up_value result;
    std::memset(&result, 0, sizeof(result));
    if (!get_field(obj, name, &result))
    {
        return 0;
    }
    return result.type == UP_VAL_OBJ ? result.as.obj : 0;
}

bool f32_field(uint64_t obj, const char* name, float* out)
{
    up_value result;
    std::memset(&result, 0, sizeof(result));
    if (!get_field(obj, name, &result) || result.type != UP_VAL_F32)
    {
        return false;
    }
    *out = result.as.f32;
    return true;
}

bool i32_field(uint64_t obj, const char* name, int32_t* out)
{
    up_value result;
    std::memset(&result, 0, sizeof(result));
    if (!get_field(obj, name, &result) || result.type != UP_VAL_I32)
    {
        return false;
    }
    *out = result.as.i32;
    return true;
}

bool bool_field(uint64_t obj, const char* name, bool* out)
{
    up_value result;
    std::memset(&result, 0, sizeof(result));
    if (!get_field(obj, name, &result) || result.type != UP_VAL_BOOL)
    {
        return false;
    }
    *out = result.as.i32 != 0;
    return true;
}

bool str_field(uint64_t obj, const char* name, std::string* out)
{
    up_value result;
    std::memset(&result, 0, sizeof(result));
    if (!get_field(obj, name, &result) || result.type != UP_VAL_STR)
    {
        return false;
    }
    *out = result.str != nullptr ? result.str : "";
    return true;
}

bool set_f32(uint64_t obj, const char* name, float value)
{
    up_value converted = value_f32(value);
    return set_field(obj, name, &converted);
}

bool set_i32(uint64_t obj, const char* name, int32_t value)
{
    up_value converted = value_i32(value);
    return set_field(obj, name, &converted);
}

bool set_bool(uint64_t obj, const char* name, bool value)
{
    up_value converted = value_bool(value);
    return set_field(obj, name, &converted);
}

bool set_str(uint64_t obj, const char* name, const char* value)
{
    up_value converted = value_str(value);
    return set_field(obj, name, &converted);
}

bool add_f32(uint64_t obj, const char* name, float delta)
{
    float current = 0.0f;
    if (!f32_field(obj, name, &current))
    {
        return false;
    }
    return set_f32(obj, name, current + delta);
}

uint64_t component(uint64_t obj, const char* type)
{
    if (host == nullptr || obj == 0 || host->get_component == nullptr)
    {
        return 0;
    }
    uint64_t result = 0;
    bool found = with_suffixes(type, [&](const char* candidate) {
        return host->get_component(host->user, obj, candidate,
                                   &result) == 0;
    });
    return found ? result : 0;
}

int32_t instance_id(uint64_t obj)
{
    up_value result;
    std::memset(&result, 0, sizeof(result));
    if (!invoke(obj, "GetInstanceID", nullptr, 0, &result))
    {
        return 0;
    }
    return result.type == UP_VAL_I32 ? result.as.i32 : 0;
}

int32_t hash_code(uint64_t obj)
{
    up_value result;
    std::memset(&result, 0, sizeof(result));
    if (!invoke(obj, "GetHashCode", nullptr, 0, &result))
    {
        return 0;
    }
    return result.type == UP_VAL_I32 ? result.as.i32 : 0;
}

uint64_t invoke_obj0(uint64_t obj, const char* method)
{
    up_value result;
    std::memset(&result, 0, sizeof(result));
    if (!invoke(obj, method, nullptr, 0, &result))
    {
        return 0;
    }
    return result.type == UP_VAL_OBJ ? result.as.obj : 0;
}

static int32_t int_method(uint64_t obj, const char* method,
                          const up_value* args, int32_t argc)
{
    up_value result;
    std::memset(&result, 0, sizeof(result));
    if (!invoke(obj, method, args, argc, &result))
    {
        return 0;
    }
    return result.type == UP_VAL_I32 ? result.as.i32 : 0;
}

int32_t list_count(uint64_t list)
{
    return int_method(list, "get_Count", nullptr, 0);
}

uint64_t list_get(uint64_t list, int32_t index)
{
    up_value arg = value_i32(index);
    up_value result;
    std::memset(&result, 0, sizeof(result));
    if (!invoke(list, "get_Item", &arg, 1, &result))
    {
        return 0;
    }
    return result.type == UP_VAL_OBJ ? result.as.obj : 0;
}

int32_t array_length(uint64_t array)
{
    return int_method(array, "get_Length", nullptr, 0);
}

uint64_t array_get(uint64_t array, int32_t index)
{
    up_value arg = value_i32(index);
    up_value result;
    std::memset(&result, 0, sizeof(result));
    if (!invoke(array, "GetValue", &arg, 1, &result))
    {
        return 0;
    }
    return result.type == UP_VAL_OBJ ? result.as.obj : 0;
}

float list_get_f32(uint64_t list, int32_t index)
{
    up_value arg = value_i32(index);
    up_value result;
    std::memset(&result, 0, sizeof(result));
    if (!invoke(list, "get_Item", &arg, 1, &result))
    {
        return 0.0f;
    }
    return result.type == UP_VAL_F32 ? result.as.f32 : 0.0f;
}

bool list_add(uint64_t list, uint64_t item)
{
    up_value arg = value_obj(item);
    return invoke(list, "Add", &arg, 1, nullptr);
}

uint64_t dict_get(uint64_t dict, const char* key)
{
    up_value arg = value_str(key);
    up_value result;
    std::memset(&result, 0, sizeof(result));
    if (!invoke(dict, "get_Item", &arg, 1, &result))
    {
        return 0;
    }
    return result.type == UP_VAL_OBJ ? result.as.obj : 0;
}

bool dict_has(uint64_t dict, const char* key)
{
    up_value arg = value_str(key);
    up_value result;
    std::memset(&result, 0, sizeof(result));
    if (!invoke(dict, "ContainsKey", &arg, 1, &result))
    {
        return false;
    }
    return result.type == UP_VAL_BOOL && result.as.i32 != 0;
}

bool dict_add(uint64_t dict, const char* key, uint64_t value)
{
    up_value args[2];
    args[0] = value_str(key);
    args[1] = value_obj(value);
    return invoke(dict, "Add", args, 2, nullptr);
}

bool dict_set_string(uint64_t dict, const char* key, const char* value)
{
    if (dict_has(dict, key))
    {
        return true;
    }
    up_value args[2];
    args[0] = value_str(key);
    args[1] = value_str(value);
    return invoke(dict, "Add", args, 2, nullptr);
}

uint64_t clone_object(uint64_t obj)
{
    return invoke_obj0(obj, "MemberwiseClone");
}

uint64_t new_empty_like(uint64_t collection)
{
    uint64_t type = invoke_obj0(collection, "GetType");
    if (type == 0)
    {
        return 0;
    }
    up_value arg = value_obj(type);
    up_value result;
    std::memset(&result, 0, sizeof(result));
    invoke_static("System.Activator", "CreateInstance", &arg, 1,
                  &result);
    release(type);
    return result.type == UP_VAL_OBJ ? result.as.obj : 0;
}

static std::map<std::string, uint64_t> g_types;

uint64_t cached_type(const char* name)
{
    auto found = g_types.find(name);
    if (found != g_types.end())
    {
        return found->second;
    }
    std::string qualified = std::string(name) + ", Assembly-CSharp";
    up_value arg = value_str(qualified.c_str());
    up_value result;
    std::memset(&result, 0, sizeof(result));
    uint64_t handle = 0;
    if (invoke_static("System.Type", "GetType", &arg, 1, &result) &&
        result.type == UP_VAL_OBJ)
    {
        handle = result.as.obj;
    }
    g_types[name] = handle;
    return handle;
}

}
