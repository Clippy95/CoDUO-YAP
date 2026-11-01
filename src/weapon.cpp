#include <helper.hpp>
#include "hook_trace.h"
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
#include "framework.h"

namespace weapon {
    cevar_t* cg_weaponBobAmplitudeSprinting_horz;
    cevar_t* cg_weaponBobAmplitudeSprinting_vert;

    cevar_t* bg_allowJumpShot;

    cevar_t* cg_bobAmplitudeSprinting_horz;

    cevar_t* cg_bobAmplitudeSprinting_vert;

    cevar_t* cg_weaponSprint_mod;

    cevar_t* player_sprintSpeedScale;

    struct eWeaponDef {
        std::string weaponName;
        float vSprintBob[2];
        float sprintSpeedScale;
        std::optional<std::array<float, 3>> vSprintRot;
        std::optional<std::array<float, 3>> vSprintMove;

        eWeaponDef() : weaponName(""), vSprintBob{ 0.0f, 0.0f }, sprintSpeedScale{ 1.f }, vSprintRot{}, vSprintMove{} {}
    };

    std::unordered_map<std::string, eWeaponDef> g_eWeaponDefs;


    bool g_sprint_branch = false;

    unsigned g_bob_applied[3] = {};

    bool sprint_flag_set() {
        const bool set = (player_flags && (*player_flags & 0x10000) != 0) || (!sp_mp(1) && g_sprint_branch);
        static bool last = false;
        if (set != last) {
            last = set;
            if (set) {
                if (sp_mp(1) && player_flags)
                    YAP_LOG("[" MOD_NAME "] sprint flag set, fatigue %.3f, flags %08X\n", *(const float*)((const uint8_t*)player_flags + 0x58C), *player_flags);
                else
                    YAP_LOG("[" MOD_NAME "] sprint flag set\n");
            }
            else {
                YAP_LOG("[" MOD_NAME "] sprint flag cleared, bob overrides applied: vertical %u, view %u, horizontal %u\n", g_bob_applied[0], g_bob_applied[1], g_bob_applied[2]);
                memset(g_bob_applied, 0, sizeof(g_bob_applied));
            }
        }
        return set;
    }

    bool sprint_rotate_is_sprinting() {
        return sprint_flag_set() && Cevar_Int(cg_weaponSprint_mod) != 0;
    }

    // eases the bob amplitude between normal and sprint values so entering sprint while already moving doesn't pop
    float SprintBlend() {
        static LARGE_INTEGER freq = {}, last = {};
        static float blend = 0.f;
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        if (!freq.QuadPart)
            QueryPerformanceFrequency(&freq);
        float dt = last.QuadPart ? (float)((double)(now.QuadPart - last.QuadPart) / (double)freq.QuadPart) : 0.f;
        last = now;
        if (dt < 0.f) dt = 0.f;
        if (dt > 0.05f) dt = 0.05f;
        const float step = dt / 0.15f;
        if (sprint_rotate_is_sprinting())
            blend = blend + step > 1.f ? 1.f : blend + step;
        else
            blend = blend - step < 0.f ? 0.f : blend - step;
        return blend;
    }

    inline float BlendAmp(float base, float sprint, float b) {
        return base + (sprint - base) * b;
    }

    // ads fraction for fov.cpp, -1 when the player state isn't known (MP layout only)
    float g_phase_offset = 0.f;
}
float weapon_ads_fraction();
namespace weapon {
    // ps->bobCycle can stay at 0 while sprint is held and the player starts moving; keep the sway phase running
    void SprintPhaseUpdate(float scale, float blend) {
        static LARGE_INTEGER freq = {}, last = {};
        static int lastBob = -1;
        static double changeTime = 0.0;
        static bool reported = false;
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        if (!freq.QuadPart)
            QueryPerformanceFrequency(&freq);
        const double t = (double)now.QuadPart / (double)freq.QuadPart;
        double dt = last.QuadPart ? (double)(now.QuadPart - last.QuadPart) / (double)freq.QuadPart : 0.0;
        last = now;
        dt = dt < 0.0 ? 0.0 : (dt > 0.05 ? 0.05 : dt);

        if (blend <= 0.f || !player_flags) {
            g_phase_offset = 0.f;
            lastBob = -1;
            reported = false;
            return;
        }

        const int bob = *(const int*)((const uint8_t*)player_flags - 4) & 0xFF;
        if (bob != lastBob) {
            lastBob = bob;
            changeTime = t;
        }
        if (scale > 2.f && t - changeTime > 0.1) {
            if (!reported) {
                reported = true;
                YAP_LOG("[" MOD_NAME "] bobCycle stalled at %d while moving (scale %.2f), driving sway phase manually\n", bob, scale);
            }
            const float ratio = scale / 48.64f > 1.2f ? 1.2f : scale / 48.64f;
            g_phase_offset = fmodf(g_phase_offset + (float)dt * 8.25f * ratio, 6.2831853f);
        }
    }

    vmCvar_t* g_gun_rot_p = nullptr;
    vmCvar_t* g_gun_rot_y = nullptr;
    vmCvar_t* g_gun_rot_r = nullptr;

    const uintptr_t* g_bob_cur = nullptr;
    const uintptr_t* g_bob_k1 = nullptr;
    const uintptr_t* g_bob_k2 = nullptr;
    const float* g_bob_v0 = nullptr;
    const float* g_bob_v1 = nullptr;
    const float* g_bob_v2 = nullptr;
    const uintptr_t* g_cur_weapon_ptr = nullptr;
    uintptr_t g_sp_horz_weapon_resume = 0;

    bool GetBobBase(double& out) {
        if (!g_bob_cur || !g_bob_k1 || !g_bob_k2) {
            static bool reported = false;
            if (!reported) {
                reported = true;
                YAP_LOG("[" MOD_NAME "] bob base unavailable, game offset %08X\n", (unsigned)cg_game_offset);
            }
            return false;
        }

        out = *(*g_bob_cur == *g_bob_k1 ? g_bob_v0 : (*g_bob_cur == *g_bob_k2 ? g_bob_v1 : g_bob_v2));
        return true;
    }

    uintptr_t g_weapon_info = 0;

    const char* GetCurrentWeaponName() {
        uintptr_t current_weapon = sp_mp(1) ? (g_cur_weapon_ptr ? *g_cur_weapon_ptr : 0) : g_weapon_info;
        if (current_weapon < 0x10000)
            return nullptr;

        const char* name = *(const char**)(current_weapon + 0x4);
        return (uintptr_t)name < 0x10000 ? nullptr : name;
    }

    const eWeaponDef* GetEWeapon(const char* weaponName) {
        if (!weaponName || g_eWeaponDefs.empty()) return nullptr;
        auto it = g_eWeaponDefs.find(weaponName);
        if (it != g_eWeaponDefs.end()) {
            return &it->second;
        }

        std::string name = weaponName;
        const size_t slash = name.find_last_of("/\\");
        if (slash != std::string::npos)
            name.erase(0, slash + 1);
        it = g_eWeaponDefs.find(name);
        if (it != g_eWeaponDefs.end())
            return &it->second;
        if (name.size() > 3 && name.compare(name.size() - 3, 3, "_mp") == 0) {
            it = g_eWeaponDefs.find(name.substr(0, name.size() - 3));
            if (it != g_eWeaponDefs.end())
                return &it->second;
        }

        return nullptr;
    }

    const eWeaponDef* GetCurrentEWeapon() {
        const char* name = GetCurrentWeaponName();
        const eWeaponDef* def = GetEWeapon(name);
        static std::string last;
        if (name && last != name) {
            last = name;
            YAP_LOG("[" MOD_NAME "] weapon '%s': %s\n", name, def ? "eWeapon found" : "no eWeapon definition");
        }
        return def;
    }

    double __cdecl CG_GetWeaponVerticalBobFactor(float a1, float a2, float a3)
    {
        double v3; // st7
        double v4; // st7
        float v6; // [esp+0h] [ebp-4h]



        if (!GetBobBase(v3))
            return 0.0;

        const float blend = SprintBlend();
        SprintPhaseUpdate(a2, blend);
        if (blend > 0.f)
            a1 += g_phase_offset;
        if (blend > 0.f) {
            const char* weaponName = GetCurrentWeaponName();
            if (weaponName) {
                auto eWeapon = GetEWeapon(weaponName);
                if (eWeapon) {
                    float vert_bob = Cevar_Value(cg_weaponBobAmplitudeSprinting_vert) * eWeapon->vSprintBob[1];
                    v3 = BlendAmp((float)v3, vert_bob, blend);
                    g_bob_applied[0]++;
#if YAP_HOOK_TRACE
                    static ULONGLONG lastSample = 0;
                    const ULONGLONG nowTick = GetTickCount64();
                    if (nowTick - lastSample > 500) {
                        lastSample = nowTick;
                        YAP_LOG("[" MOD_NAME "] bob sample: phase %.3f scale %.3f cap %.3f amp %.4f blend %.2f fatigue %.3f\n", a1, a2, a3, (float)v3, blend,
                            player_flags ? *(const float*)((const uint8_t*)player_flags + 0x58C) : -1.f);
                    }
#endif
                }
            }
        }

        v4 = v3 * a2;
        v6 = v4;
        if (v4 > a3)
            v6 = a3;
        return (sin(a1 * 4.0 + 1.5707964) * 0.2 + sin(a1 + a1)) * v6 * 0.75;
    }

    double __cdecl CG_GetViewVerticalBobFactor_sprint(float a1, float a2, float a3)
    {
        double v3; // st7
        double v4; // st7
        float v6; // [esp+0h] [ebp-4h]



        if (!GetBobBase(v3))
            return 0.0;

        const float blend = SprintBlend();
        if (blend > 0.f) {
            a1 += g_phase_offset;
            v3 = BlendAmp((float)v3, Cevar_Value(cg_bobAmplitudeSprinting_vert), blend);
            g_bob_applied[1]++;
        }

        v4 = v3 * a2;
        v6 = v4;
        if (v4 > a3)
            v6 = a3;
        return (sin(a1 * 4.0 + 1.5707964) * 0.2 + sin(a1 + a1)) * v6 * 0.75;
    }

    double __cdecl CG_GetViewGetHorizontalBobFactor_sprint(float a1, float a2, float a3)
    {
        double v3; // st7
        double v4; // st7
        float v6; // [esp+0h] [ebp-4h]



        if (!GetBobBase(v3))
            return 0.0;

        const float blend = SprintBlend();
        if (blend > 0.f) {
            a1 += g_phase_offset;
            v3 = BlendAmp((float)v3, Cevar_Value(cg_bobAmplitudeSprinting_horz), blend);
            g_bob_applied[1]++;
        }

        v4 = v3 * a2;
        v6 = v4;
        if (v4 > a3)
            v4 = a3;
        return v4 * sin(a1);
    }

    struct six_long {
        uint8_t b1;
        uint8_t b2;
        uint8_t b3;
        uint8_t b4;
        uint8_t b5;
        uint8_t b6;
    };

    six_long original_reads;

    six_long original_reads_1;

    six_long original_reads_2;

    bool sprint_site_ok(uintptr_t addr, uint8_t op0, uint8_t op1) {
        return addr && Memory::VP::Valid(addr) && *(uint8_t*)addr == op0 && *(uint8_t*)(addr + 1) == op1;
    }

    constexpr uintptr_t kMpBobVertWeaponRet = 0x30015350;
    constexpr uintptr_t kMpBobVertViewRet = 0x30015BED;
    constexpr uintptr_t kMpBobVertViewRet2 = 0x3003FD69;
    constexpr uintptr_t kMpBobHorzViewRet = 0x30015C0E;
    constexpr uintptr_t kMpBobHorzViewRet2 = 0x3003FD8A;

    uintptr_t g_ret_vert_weapon, g_ret_vert_view, g_ret_vert_view2, g_ret_horz_view, g_ret_horz_view2;
    uintptr_t g_vert_resume, g_horz_resume, g_horz_weapon_resume;

    float SiteBase(const SafetyHookContext& ctx) {
        return *(const float*)*(const uintptr_t*)(ctx.eip + 2);
    }

    void MpVertBob(SafetyHookContext& ctx) {
        const float blend = SprintBlend();
        if (blend <= 0.f)
            return;

        const uintptr_t ret = *(uintptr_t*)(ctx.esp + 4);
        float value;
        if (ret == g_ret_vert_weapon) {
            auto eWeapon = GetCurrentEWeapon();
            if (!eWeapon)
                return;
            value = BlendAmp(SiteBase(ctx), Cevar_Value(cg_weaponBobAmplitudeSprinting_vert) * eWeapon->vSprintBob[1], blend);
            g_bob_applied[0]++;
        }
        else if (ret == g_ret_vert_view || ret == g_ret_vert_view2) {
            value = BlendAmp(SiteBase(ctx), Cevar_Value(cg_bobAmplitudeSprinting_vert), blend);
            g_bob_applied[1]++;
        }
        else
            return;

        static bool reported = false;
        if (!reported) {
            reported = true;
            YAP_LOG("[" MOD_NAME "] vertical bob override applied (%f), caller %08X\n", value, (unsigned)ret);
        }
        FPU::FLD(value);
        ctx.eip = g_vert_resume;
    }

    void MpHorzViewBob(SafetyHookContext& ctx) {
        const uintptr_t ret = *(uintptr_t*)ctx.esp;
        if (ret != g_ret_horz_view && ret != g_ret_horz_view2)
            return;
        const float blend = SprintBlend();
        if (blend <= 0.f)
            return;

        static bool reported = false;
        if (!reported) {
            reported = true;
            YAP_LOG("[" MOD_NAME "] view horizontal bob override applied, caller %08X\n", (unsigned)ret);
        }
        g_bob_applied[1]++;
        FPU::FLD(BlendAmp(SiteBase(ctx), Cevar_Value(cg_bobAmplitudeSprinting_horz), blend));
        ctx.eip = g_horz_resume;
    }

    void MpHorzWeaponBob(SafetyHookContext& ctx) {
        const float blend = SprintBlend();
        if (blend <= 0.f)
            return;

        auto eWeapon = GetCurrentEWeapon();
        if (!eWeapon)
            return;

        static bool reported = false;
        if (!reported) {
            reported = true;
            YAP_LOG("[" MOD_NAME "] weapon horizontal bob override applied\n");
        }
        g_bob_applied[2]++;
        FPU::FLD(BlendAmp(SiteBase(ctx), Cevar_Value(cg_weaponBobAmplitudeSprinting_horz) * eWeapon->vSprintBob[0], blend));
        ctx.eip = g_horz_weapon_resume;
    }

    template<int I>
    void MpSprintRot(SafetyHookContext& ctx) {
        g_weapon_info = ctx.esi;
        const float* game_sprint_rot = (const float*)(ctx.esi + 0x128);
        auto eWeapon = GetCurrentEWeapon();
        if (eWeapon && eWeapon->vSprintRot && Cevar_Int(cg_weaponSprint_mod)) {
            static bool reported = false;
            if (!reported) {
                reported = true;
                YAP_LOG("[" MOD_NAME "] MP sprint rotation override applied\n");
            }
            FPU::FMUL((*eWeapon->vSprintRot)[I]);
            return;
        }

        FPU::FMUL(game_sprint_rot[I]);
    }

    void MpRotHooks() {
        // BG_CalculateWeaponPosition_BasePosition_angles, sprint branch reads weaponInfo->sprintRot
        const uintptr_t sites[] = { cg(0, 0x30014F60), cg(0, 0x30014F6C), cg(0, 0x30014F76) };
        for (auto site : sites) {
            if (!sprint_site_ok(site, 0xD8, 0x8E)) {
                YAP_LOG("[" MOD_NAME "] MP rotation hooks skipped: unexpected code at %08X\n", (unsigned)site);
                return;
            }
        }

        for (auto site : sites)
            Memory::VP::Nop((void*)site, 6);

        CreateMidHook(sites[0], MpSprintRot<0>);
        CreateMidHook(sites[1], MpSprintRot<1>);
        CreateMidHook(sites[2], MpSprintRot<2>);
    }

    void MpBobHooks() {
        g_ret_vert_weapon = cg(0, kMpBobVertWeaponRet);
        g_ret_vert_view = cg(0, kMpBobVertViewRet);
        g_ret_vert_view2 = cg(0, kMpBobVertViewRet2);
        g_ret_horz_view = cg(0, kMpBobHorzViewRet);
        g_ret_horz_view2 = cg(0, kMpBobHorzViewRet2);
        g_vert_resume = cg(0, 0x30014DFD);
        g_horz_resume = cg(0, 0x30014E7C);
        g_horz_weapon_resume = cg(0, 0x30015387);

        // the stance specific amplitude loads of each bob function
        const uintptr_t vert_sites[] = { cg(0, 0x30014DDF), cg(0, 0x30014DEF), cg(0, 0x30014DF7) };
        const uintptr_t horz_sites[] = { cg(0, 0x30014E5E), cg(0, 0x30014E6E), cg(0, 0x30014E76) };
        const uintptr_t weapon_horz_sites[] = { cg(0, 0x30015369), cg(0, 0x30015379), cg(0, 0x30015381) };

        for (auto site : vert_sites) {
            if (!sprint_site_ok(site, 0xD9, 0x05)) {
                YAP_LOG("[" MOD_NAME "] MP bob hooks skipped: unexpected code at %08X\n", (unsigned)site);
                return;
            }
        }
        for (auto site : horz_sites) {
            if (!sprint_site_ok(site, 0xD9, 0x05)) {
                YAP_LOG("[" MOD_NAME "] MP bob hooks skipped: unexpected code at %08X\n", (unsigned)site);
                return;
            }
        }
        for (auto site : weapon_horz_sites) {
            if (!sprint_site_ok(site, 0xD9, 0x05)) {
                YAP_LOG("[" MOD_NAME "] MP bob hooks skipped: unexpected code at %08X\n", (unsigned)site);
                return;
            }
        }

        for (auto site : vert_sites)
            CreateMidHook(site, MpVertBob);
        for (auto site : horz_sites)
            CreateMidHook(site, MpHorzViewBob);
        for (auto site : weapon_horz_sites)
            CreateMidHook(site, MpHorzWeaponBob);
    }

    void SprintT4_lol(HMODULE handle) {
        if (!cg_game_offset)
            return;

        // MP cgame has no weapon rotation code, those loads are the gun offset cvars of the position function
        const uintptr_t rot_x = sp_mp(1) ? cg(0x30031DF0) : 0;
        const uintptr_t rot_y = sp_mp(1) ? cg(0x30031DFE) : 0;
        const uintptr_t rot_z = sp_mp(1) ? cg(0x30031E10) : 0;
        const uintptr_t move_x = cg(0x30031A25, 0x30044DA5);
        const uintptr_t move_y = cg(0x30031A2D, 0x30044DAD);
        const uintptr_t move_z = cg(0x30031A35, 0x30044DB5);

        const bool rot_ok = !sp_mp(1) || (sprint_site_ok(rot_x, 0xD9, 0x05) && sprint_site_ok(rot_y, 0xD9, 0x05) && sprint_site_ok(rot_z, 0xD9, 0x05));
        if (!rot_ok ||
            !sprint_site_ok(move_x, 0xD8, 0x8A) || !sprint_site_ok(move_y, 0xD8, 0x8A) || !sprint_site_ok(move_z, 0xD8, 0x8A)) {
            YAP_LOG("[" MOD_NAME "] sprint hooks skipped: unexpected code at the patch sites\n");
            return;
        }

        const uintptr_t pos_func = cg(0x30031960, 0x30044CE0);
        player_flags = sprint_site_ok(pos_func, 0x8B, 0x0D) ? *(uint32_t**)(pos_func + 2) : nullptr;
        if (!player_flags) {
            YAP_LOG("[" MOD_NAME "] sprint hooks skipped: sprint flag not found\n");
            return;
        }
        YAP_LOG("[" MOD_NAME "] sprint hooks installed (%zu eWeapon definitions), flags at %p (mapped %p)\n", g_eWeaponDefs.size(), (void*)player_flags, (void*)cg_ptr<uint32_t>(0x3026D9F0, 0x304831D0));

        if (sp_mp(1)) {
            g_bob_cur = cg_ptr<uintptr_t>(0x3026DAC8);
            g_bob_k1 = cg_ptr<uintptr_t>(0x3026DF40);
            g_bob_k2 = cg_ptr<uintptr_t>(0x3026DF44);
            g_bob_v0 = cg_ptr<float>(0x30256828);
            g_bob_v1 = cg_ptr<float>(0x3022C9A8);
            g_bob_v2 = cg_ptr<float>(0x30255CE8);
            g_cur_weapon_ptr = cg_ptr<uintptr_t>(0x30275DF0);
            g_sp_horz_weapon_resume = cg(0x30031054);
        }

        g_gun_rot_p = cg_ptr<vmCvar_t>(0x30258E60, 0x30457F40);
        g_gun_rot_y = cg_ptr<vmCvar_t>(0x30256CA0, 0x30457BE0);
        g_gun_rot_r = cg_ptr<vmCvar_t>(0x30252A40, 0x3052FB20);

        if (sp_mp(1)) {
            Memory::VP::Read(rot_x, original_reads);
            Memory::VP::Read(rot_y, original_reads_1);
            Memory::VP::Read(rot_z, original_reads_2);

            Memory::VP::Nop((void*)rot_x, 6);
            Memory::VP::Nop((void*)rot_y, 6);
            Memory::VP::Nop((void*)rot_z, 6);

            CreateMidHook(rot_x, [](SafetyHookContext& ctx) {
                g_weapon_info = ctx.edx;
                g_sprint_branch = (ctx.ecx & 0x10000) != 0;
                static bool reported_rot = false;
                if (g_sprint_branch && !reported_rot) {
                    reported_rot = true;
                    YAP_LOG("[" MOD_NAME "] rotation hook in sprint branch, flags %08X, game offset %08X\n", player_flags ? *player_flags : 0u, (unsigned)cg_game_offset);
                }
                vmCvar_t* cg_gun_rot_p = g_gun_rot_p;
                vector3* game_sprint_rot = (vector3*)(ctx.edx + 0x128);
                auto eWeapon = GetCurrentEWeapon();
                if (sprint_rotate_is_sprinting()) {
                    if (eWeapon && eWeapon->vSprintRot) {
                        FPU::FLD((*eWeapon->vSprintRot)[0]);
                        return;
                    }
                    if (sp_mp(1)) {
                        FPU::FLD(game_sprint_rot->x);
                        return;
                    }
                }

                FPU::FLD(cg_gun_rot_p ? cg_gun_rot_p->value : 0.f);
                });

            CreateMidHook(rot_y, [](SafetyHookContext& ctx) {
                vmCvar_t* cg_gun_rot_y = g_gun_rot_y;
                vector3* game_sprint_rot = (vector3*)(ctx.edx + 0x128);
                auto eWeapon = GetCurrentEWeapon();
                if (sprint_rotate_is_sprinting()) {
                    if (eWeapon && eWeapon->vSprintRot) {
                        FPU::FLD((*eWeapon->vSprintRot)[1]);
                        return;
                    }
                    if (sp_mp(1)) {
                        FPU::FLD(game_sprint_rot->y);
                        return;
                    }
                }

                FPU::FLD(cg_gun_rot_y ? cg_gun_rot_y->value : 0.f);
                });

            CreateMidHook(rot_z, [](SafetyHookContext& ctx) {
                vmCvar_t* cg_gun_rot_r = g_gun_rot_r;
                vector3* game_sprint_rot = (vector3*)(ctx.edx + 0x128);
                auto eWeapon = GetCurrentEWeapon();
                if (sprint_rotate_is_sprinting()) {
                    if (eWeapon && eWeapon->vSprintRot) {
                        FPU::FLD((*eWeapon->vSprintRot)[2]);
                        return;
                    }
                    if (sp_mp(1)) {
                        FPU::FLD(game_sprint_rot->z);
                        return;
                    }
                }

                FPU::FLD(cg_gun_rot_r ? cg_gun_rot_r->value : 0.f);
                });
        }

        if (sp_mp(1)) {
            CreateMidHook(cg(0x3003104E), [](SafetyHookContext& ctx) {
                const float blend = SprintBlend();
                if (blend > 0.f) {
                    *(float*)(ctx.esp + 8) += g_phase_offset;
                    auto eWeapon = GetCurrentEWeapon();
                    if (eWeapon) {
                        g_bob_applied[2]++;
                        FPU::FLD(BlendAmp(SiteBase(ctx), Cevar_Value(cg_weaponBobAmplitudeSprinting_horz) * eWeapon->vSprintBob[0], blend));
                        ctx.eip = g_sp_horz_weapon_resume;
                    }
                }
                });
        }

        Memory::VP::Nop((void*)move_x, 6);
        Memory::VP::Nop((void*)move_y, 6);
        Memory::VP::Nop((void*)move_z, 6);

        CreateMidHook(move_x, [](SafetyHookContext& ctx) {
            g_weapon_info = ctx.edx;
            static bool reported = false;
            if (!reported) {
                reported = true;
                YAP_LOG("[" MOD_NAME "] sprint branch reached, ecx %08X, flags %08X\n", (unsigned)ctx.ecx, player_flags ? *player_flags : 0u);
            }
            vector3* game_sprintMove = (vector3*)(ctx.edx + 0x11C);
            auto eWeapon = GetCurrentEWeapon();
#if YAP_HOOK_TRACE
            {
                static uintptr_t last_info = 0;
                const vector3* game_sprintRot = (vector3*)(ctx.edx + 0x128);
                if (last_info != ctx.edx) {
                    last_info = ctx.edx;
                    const char* name = GetCurrentWeaponName();
                    YAP_LOG("[" MOD_NAME "] sprint pose '%s': game move %.3f %.3f %.3f rot %.3f %.3f %.3f", name ? name : "?",
                        game_sprintMove->x, game_sprintMove->y, game_sprintMove->z, game_sprintRot->x, game_sprintRot->y, game_sprintRot->z);
                    if (eWeapon) {
                        YAP_LOG(", eWeapon move %s", eWeapon->vSprintMove ? "" : "none");
                        if (eWeapon->vSprintMove)
                            YAP_LOG("%.3f %.3f %.3f", (*eWeapon->vSprintMove)[0], (*eWeapon->vSprintMove)[1], (*eWeapon->vSprintMove)[2]);
                        YAP_LOG(" rot %s", eWeapon->vSprintRot ? "" : "none");
                        if (eWeapon->vSprintRot)
                            YAP_LOG("%.3f %.3f %.3f", (*eWeapon->vSprintRot)[0], (*eWeapon->vSprintRot)[1], (*eWeapon->vSprintRot)[2]);
                        YAP_LOG(" bob %.3f %.3f", eWeapon->vSprintBob[0], eWeapon->vSprintBob[1]);
                    }
                    YAP_LOG("\n");
                }
            }
#endif
            if (eWeapon && eWeapon->vSprintMove && Cevar_Int(cg_weaponSprint_mod)) {
                FPU::FMUL((*eWeapon->vSprintMove)[0]);
                return;
            }

            FPU::FMUL(game_sprintMove->x);
            });

        CreateMidHook(move_y, [](SafetyHookContext& ctx) {
            vector3* game_sprintMove = (vector3*)(ctx.edx + 0x11C);
            auto eWeapon = GetCurrentEWeapon();
            if (eWeapon && eWeapon->vSprintMove && Cevar_Int(cg_weaponSprint_mod)) {
                FPU::FMUL((*eWeapon->vSprintMove)[1]);
                return;
            }

            FPU::FMUL(game_sprintMove->y);
            });

        CreateMidHook(move_z, [](SafetyHookContext& ctx) {
            vector3* game_sprintMove = (vector3*)(ctx.edx + 0x11C);
            auto eWeapon = GetCurrentEWeapon();
            if (eWeapon && eWeapon->vSprintMove && Cevar_Int(cg_weaponSprint_mod)) {
                FPU::FMUL((*eWeapon->vSprintMove)[2]);
                return;
            }

            FPU::FMUL(game_sprintMove->z);
            });

        if (sp_mp(1))
            Memory::VP::InjectHook(cg(0x30031010), CG_GetWeaponVerticalBobFactor);
        else {
            MpRotHooks();
            MpBobHooks();
        }
    }


    void LoadEWeaponsFromDirectory(const std::filesystem::path& eWeaponsDir, bool overwrite = true) {
        if (!std::filesystem::exists(eWeaponsDir)) {
            Com_Printf("eWeapons directory not found: %s\n", eWeaponsDir.string().c_str());
            return;
        }

        Com_Printf("Loading eWeapons from: %s\n", eWeaponsDir.string().c_str());

        for (const auto& entry : std::filesystem::directory_iterator(eWeaponsDir)) {
            if (entry.path().extension() != ".json") continue;

            std::string weaponName = entry.path().stem().string();

            if (!overwrite && g_eWeaponDefs.find(weaponName) != g_eWeaponDefs.end()) {
                Com_Printf("Skipping '%s' (already loaded with higher priority)\n", weaponName.c_str());
                continue;
            }

            try {
                std::ifstream file(entry.path());
                nlohmann::json j = nlohmann::json::parse(file);

                eWeaponDef weaponDef;
                weaponDef.weaponName = weaponName;

                if (j.contains("sprintBobH")) {
                    weaponDef.vSprintBob[0] = j["sprintBobH"].get<float>();
                }

                if (j.contains("sprintBobV")) {
                    weaponDef.vSprintBob[1] = j["sprintBobV"].get<float>();
                }

                if (j.contains("sprintSpeedScale")) {
                    weaponDef.sprintSpeedScale = j["sprintSpeedScale"].get<float>();
                }

                if (j.contains("SprintRot") && j["SprintRot"].is_array() && j["SprintRot"].size() == 3) {
                    weaponDef.vSprintRot = std::array<float, 3>{
                        j["SprintRot"][0].get<float>(),
                        j["SprintRot"][1].get<float>(),
                        j["SprintRot"][2].get<float>()
                    };
                }



                if (j.contains("SprintMove") && j["SprintMove"].is_array() && j["SprintMove"].size() == 3) {
                    weaponDef.vSprintMove = std::array<float, 3>{
                        j["SprintMove"][0].get<float>(),
                        j["SprintMove"][1].get<float>(),
                        j["SprintMove"][2].get<float>()
                    };
                }

                g_eWeaponDefs[weaponName] = weaponDef;

                Com_Printf("Loaded eWeapon '%s' - sprintBobH: %.3f, sprintBobV: %.3f, sprintSpeedScale %.3f\n",
                    weaponName.c_str(),
                    weaponDef.vSprintBob[0],
                    weaponDef.vSprintBob[1],
                    weaponDef.sprintSpeedScale);

            }
            catch (const std::exception& e) {
                Com_Printf("Failed to parse %s: %s\n",
                    entry.path().string().c_str(), e.what());
            }
        }
    }

    void loadEWeapons() {
        g_eWeaponDefs.clear();
        char modulePath[MAX_PATH];
        GetModuleFileNameA(NULL, modulePath, MAX_PATH);
        std::filesystem::path exePath(modulePath);
        std::filesystem::path baseDir = exePath.parent_path();
        cvar_s* fs_game = Cvar_Find("fs_game");
        cvar_s* fs_basegame = Cvar_Find("fs_basegame");

        std::filesystem::path baseEWeaponsDir = baseDir / "eWeapons";
        LoadEWeaponsFromDirectory(baseEWeaponsDir, true);

        if (fs_basegame && fs_basegame->string && fs_basegame->string[0] != '\0') {
            std::filesystem::path fsBaseGameDir = baseDir / fs_basegame->string;
            std::filesystem::path fsBaseGameEWeaponsDir = fsBaseGameDir / "eWeapons";
            LoadEWeaponsFromDirectory(fsBaseGameEWeaponsDir, true);
        }

        if (fs_game && fs_game->string && fs_game->string[0] != '\0') {
            std::filesystem::path fsGameDir = baseDir / fs_game->string;
            std::filesystem::path fsGameEWeaponsDir = fsGameDir / "eWeapons";
            LoadEWeaponsFromDirectory(fsGameEWeaponsDir, true);
            Com_Printf("Loaded %d eWeapon definitions (fs_game: '%s' has priority)\n",
                g_eWeaponDefs.size(),
                fs_game->string);
        }
        else {
            Com_Printf("Loaded %d eWeapon definitions from base directory\n",
                g_eWeaponDefs.size());
        }
    }

    void PatchSprintScale(HMODULE handle) {
        if (!sp_mp(1))
            return;
        auto pattern = hook::pattern(handle, "? ? ? ? ? ? EB ? ? ? ? ? ? ? EB ? ? ? ? ? ? ? 8B 46");
        if (!pattern.empty() && player_sprintSpeedScale && player_sprintSpeedScale->base) {


            Memory::VP::Nop(pattern.get_first(), 6);
            CreateMidHook(pattern.get_first(), [](SafetyHookContext& ctx) {

                float current_speedscale = Cevar_Value(player_sprintSpeedScale, 1.f);
                auto eWeapon = GetCurrentEWeapon();
                if (eWeapon) {
                    current_speedscale *= eWeapon->sprintSpeedScale;
                }
                FPU::FMUL(current_speedscale);
                });

        }

        //pattern = hook::pattern(handle,"83 F8 ? 75 ? ? ? ? ? ? ? ? ? ? ? E9 ? ? ? ? 83 F8");
        //if (!pattern.empty()) {
        //    CreateMidHook(pattern.get_first(), [](SafetyHookContext& ctx) {

        //        printf("speed %f\n", *(float*)(ctx.esp + 0x8));

        //        });
        //}

    }

    void PatchJumpShot(HMODULE handle) {
        if (!sp_mp(1))
            return;
        auto pattern = hook::pattern(handle, "0F 85 ? ? ? ? 8B 0D ? ? ? ? 8B 90");
        if (!pattern.empty()) {

            CreateMidHook(pattern.get_first(), [](SafetyHookContext& ctx) {

                if (bg_allowJumpShot && bg_allowJumpShot->base && bg_allowJumpShot->base->integer) {
                    ctx.eip += 6;
                }

                });

        }

        //pattern = hook::pattern(handle,"83 F8 ? 75 ? ? ? ? ? ? ? ? ? ? ? E9 ? ? ? ? 83 F8");
        //if (!pattern.empty()) {
        //    CreateMidHook(pattern.get_first(), [](SafetyHookContext& ctx) {

        //        printf("speed %f\n", *(float*)(ctx.esp + 0x8));

        //        });
        //}

    }


    uintptr_t *syscall;
    //static bool do_transitionTime = false;
    //static cevar_s* yap_xanim_iw3_transitionTime;
    //int startweaponanim_syscall(int id, int a1, int a2, float a3, float a4, int a5, int a6, int a7) {

    //    if (do_transitionTime)
    //        a4 = yap_xanim_iw3_transitionTime->base->value;

    //    return cdecl_call<int>(*syscall, id, a1, a2, a3, a4, a5, a6, a7);

    //}


    // UO stalls regeneration while the sprint key is held, even when the player is idle or pressed against a wall. SP: drain and the regen block now apply only while actually moving
    cevar_t* player_sprintIdleRegen = nullptr;

    struct FatigueSite {
        uintptr_t pm;
        uintptr_t msec;
        uintptr_t nofatigue;
        void (*playSound)();
        const float* drain[2];
        const float* regen[2];
        SafetyHookInline* hook;
    };
    FatigueSite g_fatigue[2];

    template<int M>
    void __cdecl PM_UpdateFatigue_Idle() {
        const FatigueSite& c = g_fatigue[M];
        if (!Cevar_Int(player_sprintIdleRegen, 1)) {
            c.hook->call<void>();
            return;
        }

        uint8_t* pm = *(uint8_t**)c.pm;
        uint8_t* ps = *(uint8_t**)pm;
        uint32_t& flags = *(uint32_t*)(ps + 0x0C);
        float& fatigue = *(float*)(ps + 0x598);
        int& lastSprintTime = *(int*)(ps + 0x59C);
        const int commandTime = *(int*)(pm + 4);
        const uint8_t buttons = *(uint8_t*)(pm + 8);
        const int msec = *(int*)c.msec;
        const float vx = *(float*)(ps + 0x20);
        const float vy = *(float*)(ps + 0x24);
        const bool moving = vx * vx + vy * vy > 100.f;
        const bool sprinting = (flags & 0x10000) != 0;

        if (sprinting) {
            if (*(int*)(ps + 4) == 2 || *(int*)c.nofatigue)
                return;
            if (moving) {
                lastSprintTime = commandTime;
                fatigue = (float)((double)fatigue - (double)msec * *c.drain[0] * *c.drain[1]);
                if (fatigue < 0.5f) {
                    int& n = *(int*)(ps + 0x78);
                    *(int*)(ps + 0x7C + (n & 3) * 4) = 0x8C;
                    *(int*)(ps + 0x8C + (n & 3) * 4) = 0;
                    n++;
                    flags |= 0x20000;
                }
                if (fatigue < 0.f)
                    fatigue = 0.f;
                return;
            }
        }

        // a held sprint key only blocks regen while the player is actually moving without being able to sprint
        if (!((buttons & 8) && moving) && commandTime >= lastSprintTime + 1000) {
            fatigue = (float)((double)fatigue + (double)msec * *c.regen[0] * *c.regen[1]);
            if (fatigue >= 1.f) {
                fatigue = 1.f;
                flags &= ~0x20000u;
            }
        }
        c.playSound();
    }

    // PM_Footsteps stores (int)(msec * rate + bobCycle), so a small rate never leaves 0 and the sprint sway stays frozen. Keep the dropped fraction and carry it over, at the nominal run rate scaled by the speed seen so far
    struct BobSite { const int* msec; float acc; float maxSpeed; };
    BobSite g_bobSite[2];

    void BobStallStep(BobSite& st, SafetyHookContext& ctx) {
        const uint32_t* ps = (const uint32_t*)ctx.esi;
        if (!(ps[3] & 0x10000u) || !st.msec)
            return;
        if (((ctx.eax ^ ctx.ebp) & 0xFF) != 0) {
            st.acc = 0.f;
            return;
        }
        const float vx = *(const float*)(ctx.esi + 0x20), vy = *(const float*)(ctx.esi + 0x24);
        const float speed = sqrtf(vx * vx + vy * vy);
        if (speed > st.maxSpeed)
            st.maxSpeed = speed;
        const float ratio = st.maxSpeed > 1.f ? speed / st.maxSpeed : 1.f;
        st.acc += (float)*st.msec * 0.335f * ratio;
        if (st.acc >= 1.f) {
            st.acc -= 1.f;
            ctx.eax = ctx.ebp + 1;
        }
    }

    void InstallBobStallFix(int idx, uintptr_t site) {
        static const uint8_t expect[] = { 0x25, 0xFF, 0x00, 0x00, 0x00, 0x89, 0x46, 0x08 };
        if (!site || !Memory::VP::Valid(site) || !Memory::VP::Valid(site - 0x1C) || memcmp((const void*)site, expect, sizeof(expect)) != 0)
            return;
        const uint8_t* fild = (const uint8_t*)(site - 0x1C);   // fild dword ptr [pml.msec]
        if (fild[0] != 0xDB || fild[1] != 0x05)
            return;
        g_bobSite[idx].msec = *(const int* const*)(fild + 2);
        if (idx == 0)
            CreateMidHook((void*)site, [](SafetyHookContext& ctx) { BobStallStep(g_bobSite[0], ctx); });
        else
            CreateMidHook((void*)site, [](SafetyHookContext& ctx) { BobStallStep(g_bobSite[1], ctx); });
    }

    bool InstallFatigueHook(int idx, uintptr_t fn, uintptr_t snd, void* detour) {
        auto ok = [](uintptr_t a, std::initializer_list<int> b) {
            if (!a || !Memory::VP::Valid(a)) return false;
            int i = 0;
            for (int v : b) { if (v >= 0 && ((uint8_t*)a)[i] != v) return false; i++; }
            return true;
        };
        // 8B 15 <pm> 8B 02 F7 40 0C 00 00 01 00 0F 84 ...
        if (!ok(fn, { 0x8B,0x15,-1,-1,-1,-1,0x8B,0x02,0xF7,0x40,0x0C,0x00,0x00,0x01,0x00,0x0F,0x84 }) ||
            !ok(fn + 0x1F, { 0x8B,0x0D }) || !ok(fn + 0x36, { 0xDB,0x05 }) ||
            !ok(fn + 0x3E, { 0xD8,0x0D }) || !ok(fn + 0x44, { 0xD8,0x0D }) ||
            !ok(fn + 0xD7, { 0xD8,0x0D }) || !ok(fn + 0xDD, { 0xD8,0x0D }) || !ok(snd, { 0x8B,0x0D })) {
            YAP_LOG("[" MOD_NAME "] fatigue hook skipped, unexpected code at %08X\n", (unsigned)fn);
            return false;
        }
        auto rd = [&](uintptr_t a) { return *(uintptr_t*)a; };
        FatigueSite& c = g_fatigue[idx];
        c.pm = rd(fn + 2);
        c.nofatigue = rd(fn + 0x21);
        c.msec = rd(fn + 0x38);
        c.drain[0] = (const float*)rd(fn + 0x40);
        c.drain[1] = (const float*)rd(fn + 0x46);
        c.regen[0] = (const float*)rd(fn + 0xD9);
        c.regen[1] = (const float*)rd(fn + 0xDF);
        c.playSound = (void (*)())snd;
        c.hook = CreateInlineHook((void*)fn, detour);
        return c.hook != nullptr;
    }

    class component final : public component_interface
    {
    public:
        void post_unpack() override
        {
            //yap_xanim_iw3_transitionTime = Cevar_Get("yap_xanim_iw3_transitionTime", 0.f, CVAR_ARCHIVE, 0.f, 1.f);
                cg_weaponBobAmplitudeSprinting_horz = Cevar_Get("cg_BobweaponAmplitudeSprinting_horz", 0.02f, 0, 0.f, 1.f);
                cg_weaponBobAmplitudeSprinting_vert = Cevar_Get("cg_BobweaponAmplitudeSprinting_vert", 0.014f, 0, 0.f, 1.f);

                cg_bobAmplitudeSprinting_horz = Cevar_Get("cg_bobAmplitudeSprinting_horz", 0.02f, 0, 0.f, 1.f);
                cg_bobAmplitudeSprinting_vert = Cevar_Get("cg_bobAmplitudeSprinting_vert", 0.014f, 0, 0.f, 1.f);

                cg_weaponSprint_mod = Cevar_Get("cg_weaponSprint_mod", 1, CVAR_ARCHIVE);

            if (sp_mp(1)) {
                player_sprintSpeedScale = Cevar_Get("player_sprintSpeedScale", 1.6f, NULL, 0.f, FLT_MAX);

                bg_allowJumpShot = Cevar_Get("bg_allowJumpShot", 0, CVAR_ARCHIVE, 0, 1);
                player_sprintIdleRegen = Cevar_Get("player_sprintIdleRegen", 1, CVAR_ARCHIVE, 0, 1);
            }


            game::Cmd_AddCommand("reload_eweapons", loadEWeapons);


        }

        void post_cgame() override
        {

            auto pattern = hook::pattern((HMODULE)cg_game_offset,"74 ? 43 83 FB");

            //if (!pattern.empty()) {
            //    CreateMidHook(pattern.get_first(), [](SafetyHookContext& ctx) {

            //        if (yap_xanim_iw3_transitionTime->base->value != 0.f)
            //            do_transitionTime = true;

            //        });

            //    pattern = hook::pattern((HMODULE)cg_game_offset, "FF 15 ? ? ? ? 83 C4 ? 46 83 C7");
            //    if (!pattern.empty()) {

            //        Memory::VP::Read(pattern.get_first(2), syscall);
            //        Memory::VP::Nop(pattern.get_first(), 6);
            //        Memory::VP::InjectHook(pattern.get_first(), startweaponanim_syscall,Memory::VP::HookType::Call);

            //        pattern = hook::pattern((HMODULE)cg_game_offset, "75 ? 5F 5E 5D 83 C4");

            //        if (!pattern.empty()) {
            //            CreateMidHook(pattern.get_first(2), [](SafetyHookContext& ctx) {

            //
            //                    do_transitionTime = false;

            //                });
            //        }

            //    }
            //    //pattern = hook::pattern((HMODULE)cg_game_offset, "51 56 53 68 ? ? ? ? FF 15 ? ? ? ? 83 C4 ? 46");

            //    //if (!pattern.empty()) {

            //    //    CreateMidHook(pattern.get_first(), [](SafetyHookContext& ctx) {

            //    //        if (do_transitionTime) {
            //    //            *(float*)(ctx.esp) = yap_xanim_iw3_transitionTime->base->value;
            //    //        }

            //    //        });

            //    //}

            //}

            loadEWeapons();

            SprintT4_lol((HMODULE)cg_game_offset);

            if (!sp_mp(1))
                return;

            PatchSprintScale((HMODULE)cg_game_offset);

            Memory::VP::InjectHook(cg(0x3002C995), CG_GetViewGetHorizontalBobFactor_sprint);
            Memory::VP::InjectHook(cg(0x3002C9EA), CG_GetViewGetHorizontalBobFactor_sprint);

            Memory::VP::InjectHook(cg(0x3002C9CE), CG_GetViewVerticalBobFactor_sprint);
            Memory::VP::InjectHook(cg(0x3002C961), CG_GetViewVerticalBobFactor_sprint);
            PatchJumpShot((HMODULE) cg_game_offset);
            InstallFatigueHook(0, cg(0x300067A0), cg(0x30006730), (void*)PM_UpdateFatigue_Idle<0>);
            InstallBobStallFix(0, cg(0x3000649D));

        }

        void post_game_sp() override
        {
            PatchSprintScale((HMODULE)game_offset);
            PatchJumpShot((HMODULE)game_offset);
            InstallFatigueHook(1, game_offset + 0x70A0, game_offset + 0x7030, (void*)PM_UpdateFatigue_Idle<1>);
            InstallBobStallFix(1, game_offset + 0x6D9D);
        }

    };



}
REGISTER_COMPONENT(weapon::component);

float weapon_ads_fraction() {
    // MP player state layout only: adsFraction sits at +0xE0, flags at +0xC
    if (sp_mp(1) || !player_flags)
        return -1.f;
    return *(const float*)((const uint8_t*)player_flags - 0xC + 0xE0);
}
