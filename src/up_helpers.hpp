#pragma once

#include "up_api.h"

#include <cstdint>
#include <string>

namespace up {

extern up_host_api* host;

void logi(const char* message);
void logw(const char* message);
void loge(const char* message);

up_value value_nil();
up_value value_bool(bool value);
up_value value_i32(int32_t value);
up_value value_f32(float value);
up_value value_str(const char* value);
up_value value_obj(uint64_t value);
up_value value_vec3(float x, float y, float z);
up_value value_vec4(float x, float y, float z, float w);

bool get_static(const char* type, const char* name, up_value* out);
bool get_field(uint64_t obj, const char* name, up_value* out);
bool set_field(uint64_t obj, const char* name, const up_value* value);
bool invoke(uint64_t obj, const char* method, const up_value* args,
            int32_t argc, up_value* result);
bool invoke_static(const char* type, const char* method,
                   const up_value* args, int32_t argc,
                   up_value* result);
void release(uint64_t obj);

uint64_t obj_static(const char* type, const char* name);
uint64_t obj_field(uint64_t obj, const char* name);
bool f32_field(uint64_t obj, const char* name, float* out);
bool i32_field(uint64_t obj, const char* name, int32_t* out);
bool bool_field(uint64_t obj, const char* name, bool* out);
bool str_field(uint64_t obj, const char* name, std::string* out);
bool set_f32(uint64_t obj, const char* name, float value);
bool set_i32(uint64_t obj, const char* name, int32_t value);
bool set_bool(uint64_t obj, const char* name, bool value);
bool set_str(uint64_t obj, const char* name, const char* value);
bool add_f32(uint64_t obj, const char* name, float delta);
uint64_t component(uint64_t obj, const char* type);
int32_t instance_id(uint64_t obj);
int32_t hash_code(uint64_t obj);
uint64_t invoke_obj0(uint64_t obj, const char* method);
int32_t list_count(uint64_t list);
uint64_t list_get(uint64_t list, int32_t index);
float list_get_f32(uint64_t list, int32_t index);
int32_t array_length(uint64_t array);
uint64_t array_get(uint64_t array, int32_t index);
bool list_add(uint64_t list, uint64_t item);
uint64_t dict_get(uint64_t dict, const char* key);
bool dict_has(uint64_t dict, const char* key);
bool dict_add(uint64_t dict, const char* key, uint64_t value);
bool dict_set_string(uint64_t dict, const char* key,
                     const char* value);
uint64_t clone_object(uint64_t obj);
uint64_t new_empty_like(uint64_t collection);
uint64_t cached_type(const char* name);

}
