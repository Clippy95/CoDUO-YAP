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
#include <buildnumber.h>
#include "framework.h"
cvar_t* get_cg_fov();
float GetAspectRatio();

cevar_s* cg_fovscale_effect_zoom_sens;
cevar_s* cg_fixedAspectFOV_fixads;
cevar_s* cg_fovMin;
cevar_s* cg_fovscale;
cevar_s* cg_fixedAspectFOV;
SafetyHookInline* CG_GetViewFov_og_S{};
double g_cg_fov_raw = 0.0;
constexpr auto STANDARD_ASPECT = 1.33333333333f;
float weapon_ads_fraction();

double ApplyFOVScale(double fov, double fovScale, bool useFixedAspect, bool dofovScaleMath) {
    if (dofovScaleMath && fovScale == 0.0) {
        fovScale = Cevar_Value(cg_fovscale, 1.f);
    }

    if (fovScale <= 0.0) {
        return fov; // Invalid scale, return original
    }

    if (!dofovScaleMath)
        fovScale = 1.0;

    if (dofovScaleMath)
        fov *= fovScale;

    double halfFovRad = (fov / 2.0) * (3.14159265358979323846 / 180.0);
    double tanHalfFov = tan(halfFovRad);

    if (useFixedAspect) {
        // horizontal FOV -> vertical (4:3 based), then back to horizontal for the real aspect ratio
        double tanHalfVFov = tanHalfFov / STANDARD_ASPECT;
        double newTanHalfFov = tanHalfVFov * GetAspectRatio();
        return (2.0 * atan(newTanHalfFov) * (180.0 / 3.14159265358979323846));
    }
    return (2.0 * atan(tanHalfFov) * (180.0 / 3.14159265358979323846));
}

double CG_GetViewFov_hook() {
    double fov = CG_GetViewFov_og_S->call<double>();
    g_cg_fov_raw = fov;

    const float fovMin = Cevar_Value(cg_fovMin, 1.f);
    if (cg_fovMin && (fovMin - fov) > 0.f) {
        fov = fovMin;
    }

    const float fovScale = Cevar_Value(cg_fovscale, 1.f);
    if (fovScale) {
        fov = ApplyFOVScale(fov, fovScale, Cevar_Int(cg_fixedAspectFOV, 1) != 0, true);
    }

    if (std::isnan(fov)) {
        cvar_t* fov_cvar = get_cg_fov();
        return fov_cvar ? fov_cvar->value : 80.f;
    }

    return fov;
}

namespace fov {

    // cg_weaponFov: 0 follows cg_fov, otherwise the first-person (depth hack) projection uses its own horizontal FOV. The renderer keeps a separate projection matrix for those entities, so scaling its x/y rows is enough
    cevar_s* cg_weaponFov;

    typedef void (WINAPI* glDepthRange_t)(double, double);
    typedef void (WINAPI* glMatrixMode_t)(unsigned);
    typedef void (WINAPI* glLoadMatrixf_t)(const float*);
    typedef void (WINAPI* glGetFloatv_t)(unsigned, float*);
    typedef void (WINAPI* glGetIntegerv_t)(unsigned, int*);

    SafetyHookInline* g_glDepthRange = nullptr;
    glMatrixMode_t fn_glMatrixMode;
    glLoadMatrixf_t fn_glLoadMatrixf;
    glGetFloatv_t fn_glGetFloatv;
    glGetIntegerv_t fn_glGetIntegerv;

    float WeaponProjectionScale() {
        float weaponFov = Cevar_Value(cg_weaponFov, 80.f);
        if (weaponFov <= 0.f)
            return 1.f;
        weaponFov = weaponFov < 1.f ? 1.f : (weaponFov > 160.f ? 160.f : weaponFov);

        cvar_t* fov_cvar = get_cg_fov();
        double hip = fov_cvar ? fov_cvar->value : 80.f;
        const double hipMin = sp_mp(1) ? 80.0 : 1.0;   // MP has the 80 degree lower clamp removed
        hip = hip < hipMin ? hipMin : (hip > 160.0 ? 160.0 : hip);
        const double toRad = 3.14159265358979323846 / 360.0;
        const double hipTan = tan(hip * toRad);
        const double k = hipTan / tan((double)weaponFov * toRad);

        // aiming down sights: fade the weapon back to the stock projection
        double zoom = 0.0;
        const float ads = weapon_ads_fraction();
        if (ads >= 0.f) {
            zoom = ads > 1.f ? 1.0 : ads;
        }
        else if (g_cg_fov_raw > 1.0) {
            const double r = tan(g_cg_fov_raw * toRad) / hipTan;
            zoom = (1.0 - r) / 0.15;
            zoom = zoom < 0.0 ? 0.0 : (zoom > 1.0 ? 1.0 : zoom);
        }
        return (float)(1.0 + (k - 1.0) * (1.0 - zoom));
    }

    void ScaleProjection(float* m, float k) {
        for (int i = 0; i < 4; i++) {
            m[i * 4 + 0] *= k;
            m[i * 4 + 1] *= k;
        }
    }

    // MP: the depth hack matrix is scaled inside R_SetupProjection, right after it was copied from the projection matrix (CoDUOMP.exe 0x4E4602)
    float* g_depthHackMatrix = nullptr;

    void ScaleDepthHackMatrix(SafetyHookContext&) {
        static bool faulted = false;
        float* m = g_depthHackMatrix;
        if (faulted || !m)
            return;
        __try {
            const float k = WeaponProjectionScale();
            if (m[0] != 0.f && m[5] != 0.f && k != 1.f && k > 0.f && k < 1000.f)
                ScaleProjection(m, k);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            faulted = true;
            YAP_LOG("[" MOD_NAME "] R_SetupProjection hook faulted, weapon fov scaling disabled\n");
        }
    }

    // installed at map load, the hook does not stick when it is written during post_unpack
    void InstallSetupProjectionHook() {
        static bool done = false;
        if (done)
            return;
        done = true;
        const uintptr_t fn = (uintptr_t)GetModuleHandleW(nullptr) + 0xE44E0;   // CoDUOMP.exe 0x4E44E0
        const uintptr_t site = fn + 0x122;                                     // fstp dword ptr [depthHack + 14 * 4]
        if (!Memory::VP::Valid(fn) || !Memory::VP::Valid(site + 6))
            return;
        const uint8_t* p = (const uint8_t*)site;
        const uint8_t* copy = (const uint8_t*)(fn + 0x120);
        if (p[0] != 0xD9 || p[1] != 0x1D || copy[0] != 0xF3 || copy[1] != 0xA5) {   // rep movsd; fstp dword ptr [abs]
            YAP_LOG("[" MOD_NAME "] R_SetupProjection: unexpected code, cg_weaponFov is disabled\n");
            return;
        }
        const uintptr_t store = *(const uintptr_t*)(p + 2);
        if (!Memory::VP::Valid(store))
            return;
        g_depthHackMatrix = (float*)(store - 14 * sizeof(float));
        CreateMidHook((void*)site, ScaleDepthHackMatrix);
    }

    void WINAPI glDepthRange_hook(double zNear, double zFar) {
        // RB_DrawSurfs loads depthHackProjectionMatrix and then calls glDepthRange(0, 0.2) for RF_DEPTHHACK entities
        if (zNear == 0.0 && zFar > 0.1999 && zFar < 0.2001) {
            const float k = WeaponProjectionScale();
            if (k != 1.f && k > 0.f && k < 1000.f) {
                int mode = 0;
                float m[16];
                fn_glGetIntegerv(0x0BA0, &mode);       // GL_MATRIX_MODE
                fn_glMatrixMode(0x1701);               // GL_PROJECTION
                fn_glGetFloatv(0x0BA7, m);             // GL_PROJECTION_MATRIX
                ScaleProjection(m, k);
                fn_glLoadMatrixf(m);
                fn_glMatrixMode((unsigned)mode);
            }
        }
        g_glDepthRange->unsafe_stdcall<void>(zNear, zFar);
    }

    bool isCurrentWeaponaSniper() {
        auto curweapon = *game::PlayerCurrentWeapon.get();
        return curweapon && curweapon->adsOverlayShader && *curweapon->adsOverlayShader;
    }

    class component final : public component_interface
    {
    public:

        void post_unpack() override
        {
            cg_fovscale_effect_zoom_sens = Cevar_Get("cg_fovscale_effect_zoom_sens", 0, CVAR_ARCHIVE, 0, 1);
            cg_fixedAspectFOV_fixads = Cevar_Get("cg_fixedAspectFOV_fixads", 1, CVAR_ARCHIVE, 0, 1);
            cg_fovMin = Cevar_Get("cg_fovMin", 1.f, CVAR_ARCHIVE, 1.f, 160.f);
            cg_fovscale = Cevar_Get("cg_fovscale", 1.f, CVAR_ARCHIVE);
            cg_fixedAspectFOV = Cevar_Get("cg_fixedAspectFOV", 1, CVAR_ARCHIVE);
            cg_weaponFov = Cevar_Get("cg_weaponFov", 80.f, CVAR_ARCHIVE, 0.f, 160.f);
        }

        void on_ogl_load(HMODULE tOHGL) override
        {
            static bool inited = false;
            if (inited)
                return;
            fn_glMatrixMode = (glMatrixMode_t)GetProcAddress(tOHGL, "glMatrixMode");
            fn_glLoadMatrixf = (glLoadMatrixf_t)GetProcAddress(tOHGL, "glLoadMatrixf");
            fn_glGetFloatv = (glGetFloatv_t)GetProcAddress(tOHGL, "glGetFloatv");
            fn_glGetIntegerv = (glGetIntegerv_t)GetProcAddress(tOHGL, "glGetIntegerv");
            auto depthRange = (void*)GetProcAddress(tOHGL, "glDepthRange");
            if (depthRange && fn_glMatrixMode && fn_glLoadMatrixf && fn_glGetFloatv && fn_glGetIntegerv) {
                g_glDepthRange = CreateInlineHook(depthRange, glDepthRange_hook);
                inited = g_glDepthRange != nullptr;
            }
        }

        void post_cgame() override
        {
            CG_GetViewFov_og_S = CreateInlineHook((void*)cg(0x3002CC20, 0x3003FFC0), &CG_GetViewFov_hook);

            if (!sp_mp(1))
                InstallSetupProjectionHook();

            auto ptr = cg(0x3002D008,0x30040352);
            Memory::VP::Nop(ptr, 6);
            CreateMidHook(ptr, [](SafetyHookContext& ctx) {
                cvar_t* fov_cvar = get_cg_fov();
                float originalFOV = fov_cvar ? fov_cvar->value : 80.f;

                if (Cevar_Int(cg_fixedAspectFOV_fixads)) {
                    originalFOV = ApplyFOVScale(originalFOV, 1.f, Cevar_Int(cg_fixedAspectFOV_fixads), Cevar_Int(cg_fovscale_effect_zoom_sens) != 0);
                }
                FPU::FDIV(originalFOV);


                });
        }

    };
}
REGISTER_COMPONENT(fov::component);