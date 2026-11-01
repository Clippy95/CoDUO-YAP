#include <helper.hpp>
#include "hook_trace.h"
#include <game.h>
#include "component_loader.h"
#include "utils/hooking.h"
#include "framework.h"

namespace render_guard {

    // the static-model batcher appends to a fixed index array without any capacity check
    constexpr int kMaxIndices = 0x40000 / sizeof(uint16_t);
    uintptr_t kIndexCount = 0;
    uintptr_t kVertexRange = 0;
    uintptr_t kFlush = 0;

    SafetyHookInline surface_og;

    void FlushBatch()
    {
        reinterpret_cast<void(__cdecl*)()>(kFlush)();
        *(int*)(kVertexRange) = 0;
        *(int*)(kVertexRange + 4) = 0;
    }

    void __cdecl surface_hook(const uint8_t* batch)
    {
        const uint8_t* surf = batch ? *(const uint8_t* const*)(batch + 4) : nullptr;

        if (surf) {
            const int add = *(const uint16_t*)(surf + 0x10);
            int used = *(const int*)kIndexCount;

            if (used < 0 || used > kMaxIndices || add > kMaxIndices) {
                static bool warned = false;
                if (!warned) {
                    warned = true;
                    YAP_LOG("[" MOD_NAME "] invalid static batch state (%d + %d indices), surface skipped.\n", used, add);
                }
                return;
            }

            if (add > kMaxIndices - used)
                FlushBatch();
        }

        surface_og.unsafe_ccall<void>(batch);
    }

    class component final : public component_interface
    {
    public:
        void post_unpack() override
        {
            const uintptr_t target = exe(0x004FFDA0, 0x0051A190);
            kIndexCount = exe(0x046D9854, 0x04884D94);
            kVertexRange = exe(0x0469984C, 0x04844D8C);
            kFlush = exe(0x004FCD70, 0x004EEAD0);
            if (!target || !kIndexCount || !kVertexRange || !kFlush)
                return;

            if (*(const uint8_t*)kFlush != 0x55)
                return;

            if (*(const uint8_t*)target != 0xA1 || !Memory::VP::MemEquals(target + 5, { 0x8B, 0x48, 0x20 }))
                return;

            surface_og = safetyhook::create_inline(target, surface_hook);
        }
    };
}
REGISTER_COMPONENT(render_guard::component);
