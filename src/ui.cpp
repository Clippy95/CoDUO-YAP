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
#include "utils/common.h"
bool GetGameScreenRes(vector2& res);
double process_width(double width);
double process_widths(double width); 

typedef int(__stdcall* glClearColorT)(float r, float g, float b, float a);

typedef int(__stdcall* glClearT)(uint32_t bit);

glClearT *glClear = (glClearT*)0x047BD87C;
glClearColorT* glClearColor = (glClearColorT*)0x047BDA10;

namespace gui {
    cevar_s* branding = nullptr;
    cevar_s* cg_ammo_overwrite_size_enabled = nullptr;
    cevar_s* cg_ammo_overwrite_size = nullptr;
    cevar_s* r_fixedaspect_clear = nullptr;
    cevar_s* cg_subtitle_centered_enable = nullptr;
    cevar_s* cg_subtitle_centered_y = nullptr;
    cevar_s* cg_subtitle_centered_font = nullptr;
    cevar_s* cg_subtitle_centered_scale = nullptr;
    cevar_s* cg_subtitle_centered_spacing_multiplier = nullptr;
    cevar_s* cg_subtitle_centered_ignore_hook = nullptr;
    cevar_s* cg_DrawPlayerStance_disable = nullptr;
    bool mp_loading_state(int state) {
        return !sp_mp(1) && (state == 4 || state == 5);
    }

	void draw_branding() {
        if (!branding || !branding->base || !branding->base->integer)
            return;
        if (game::cstate && mp_loading_state(*game::cstate))
            return;

        auto x = 2.f - (float)process_width(0) * 0.5f;
        auto y = 8.f;
        auto fontID = 1;
        const auto scale = 0.16f;
        float color[4] = { 1.f, 1.f, 1.f, 0.50f * 0.7f };
        float color_shadow[4] = { 0.f, 0.f, 0.f, 0.80f * 0.7f };
        auto text = MOD_NAME " r" BUILD_NUMBER_STR;
        if (branding->base->integer != 2) {
            game::SCR_DrawString(x + 1, y + 1, fontID, scale, color_shadow, text, NULL, NULL, NULL);
        }
        game::SCR_DrawString(x, y, fontID, scale, color, text, NULL, NULL, NULL);
	}
    SafetyHookInline RE_EndFrameD;
    int __cdecl RE_EndFrame_hook(DWORD* a1, DWORD* a2) {
        draw_branding();
        return RE_EndFrameD.unsafe_ccall<int>(a1, a2);
    }

    void* VM_call_og;

    bool in_game_state(int state) {
        return sp_mp(1) ? state == 2 : (state >= 4 && state <= 6);
    }

    void __stdcall DrawBlack() {
        float black[] = {0.f,0.f,0.f,1.f};
        game::SetColor(black);
        vector2 res;
        GetGameScreenRes(res);
        game::drawStretchPic(-process_widths(0), 0.f, res.x + process_widths(0), res.y, 0.f, 0.f, 0.f, 0.f,*game::whiteShader);
    }
    static bool is_subtitle_draw = false;
    uintptr_t Con_DrawSubtitles_og;
    int __cdecl Con_DrawSubtitles(int a1, float a2, int a3) {
        if (Cevar_Int(cg_subtitle_centered_enable))
        is_subtitle_draw = true;
        auto result = cdecl_call<int>(Con_DrawSubtitles_og, a1, a2, a3);
        is_subtitle_draw = false;
        return result;
    }
    uintptr_t cg_DrawPlayerStance_ptr;
    int __cdecl CG_DrawPlayerStance_hook(float* a1, int* a2, int a3) {
        if (Cevar_Int(cg_DrawPlayerStance_disable) >= 2)
            return NULL;

        return cdecl_call<int>(cg_DrawPlayerStance_ptr, a1, a2, a3);
    }

    class component final : public component_interface
    {
    public:
        void post_unpack() override
        {
            branding = Cevar_Get("branding", 1, CVAR_ARCHIVE, 0, 2);
            auto pattern = hook::pattern("A1 ? ? ? ? 57 33 FF 3B C7 0F 84 ? ? ? ? A1");
            if (!pattern.empty()) {
                RE_EndFrameD = safetyhook::create_inline(pattern.get_first(), RE_EndFrame_hook);
            }
            cg_ammo_overwrite_size = Cevar_Get("cg_ammo_overwrite_size", 0.3f,CVAR_ARCHIVE);
            cg_ammo_overwrite_size_enabled = Cevar_Get("cg_ammo_overwrite_size_enabled", 1, CVAR_ARCHIVE);

            cg_DrawPlayerStance_disable = Cevar_Get("cg_DrawPlayerStance_disable", 0, CVAR_ARCHIVE, 0, 2);

            if (sp_mp(0, 1))
                cg_subtitle_centered_ignore_hook = Cevar_Get("cg_subtitle_centered_ignore_hook", 0, CVAR_ARCHIVE,0,1);

            if (!cg_subtitle_centered_ignore_hook || Cevar_Int(cg_subtitle_centered_ignore_hook) == 0) {
                cg_subtitle_centered_enable = Cevar_Get("cg_subtitle_centered_enable", 1, CVAR_ARCHIVE);
                cg_subtitle_centered_font = Cevar_Get("cg_subtitle_centered_font", 0, CVAR_ARCHIVE);

                cg_subtitle_centered_scale = Cevar_Get("cg_subtitle_centered_scale", 0.25f, CVAR_ARCHIVE);

                cg_subtitle_centered_y = Cevar_Get("cg_subtitle_centered_y", 0x1A9, CVAR_ARCHIVE);

                cg_subtitle_centered_spacing_multiplier = Cevar_Get("cg_subtitle_centered_spacing_multiplier", 1.f, CVAR_ARCHIVE);

                Memory::VP::Nop(exe(0x00409BB5, 0x0040A7C5), 8);
                Memory::VP::InterceptCall(exe(0x0040234A, 0x0040270C), Con_DrawSubtitles_og, Con_DrawSubtitles);
                static auto mode = safetyhook::create_mid(exe(0x0040A11E, 0x0040AF8E), [](SafetyHookContext& ctx) {
                    if (!is_subtitle_draw)
                        return;
                    ctx.ecx = 3;
                    *(int*)ctx.esp = 320;
                    ctx.eax = Cevar_Int(cg_subtitle_centered_y, 0x1A9);
                    });

                static auto drawstringonhud = safetyhook::create_mid(exe(0x00409BB3, 0x0040A7C3), [](SafetyHookContext& ctx) {
                    if (!is_subtitle_draw) {
                        *(float*)((ctx.esp + 0x10)) = 0.3333333433f;
                        return;
                    }
                    ctx.esi = Cevar_Int(cg_subtitle_centered_font);
                    float scale = Cevar_Value(cg_subtitle_centered_scale, 0.25f);
                    *(float*)ctx.esp = scale;

                    *(float*)ctx.esp = scale;

                    *(float*)(ctx.esp + 0x10) = scale;

                    });


                static auto oi = safetyhook::create_mid(exe(0x409C43, 0x0040A859), [](SafetyHookContext& ctx) {
                    if (!is_subtitle_draw)
                        return;
                    float eax_float = ctx.eax;

                    eax_float *= Cevar_Value(cg_subtitle_centered_spacing_multiplier, 1.f);
                    ctx.eax = (int)eax_float;

                    });
            }
                pattern = hook::pattern("FF 15 ? ? ? ? 6A 00 FF 15 ? ? ? ? 83 C4 ? A1");
                if (!pattern.empty()) {
                    Memory::VP::Nop(pattern.get_first(), 6);
                }
            
            pattern = hook::pattern("8B 0D ? ? ? ? 8B 41 ? 85 C0 74 ? ? ? ? ? ? ? 8B 15");
            auto glclearpat = hook::pattern("FF 15 ? ? ? ? F6 05 ? ? ? ? ? 5E");
            auto glcolorpat = hook::pattern("FF 15 ? ? ? ? 68 ? ? ? ? FF 15 ? ? ? ? C7 05");

            if (!glcolorpat.empty() && !glclearpat.empty() && !pattern.empty()) {
                r_fixedaspect_clear = Cevar_Get("r_fixedaspect_clear", 2, CVAR_ARCHIVE, 0, 2);
                static auto yeah = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& ctx) {
                    const int clearMode = Cevar_Int(r_fixedaspect_clear);
                    if (!clearMode)
                        return;
                    const int state = *game::cstate;
                    static int last_state = -1;
                    const bool changed = state != last_state;
                    last_state = state;

                    bool clear;
                    if (mp_loading_state(state))
                        clear = changed;
                    else if (!sp_mp(1) && changed && state == 6)
                        clear = true;
                    else
                        clear = clearMode == 1 || (!in_game_state(state) || *game::keycatchers & KEYCATCH_UI);

                    if (clear) {
                        auto qglClearColor = *glClearColor;
                        qglClearColor(0.f, 0.f, 0.f, 1.f);

                        auto qglClear = *glClear;
                        qglClear(16640); // GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT
                    }
                    });

                glClearColor = *glcolorpat.get_first<glClearColorT*>(2);
                glClear = *glclearpat.get_first<glClearT*>(2);

            }
        }

        void post_ui() override {
            // Item_Multi_HandleKey
                CreateMidHook(ui(0x4001128A,0x40015941), [](SafetyHookContext& ctx) {
                    auto& key = sp_mp(1) ? *(int*)(ctx.esp) : *(int*)(ctx.esp + 0x1C);
                    auto& current = sp_mp(1) ? ctx.eax : ctx.esi;
                    auto& max = sp_mp(1) ? ctx.ecx : ctx.eax;

                    if (key == 201) {
                        current = (current - 2 + max) % max;
                    }
                    });
            

        }

        void post_cgame() override
        {
            HMODULE cgame = (HMODULE)cg_game_offset;


                CreateMidHook(cg(0x3002570E,0x3003218A), [](SafetyHookContext& ctx) {

                    if (Cevar_Int(cg_ammo_overwrite_size_enabled) && Cevar_Value(cg_ammo_overwrite_size)) {

                        float* scale = sp_mp(1) ? (float*)&ctx.eax : (float*)ctx.esp;
                        *scale = Cevar_Value(cg_ammo_overwrite_size);


                    }


                    });
            

            Memory::VP::InterceptCall(cg(0x3002575E, 0x3003229F), cg_DrawPlayerStance_ptr, CG_DrawPlayerStance_hook);


            CreateMidHook(cg(0x30023959, 0x3002F589), [](SafetyHookContext& ctx) {

                if (Cevar_Int(cg_DrawPlayerStance_disable) == 1) {
                    EFlags32& flags = *(EFlags32*)&ctx.eflags;
                    flags.ZF = 1;
                }

                });


        }

    };

}
REGISTER_COMPONENT(gui::component);