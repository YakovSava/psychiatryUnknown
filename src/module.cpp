#include "hooks.hpp"
#include "psych.hpp"
#include "registry.hpp"
#include "up_helpers.hpp"

#include <cstdint>

namespace {

bool g_hooks_installed = false;
float g_registry_timer = 0.0f;

void UP_CDECL module_tick(void* user, float dt)
{
    (void)user;
    if (!g_hooks_installed)
    {
        g_hooks_installed = true;
        hooks::install_all(up::host);
    }
    g_registry_timer -= dt;
    if (g_registry_timer <= 0.0f)
    {
        g_registry_timer = 1.0f;
        registry::ensure();
    }
    psych::tick(dt);
}

int32_t UP_CDECL module_init(up_host_api* host, void** module_user)
{
    *module_user = nullptr;
    up::host = host;
    up::logi("Psychiatry: Unknown loaded");
    return 0;
}

void UP_CDECL module_shutdown(void* user)
{
    (void)user;
    up::logi("Psychiatry: Unknown unloaded");
}

}

extern "C" UP_EXPORT const up_module_desc* UP_CDECL
UP_GetModule(uint32_t host_abi_version)
{
    (void)host_abi_version;
    static const up_module_desc description = {
        UP_ABI_VERSION,
        static_cast<uint32_t>(sizeof(up_module_desc)),
        "psychiatry-unknown",
        "Psychiatry: Unknown",
        "0.2.0",
        "Iakov Savelev",
        "Expanded mood, trauma and brain damage with psychiatric "
        "conditions, drugs and phobias",
        module_init,
        module_tick,
        module_shutdown
    };
    return &description;
}
