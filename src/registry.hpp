#pragma once

#include <cstdint>
#include <string>

namespace registry {

bool ensure();

void note_virtual_load(const char* path);

bool take_pending_item_id(std::string* out, std::string* expected);

uint64_t virtual_prefab(const char* path);

}
