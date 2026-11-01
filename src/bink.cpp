#include <helper.hpp>
#include <game.h>
#include "component_loader.h"
#include "cevar.h"
#include <Hooking.Patterns.h>
#include "utils/hooking.h"

#include <filesystem>
#include <fstream>
#include "nlohmann/json.hpp"

#include <optional>
#include "GMath.h"
#include <buildnumber.h>
#include "framework.h"
cvar_s* mss_volume;
cevar_s* cinematic_reset_RoQPlayed;

FARPROC GetProcAddress(const char* modulename, const char* lpProcName) {
    auto module = GetModuleHandleA(modulename);
    if (module) {
        return GetProcAddress(module, lpProcName);
    }
    return NULL;
}
uintptr_t BinkSetVolumePtr;
namespace bink_game {

    SafetyHookInline BinkDoFrameog;
     void __stdcall BinkDoFrame_hook(void* bnk) {
        if (bnk && BinkSetVolumePtr && mss_volume) {
            int volume = (int)(32768.0f * std::clamp(mss_volume->value, 0.f, 1.f));
            stdcall_call<void>(BinkSetVolumePtr, bnk, 0u, volume);
        }

         BinkDoFrameog.unsafe_stdcall(bnk);
    }

    class component final : public component_interface
    {
    public:
        void post_unpack() override
        {
            auto BinkDoFrameptr = GetProcAddress("binkw32.dll","_BinkDoFrame@4");
            BinkSetVolumePtr = (uintptr_t)GetProcAddress("binkw32.dll", "_BinkSetVolume@12");

            mss_volume = Cvar_Find("mss_volume");

            cinematic_reset_RoQPlayed = Cevar_Get("cinematic_reset_RoQPlayed", 1, CVAR_ARCHIVE, 0, 1);

            if (BinkDoFrameptr && BinkSetVolumePtr && mss_volume) {
                BinkDoFrameog = safetyhook::create_inline(BinkDoFrameptr, BinkDoFrame_hook);
            }

            auto pattern = hook::pattern("88 99 ? ? ? ? C7 05");

            if (!pattern.empty()) {
                static auto roq_test_fix1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& ctx) {

                    const uintptr_t stride = sp_mp(0x1D8, 0x1D0);
                    const uintptr_t tables = exe(0x877558, 0x8CB4F8);
                    if (Cevar_Int(cinematic_reset_RoQPlayed) != 0 && ctx.ecx < stride * 16 && ctx.ecx % stride == 0) {
                        Memory::VP::Patch<int>((int*)(tables + ctx.ecx + offsetof(cin_cache, RoQPlayed)), 0);
                    }
                    

                    });
            }

        }
    };
}
REGISTER_COMPONENT(bink_game::component);