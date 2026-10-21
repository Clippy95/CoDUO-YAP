// dllmain.cpp : Defines the entry point for the DLL application.
import game;
#include "pch.h"
#include "game/game.h"
#include <component_loader.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#include "utils/common.h"
#include "cexception.hpp"
#include "utils/hooking.h"
#include <direct.h>
//#include "MinHook.h"


#define SCREEN_WIDTH        640
#define SCREEN_HEIGHT       480

#define MAX_CVARS_OLD 1024

#define MAX_CVARS_NEW 2048






static char charbuffer[2048]{};



std::vector<const char*> QOL_registered_cvars;
typedef cvar_t* (__cdecl* Cvar_GetT)(const char* var_name, const char* var_value, int flags);
Cvar_GetT Cvar_GetPtr = (Cvar_GetT)NULL;

cvar_t* __cdecl Cvar_Get(const char* var_name, const char* var_value, int flag) {

    if (!(std::find(QOL_registered_cvars.begin(), QOL_registered_cvars.end(), var_name) != QOL_registered_cvars.end())) {
        QOL_registered_cvars.push_back(var_name);
    }

    return Cvar_GetPtr(var_name, var_value, flag);
}

// SP Only

cvar_t* g_save_allowbadchecksum;

// cvars
cvar_t* cg_fov;

cvar_t* get_cg_fov() {
    return cg_fov;
}

cvar_t* cg_fovscale_ads;
cevar_t* cg_fov_fix_lowfovads;
cevar_t* cg_fovMin;
cvar_t* cg_fovscale;
cevar_t* cg_hudelem_alignhack;
cevar_t* cg_hudelem_printnames;
cvar_t* cg_fixedAspectFOV;
cevar_t* cg_fixedAspect;
cvar_t* safeArea_horizontal;
cvar_t* safeArea_vertical;
cvar_t* r_noborder;
cevar_t* r_mode_auto;
cevar_t* cg_fixedAspect_blackshader_all;

cvar_t* player_sprintmult;

cvar_t* hook_shortversion;

cvar_t* hook_version;

cevar_t* r_qol_texture_filter_anisotropic;

void codDLLhooks(HMODULE handle);

void ui_hooks(HMODULE handle);

unsigned int x_modded = 1920;

typedef HMODULE(__cdecl* LoadsDLLsT)(const char* a1, FARPROC* a2, int a3);
LoadsDLLsT originalLoadDLL = nullptr;

enum COD_GAME {
    UNSUPPORTED = -1,
    UO_SP,
    UO_MP,
    COD_MAX_GAMES,
};

struct COD_Classic_Version {
    DWORD WinMain_Check[2];
    const char* cgamename;
    const char* uixname;
    DWORD LoadDLLAddr;
    DWORD DLL_CG_GetViewFov_offset;
    DWORD Cvar_Get_Addr;
    DWORD X_res_Addr;
    DWORD GL_Ortho_ptr;
    DWORD gl_ortho_ret;
    DWORD Item_Paint;
    DWORD CG_DrawFlashImage_Draw;
    COD_GAME game;
};

COD_Classic_Version COD_UO_SP = {
{0x00455050,0x83EC8B55},
"uo_cgamex86.dll",
"uo_uix86.dll",
0x454440,
0x2CC20,
0x004337F0,
0x047BE104,
0x47BCF98,
0x004D7E02,
0x43EE0,
0x122BB,
UO_SP,
};

COD_Classic_Version COD_SP = {
{0x0046C5C0,0x83EC8B55},
"uo_cgame_mp_x86.dll",
"uo_ui_mp_x86.dll",
0x452190,
0x3FFC0,
0x43D9E0,
0x0489A0A4,
0x4898F38,
0x004BF2B2,
0x58250,
0x1A96B,
UO_MP,
};

COD_Classic_Version *LoadedGame = NULL;

bool GetGameScreenRes(vector2& res) {
    if (!LoadedGame)
        return false;
    res.x = (float)*(uint32_t*)(LoadedGame->X_res_Addr);
    res.y = (float)*(uint32_t*)(LoadedGame->X_res_Addr + 0x4);
    return true;
}

#define CGAME_OFF(x) (cg_game_offset + (x - 0x30000000))
#define GAME_OFF(x) (game_offset + (x - 0x20000000))

#define UI_OFF(x) (ui_offset + (x - 0x40000000))

uintptr_t cg(uintptr_t CODUOSP, uintptr_t CODUOMP) {

    if (cg_game_offset == NULL)
        return NULL;

    uintptr_t result = 0;

    if (LoadedGame == &COD_SP) {
        result = CODUOMP;
    }
    else result = CODUOSP;

    if (result >= 0x30000000) {
        result = CGAME_OFF(result);
    }
    return result;

}

uintptr_t ui(uintptr_t CODUOSP, uintptr_t CODUOMP) {

    if (ui_offset == NULL)
        return NULL;

    uintptr_t result = 0;

    if (LoadedGame == &COD_SP) {
        result = CODUOMP;
    }
    else result = CODUOSP;

    if (result >= 0x40000000) {
        result = UI_OFF(result);
    }
    return result;

}

uintptr_t g(uintptr_t CODUOSP, uintptr_t CODUOMP) {

    if (ui_offset == NULL)
        return NULL;

    uintptr_t result = 0;

    if (LoadedGame == &COD_SP) {
        result = CODUOMP;
    }
    else result = CODUOSP;

    if (result >= 0x20000000) {
        result = GAME_OFF(result);
    }
    return result;

}



uintptr_t exe(uintptr_t CODUOSP, uintptr_t CODUOMP) {

    if (!LoadedGame)
        return NULL;

    if (LoadedGame->game == UO_SP)
        return CODUOSP;

    if (LoadedGame->game == UO_MP)
        return CODUOMP;

    return NULL;

}


uintptr_t sp_mp(uintptr_t CODUOSP, uintptr_t CODUOMP) {

    if (!LoadedGame)
        return NULL;

    if (LoadedGame->game == UO_SP)
        return CODUOSP;

    if (LoadedGame->game == UO_MP)
        return CODUOMP;

    return NULL;

}

cvar_s* __cdecl Cvar_Find(const char* a1) {
    return cdecl_call<cvar_s*>(exe(0x00433700,0x0043D8F0),a1);
}

void trap_R_SetColor(float* rgba) {
    auto ptr = cg(0x3002A750);
    if (ptr)
        cdecl_call<void>(ptr, rgba);
}

SafetyHookInline* CG_GetViewFov_og_S{};
constexpr auto STANDARD_ASPECT = 1.33333333333f;
void OpenConsole()
{
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONIN$", "r", stdin);
    SetConsoleTitleA(MOD_NAME " console");
    std::cout << "Console initialized.\n";
    printf("Oh,Hi Mark\n");
}
float GetAspectRatio() {
    float x = (float)*(int*)LoadedGame->X_res_Addr;
    float y = (float)*(int*)(LoadedGame->X_res_Addr + 0x4);

    //printf("aspect ratio %f\n", x / y);

    return x / y;
}

double GetAspectRatio_standardfix() {
    return (GetAspectRatio() / STANDARD_ASPECT);
}

double ApplyFOVScale(double fov, double fovScale, bool useFixedAspect, bool dofovScaleMath) {
    if (dofovScaleMath && fovScale == 0.0) {
        fovScale = cg_fovscale->value;
    }

    if (fovScale <= 0.0) {
        return fov; // Invalid scale, return original
    }

    if (!dofovScaleMath)
        fovScale = 1.0;

    if (dofovScaleMath)
        fov *= fovScale;

    // Convert FOV to radians
    double halfFovRad = (fov / 2.0) * (M_PI / 180.0);
    double tanHalfFov = tan(halfFovRad);

    if (useFixedAspect) {
        // Convert horizontal FOV to vertical, then back to horizontal with new aspect ratio
        // Convert to vertical FOV (aspect-independent)
        double tanHalfVFov = tanHalfFov / STANDARD_ASPECT;
        // Apply fovscale to the tangent



        double newTanHalfFov = tanHalfVFov * GetAspectRatio();
        // Convert back to degrees
        return (2.0 * atan(newTanHalfFov) * (180.0 / M_PI));
    }
    else {

        return (2.0 * atan(tanHalfFov) * (180.0 / M_PI));
    }
}

double CG_GetViewFov_hook() {
    double fov = CG_GetViewFov_og_S->call<double>();

    // Apply minimum FOV constraint
    if (cg_fovMin && cg_fovMin->base && (cg_fovMin->base->value - fov) > 0.f) {
        fov = cg_fovMin->base->value;
    }

    // Apply FOV scaling
    if (cg_fovscale && cg_fovscale->value) {
        bool useFixedAspect = cg_fixedAspectFOV && cg_fixedAspectFOV->integer;
        fov = ApplyFOVScale(fov, cg_fovscale->value, useFixedAspect,true);
    }

    // Validate result
    if (std::isnan(fov)) {
        return cg_fov != NULL ? cg_fov->value : 80.f;
    }

    return fov;
}

int __stdcall StringContains(const char* SubStr, const char* StringBuffer)
{
    unsigned int v2; // edi
    const char* Str_1; // esi

    v2 = strlen(SubStr);
    Str_1 = strstr(StringBuffer, SubStr);
    if (!Str_1)
        return 0;
    while (Str_1 != StringBuffer && !isspace(*(Str_1 - 1)) || !isspace(Str_1[v2]) && Str_1[v2])
    {
        Str_1 = strstr(Str_1 + 1, SubStr);
        if (!Str_1)
            return 0;
    }
    return 1;
}

void CheckModule()
{
    HMODULE hMod = GetModuleHandleA(LoadedGame->cgamename);
    if (hMod)
    {
        std::cout << "cgamex86.dll is attached at address: " << hMod << std::endl;
        codDLLhooks(hMod);
    }
    else
    {
        std::cout << "cgamex86.dll is NOT attached.\n";
    }
}


SafetyHookInline originalLoadDLLd;
HMODULE __cdecl hookCOD_dllLoad(const char* a1, FARPROC* a2, int a3) {
    HMODULE result = originalLoadDLLd.ccall<HMODULE>(a1, a2, a3);
    CheckModule();
   // printf("0x%X \n", (int)result);
    return result;
}

COD_Classic_Version* CheckGame() {
    // Get the WinMain address value for comparison
    DWORD* winMainAddr;
    DWORD winMainValue;

    // Check for COD_UO_SP
    winMainAddr = (DWORD*)COD_UO_SP.WinMain_Check[0];
    winMainValue = *winMainAddr;

    if (winMainValue == COD_UO_SP.WinMain_Check[1]) {
        LoadedGame = &COD_UO_SP;
        return &COD_UO_SP;
    }

    // Check for COD_SP
    winMainAddr = (DWORD*)COD_SP.WinMain_Check[0];
    winMainValue = *winMainAddr;

    if (winMainValue == COD_SP.WinMain_Check[1]) {
        LoadedGame = &COD_SP;
        return &COD_SP;
    }

    // If no match found, return NULL
    return NULL;
}

void SetUpFunctions() {
    Cvar_GetPtr = (Cvar_GetT)LoadedGame->Cvar_Get_Addr;
}

int __stdcall glviewport_47BD978 (DWORD xo, DWORD yo , DWORD x, DWORD y) {

    return stdcall_call<int>(**(uintptr_t**)0x47BD978, 240, yo, x, y);

}


LPVOID GetModuleEndAddress(HMODULE hModule) {
    if (hModule == NULL) {
        return NULL;
    }


    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)hModule;


    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        return NULL;
    }


    PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hModule + pDosHeader->e_lfanew);


    if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE) {
        return NULL;
    }


    DWORD imageSize = pNtHeaders->OptionalHeader.SizeOfImage;


    return (LPVOID)((BYTE*)hModule + imageSize);
}

// For glOrtho - adjusts screen-space ortho projection
double process_widths(double width = 0) {
    if (cg_fixedAspect && !cg_fixedAspect->base->integer) {
        return 0.f;
    }
    float x = (float)*(int*)LoadedGame->X_res_Addr;
    float y = (float)*(int*)(LoadedGame->X_res_Addr + 0x4);

    // orthoWidth = (x² / y²) × (3/4) × y = (x² × 3) / (4y)
    float orthoWidth = (x * x * 3.0f) / (4.0f * y);

    width += (orthoWidth - x) / 2.0f;

    return width;
}

// For game functions - adjusts game's internal 480-based coordinate system
double process_width(double width = 0) {

    if (cg_fixedAspect && !cg_fixedAspect->base->integer) {
        return 0.f;
    }

    float x = (float)*(int*)LoadedGame->X_res_Addr;
    float y = (float)*(int*)(LoadedGame->X_res_Addr + 0x4);

    // Same formula but scaled to 480-height coordinate space
    float aspect = x / y;
    width += 480.0f * aspect - 640.0f;

    return width;
}

int process_width(int width) {
    return (int)process_width((double)0.f);
}

SafetyHookInline LoadLibraryD;
void game_hooks(HMODULE handle);
HMODULE __stdcall LoadLibraryHook(const char* filename) {

    auto hModule = LoadLibraryD.unsafe_stdcall<HMODULE>(filename);

    if (strstr(filename, LoadedGame->cgamename) != NULL) {
        codDLLhooks(hModule);
        component_loader::post_cgame();
    }
    else if (strstr(filename, LoadedGame->uixname) != NULL) {
        ui_hooks(hModule);
        component_loader::post_ui();
    }
    else if (!strcmp(filename,"uo_gamex86.dll") && sp_mp(1)) {
        game_hooks(hModule);
        component_loader::post_game_sp();
    }
    else if (!strcmp(filename, "opengl32") || !strcmp(filename, "opengl32.dll")) {
        game_hooks(hModule);
        component_loader::on_ogl_load(hModule);
    }

    return hModule;

}


SafetyHookInline FreeLibraryD;

bool is_shutdown = false;

bool isShuttingDown() {
    return is_shutdown;
}

BOOL __stdcall FreeLibraryHook(HMODULE hLibModule) {
    if (!is_shutdown) {
        char LibraryName[256]{};
        GetModuleFileNameA(hLibModule, LibraryName, sizeof(LibraryName));

        if (LoadedGame && LoadedGame->cgamename && (strstr(LibraryName, LoadedGame->cgamename) != 0)) {
            cg_game_offset = 0;
        } else if (LoadedGame && LoadedGame->cgamename && (strstr(LibraryName, "uo_gamex86.dll") != 0)) {
            game_offset = 0;
        }
        else if (LoadedGame && LoadedGame->cgamename && (strstr(LibraryName, LoadedGame->uixname) != 0)) {
            ui_offset = 0;
        }

        uintptr_t moduleStart = (uintptr_t)hLibModule;
        uintptr_t moduleEnd = (uintptr_t)GetModuleEndAddress(hLibModule);

        if (moduleEnd != 0) {
            // Check inline hooks
            for (auto& hookPtr : g_inlineHooks) {
                if (hookPtr) {
                    uintptr_t targetAddr = (uintptr_t)hookPtr->target_address();
                    if (targetAddr >= moduleStart && targetAddr < moduleEnd) {
                        hookPtr->reset();
                    }
                }
            }

            // Check mid hooks
            for (auto& hookPtr : g_midHooks) {
                if (hookPtr) {
                    uintptr_t targetAddr = (uintptr_t)hookPtr->target_address();
                    if (targetAddr >= moduleStart && targetAddr < moduleEnd) {
                        hookPtr->reset();
                    }
                }
            }

            // Clear out any reset hooks
            g_inlineHooks.erase(
                std::remove_if(g_inlineHooks.begin(), g_inlineHooks.end(),
                    [](const std::unique_ptr<SafetyHookInline>& h) { return !h || !(*h); }),
                g_inlineHooks.end()
            );

            g_midHooks.erase(
                std::remove_if(g_midHooks.begin(), g_midHooks.end(),
                    [](const std::unique_ptr<SafetyHookMid>& h) { return !h || !(*h); }),
                g_midHooks.end()
            );

        }
    }
    auto hModule = FreeLibraryD.unsafe_stdcall<BOOL>(hLibModule);
    return hModule;
}

void ShutdownAllHooks() {
    for (auto& hookPtr : g_inlineHooks) {
        if (hookPtr) {
            hookPtr->reset();
        }
    }

    for (auto& hookPtr : g_midHooks) {
        if (hookPtr) {
            hookPtr->reset();
        }
    }


    g_inlineHooks.clear();
    g_midHooks.clear();

    cg_game_offset = 0;
    game_offset = 0;
    ui_offset = 0;
}

float get_safeArea_horizontal() {
    if (cg_fixedAspect && !cg_fixedAspect->base->integer) {
        return 1.f;
    }
    float printing = safeArea_horizontal != 0 ? safeArea_horizontal->value : 1.f;

    //printf("SafeArea cvar ptr %p %f\n", safeArea_horizontal, printing);

    if (!safeArea_horizontal)
        return 1.f;
    return std::clamp(safeArea_horizontal->value, 0.f, 1.f);
}



float get_safeArea_vertical_hack() {
    if (cg_fixedAspect && !cg_fixedAspect->base->integer) {
        return 0.f;
    }
    if (!safeArea_vertical)
        return 0.f;

    // Clamp first, then invert
    float clamped = std::clamp(safeArea_vertical->value, 0.f, 1.f);
    return 1.f - clamped;
}

int resolution_modded[2];

void StaticInstructionPatches(cvar_s* safeArea_horizontal_ptr = safeArea_horizontal, bool DLLLoad = true);

float* cg_drawupperright_x;

void set_cg_drawupperright_x_wide(bool is_wide) {
    if (!cg_game_offset || !cg_drawupperright_x)
        return;
    if (is_wide)
        *cg_drawupperright_x = 620.f + (process_width() * 0.5f) * get_safeArea_horizontal();
    else
        *cg_drawupperright_x = 620.f;
}

void Resolution_Static_mod(cvar_s* cvar, const char* old_value = "") {
    int* res = (int*)LoadedGame->X_res_Addr;
    if (cvar && cvar->integer) {
        resolution_modded[0] = res[1] * STANDARD_ASPECT;
    }
    else {
        resolution_modded[0] = res[0];
    }

}

void Resolution_Static_mod_cb(cvar_s* cvar, const char* old_value = "") {
    Resolution_Static_mod(cvar, old_value);
    StaticInstructionPatches(NULL, false);
}

float process_height_hack_safe() {

    return 240.f * get_safeArea_vertical_hack();

}

//
//SafetyHookInline qglViewport_og{};
//
//void __stdcall qglViewport_detour(int x,
//    int y,
//    unsigned int width,
//    unsigned int height) {
//
//    if (_ReturnAddress() == (void*)0x4D7D96) {
//        x = 240;
//        width = 1440;
//    }
//    qglViewport_og.unsafe_stdcall(x, y, width, height);
//};
//
//
//SafetyHookInline qglScissor_og{};
//
//void __stdcall qglScisso_detour(int x,
//    int y,
//    unsigned int width,
//    unsigned int height) {
//
//    if (_ReturnAddress() == (void*)0x004D7DAD) {
//        x = 240;
//        width = 1440;
//    }
//    qglScissor_og.unsafe_stdcall(x, y, width, height);
//};

uintptr_t* glOrtho_ptr;
int __stdcall glOrtho_detour(double left, double right, double bottom, double top, double zNear, double zFar) {
    left -= process_widths();
    right += process_widths();
    return stdcall_call<int>(*glOrtho_ptr, left, right, bottom, top, zNear, zFar);
}

uintptr_t InsideWinMain;

uintptr_t cvar_init_og;

void PrintRegisteredCvars() {
    Com_Printf("Registered QOL cvars (%d total):\n", QOL_registered_cvars.size());
    for (const char* cvar : QOL_registered_cvars) {
        Com_Printf("  %s\n", cvar);
    }
}

int Cvar_Init_hook() {

    component_loader::post_unpack();

    int* size_cvars = (int*)0x4805EC0;
    r_qol_texture_filter_anisotropic = Cevar_Get("r_qol_texture_filter_anisotropic", 16, CVAR_ARCHIVE | CVAR_LATCH, 1, 16);
    printf("cvar_init cvar hooks cvar_get ptr %p size %d\n", Cvar_Get, *size_cvars);
    auto result = cdecl_call<int>(cvar_init_og);


    cg_fovMin = Cevar_Get("cg_fovMin", 1.f, CVAR_ARCHIVE, 1.f, 160.f);

    Cevar_Get("cg_fov", 80.f, CVAR_ARCHIVE, 1.f, 160.f);

    rinput::raw_input = Cevar_Get("m_rawinput", is_wine() ? 0 : 1, CVAR_ARCHIVE, 0, 1);

    cg_fovscale = Cvar_Get((char*)"cg_fovscale", "1.0", CVAR_ARCHIVE);
    cg_fovscale_ads = Cvar_Get((char*)"cg_fovscale_ads", "1.0", CVAR_ARCHIVE);

    cg_fov_fix_lowfovads = Cevar_Get((char*)"cg_fov_fix_lowfovads", 0, CVAR_ARCHIVE, 0, 2);
    cg_fixedAspectFOV = Cvar_Get((char*)"cg_fixedAspectFOV", "1", CVAR_ARCHIVE);
    cg_fixedAspect = Cevar_Get((char*)"cg_fixedAspect", 1, CVAR_ARCHIVE, 0, 3, Resolution_Static_mod_cb);
    safeArea_horizontal = Cvar_Get((char*)"safeArea_horizontal", "1.0", CVAR_ARCHIVE);
    safeArea_vertical = Cvar_Get((char*)"safeArea_vertical", "1.0", CVAR_ARCHIVE);
    printf("safearea ptr return %p size after %d\n", safeArea_horizontal, *size_cvars);
    r_noborder = Cvar_Get((char*)"r_noborder", "0", CVAR_ARCHIVE);
    r_mode_auto = Cevar_Get((char*)"r_mode_auto", 0, CVAR_ARCHIVE | CVAR_LATCH, 0, 1);
    cg_fixedAspect_blackshader_all = Cevar_Get((char*)"cg_fixedAspect_blackshader_all", 1, CVAR_ARCHIVE, 0, 1);
    player_sprintmult = Cvar_Get("player_sprintmult", "0.66666669", CVAR_CHEAT);

    if (sp_mp(1)) {
        //g_save_allowbadchecksum = Cvar_Get("g_save_allowbadchecksum", "0", 1);
    }

    cg_hudelem_alignhack = Cevar_Get("cg_hudelem_alignhack", 0, CVAR_ARCHIVE, 0);

    cg_hudelem_printnames = Cevar_Get("cg_hudelem_printnames", 0, CVAR_CHEAT,0,1);

    game::Cmd_AddCommand("qol_showallcvars", PrintRegisteredCvars);

    return result;
}

void LoadMenuConfigs();



SafetyHookMid* DrawHudElemMaterial_mid{};

uintptr_t DrawStretch_og{};

int __cdecl DrawStretch_ui(float x, float y, float width, float height, float* a5) {

    int result = 0;
    x -= process_width();
    width += process_width() * 2.0f;
    auto ptr = ui(0x40007960);
    if (ptr) {
        result = cdecl_call<int>(ptr, x, y, width, height, a5);
    }

    return result;
}

int __cdecl DrawStretch_stretched(float x, float y, float width, float height, float* a5) {

    int result;
    x -= process_width();
    width += process_width() * 2.0f;

    if (DrawStretch_og) {
        result = cdecl_call<int>(DrawStretch_og, x, y, width, height, a5);
    }

        return result;
}

int __cdecl DrawStretch_300135B0(float x, float y, float width, float height, int a5) {

    x -= process_width();
    width += process_width() * 2.0f;

    //printf("this is called \n");

    return cdecl_call<int>(DrawStretch_og, x, y, width, height, a5);
}

uintptr_t trap_R_DrawStretchPic{};

float whatevers[4]{};


void __cdecl R_DrawStretchPic_leftsniper(float x, float y, float width, float height, float unk1, float unk2, float unk3, float unk4, int image_id) {


    x = -process_widths(0.f);
    width += process_widths(0.f);
    if(trap_R_DrawStretchPic)
    cdecl_call<void>(trap_R_DrawStretchPic, x, y, width, height, unk1, unk2, unk3, unk4, image_id);
}

void __cdecl R_DrawStretchPic_rightsniper(float x, float y, float width, float height, float unk1, float unk2, float unk3, float unk4, int image_id) {




    width += process_widths(0.f);

    cdecl_call<void>(trap_R_DrawStretchPic, x, y, width, height, unk1, unk2, unk3, unk4, image_id);
}

uintptr_t crosshair_render_func;

uintptr_t CG_DrawPicAddr;

int __cdecl CG_DrawPic(float a1, float a2, float a3, float a4, int a5) {
    if (CG_DrawPicAddr) {
        return cdecl_call<int>(CG_DrawPicAddr, a1, a2, a3, a4, a5);
    }
    return -1;
}
static float adjustments[10];

float* cg_screenXScale;
float* cg_screenYScale;
void CG_AdjustFrom640(float* x, float* y, float* w, float* h) {
    if(x)
    *x *= *cg_screenXScale;
    if(y)
    *y *= *cg_screenYScale;
    if (cg_fixedAspect && cg_fixedAspect->base->integer) {
        if (w)
            *w *= *cg_screenXScale;
        if (h)
            *h *= *cg_screenYScale;
    }
}

void CG_AdjustFrom640(float& x, float& y, float& w, float& h) {
    CG_AdjustFrom640(&x,&y,&w,&h);
}

vector2* fov_world;
void _cdecl crosshair_render_hook(float x, float y, float width, float height, int unk1, float u1, float u2, float v1, float rotation, int shaderHandle) {
    int side;
    __asm mov side, esi

    if (!(cg_fixedAspect && cg_fixedAspect->base->integer))
    {
        CG_AdjustFrom640(&x, &y, &width, &height);
        cdecl_call<void>(crosshair_render_func, x, y, width, height, unk1, u1, u2, v1, rotation, shaderHandle);
        return;
    }

    // top (0), right (1), bottom (2), left (3)
    const bool is_vertical = (side == 0 || side == 2);

    static float crosshair_center_x = 0.0f;
    static float crosshair_center_y = 0.0f;
    static float crosshair_radius = 0.0f;

    if (is_vertical)
    {
        CG_AdjustFrom640(&x, &y, &width, &height);

        if (side == 0)
        {
            const float screen_center_y = (float)*(int*)(LoadedGame->X_res_Addr + 0x4) * 0.5f;

            crosshair_center_x = x + width * 0.5f;
            crosshair_center_y = screen_center_y;
            crosshair_radius = std::fabs(screen_center_y - (y + height * 0.5f));
        }
        else
        {
            x = crosshair_center_x - width * 0.5f;
            y = crosshair_center_y + crosshair_radius - height * 0.5f;
        }
    }
    else
    {
        CG_AdjustFrom640(&x, &y, &height, &width);

        y = crosshair_center_y - height * 0.5f;

        const float sx = *cg_screenXScale;
        const float sy = *cg_screenYScale;
        const float widescreen_factor = (sx > sy + 0.0001f) ? (sx / sy) : 1.0f;
        const float horiz_distance = crosshair_radius * widescreen_factor;

        x = crosshair_center_x + (side == 1 ? horiz_distance : -horiz_distance) - width * 0.5f;
    }

    cdecl_call<void>(crosshair_render_func, x, y, width, height, unk1, u1, u2, v1, rotation, shaderHandle);
}

SafetyHookInline* Item_Paint_cg{};

SafetyHookInline* Item_Paint_ui{};

#define ALIGN_LEFT_BOTTOM   "[LB]"
#define ALIGN_LEFT_TOP      "[LT]"
#define ALIGN_RIGHT_BOTTOM  "[RB]"
#define ALIGN_RIGHT_TOP     "[RT]"
#define ALIGN_CENTER        "[CENTER]"
#define ALIGN_LEFT          "[LEFT]"
#define ALIGN_RIGHT         "[RIGHT]"
#define ALIGN_TOP           "[TOP]"
#define ALIGN_BOTTOM        "[BOTTOM]"
#define ALIGN_STRETCH       "[STRETCH]"
#define ALIGN_BLACKSCREEN   "[BLACKSCREEN]"

// ============================================================================
// ALIGNMENT PROCESSING FUNCTION
// ============================================================================
struct AlignmentState {
    const char* originalText;
    rectDef_t originalRect;
    bool wasModified;
};

bool ProcessItemAlignment(itemDef_t* item, char* textBuffer, size_t bufferSize, AlignmentState* state) {
    if (!item->text) return false;

    const char* text = item->text;
    state->originalText = text;
    state->originalRect = item->window.rect;
    state->wasModified = false;

    float halfWidth = process_width() * 0.5f;
    float safeX = get_safeArea_horizontal();

    // Helper macro to check and process alignment
#define CHECK_ALIGNMENT(marker, xOffset) \
        if (strncmp(text, marker, strlen(marker)) == 0) { \
            const char* remainingText = text + strlen(marker); \
            if (remainingText[0] != '\0') { \
                strcpy_s(textBuffer, bufferSize, remainingText); \
                item->text = textBuffer; \
            } else { \
                item->text = nullptr; \
            } \
            item->window.rect.x += (xOffset); \
            state->wasModified = true; \
            return true; \
        }

    // Check each alignment marker
    CHECK_ALIGNMENT(ALIGN_LEFT_BOTTOM, (-halfWidth) * safeX)
        CHECK_ALIGNMENT(ALIGN_LEFT_TOP, (-halfWidth) * safeX)
        CHECK_ALIGNMENT(ALIGN_RIGHT_BOTTOM, (halfWidth)*safeX)
        CHECK_ALIGNMENT(ALIGN_RIGHT_TOP, (halfWidth)*safeX)
        CHECK_ALIGNMENT(ALIGN_LEFT, (-halfWidth) * safeX)
        CHECK_ALIGNMENT(ALIGN_RIGHT, (halfWidth)*safeX)
        CHECK_ALIGNMENT(ALIGN_CENTER, 0.0f)
        CHECK_ALIGNMENT(ALIGN_TOP, 0.0f)
        CHECK_ALIGNMENT(ALIGN_BOTTOM, 0.0f)

#undef CHECK_ALIGNMENT

        return state->wasModified;
}

void RestoreItemState(itemDef_t* item, const AlignmentState* state) {
    if (state->wasModified) {
        item->text = state->originalText;
        item->window.rect = state->originalRect;
    }
}


// ============================================================================
// JSON CONFIGURATION STRUCTURE
// ============================================================================
struct Alignment {
    uint32_t h_left : 1;
    uint32_t h_right : 1;
    uint32_t h_center : 1;
    uint32_t v_top : 1;
    uint32_t v_bottom : 1;
    uint32_t v_center : 1;
    uint32_t stretch : 1;   // For stretch behavior
    uint32_t black_screen : 1;
    uint32_t _unused : 24;  // Padding to 32 bits
};

struct MenuConfig {
    std::string menuName;
    Alignment alignment;
};

// Global storage for all menu configs
std::vector<MenuConfig> g_menuConfigs;

// ============================================================================
// JSON PARSING - SUPPORTS MULTIPLE MENUS PER FILE
// ============================================================================

Alignment ParseAlignment(const std::string& alignStr) {
    Alignment align = { 0 };

    if (alignStr == ALIGN_LEFT) {
        align.h_left = 1;
    }
    else if (alignStr == ALIGN_RIGHT) {
        align.h_right = 1;
    }
    else if (alignStr == ALIGN_CENTER) {
        align.h_center = 1;
        align.v_center = 1;
    }
    else if (alignStr == ALIGN_LEFT_BOTTOM) {
        align.h_left = 1;
        align.v_bottom = 1;
    }
    else if (alignStr == ALIGN_LEFT_TOP) {
        align.h_left = 1;
        align.v_top = 1;
    }
    else if (alignStr == ALIGN_RIGHT_BOTTOM) {
        align.h_right = 1;
        align.v_bottom = 1;
    }
    else if (alignStr == ALIGN_RIGHT_TOP) {
        align.h_right = 1;
        align.v_top = 1;
    }
    else if (alignStr == ALIGN_TOP) {
        align.v_top = 1;
    }
    else if (alignStr == ALIGN_BOTTOM) {
        align.v_bottom = 1;
    }
    else if (alignStr == ALIGN_STRETCH) {
        align.stretch = 1;
    }
    else if (alignStr == ALIGN_BLACKSCREEN) {
        align.black_screen = 1;
    }

    return align;
}

static std::unordered_map<std::string, const MenuConfig*> g_menuConfigLookup;
void LoadMenuConfigs() {
    char modulePath[MAX_PATH];
    GetModuleFileNameA(NULL, modulePath, MAX_PATH);

    std::filesystem::path exePath(modulePath);
    std::filesystem::path menuwideDir = exePath.parent_path() / "menuwide";

    if (!std::filesystem::exists(menuwideDir)) {
        printf("menuwide directory not found: %s\n", menuwideDir.string().c_str());
        return;
    }

    printf("Loading menu configs from: %s\n", menuwideDir.string().c_str());

    for (const auto& entry : std::filesystem::directory_iterator(menuwideDir)) {
        if (entry.path().extension() != ".json") continue;

        if (entry.path().filename() == "_hudelem_shaders.json") {
            printf("Skipping _hudelem_shaders.json (handled separately)\n");
            continue;
        }

        try {
            std::ifstream file(entry.path());
            nlohmann::json j = nlohmann::json::parse(file);

            if (j.is_array()) {
                for (const auto& menuJson : j) {
                    MenuConfig config;
                    config.menuName = menuJson["menuName"].get<std::string>();
                    config.alignment = ParseAlignment(menuJson["alignment"].get<std::string>());

                    g_menuConfigs.push_back(config);
                    printf("Loaded config for menu '%s'\n", config.menuName.c_str());
                }
            }
            else {
                MenuConfig config;
                config.menuName = j["menuName"].get<std::string>();
                config.alignment = ParseAlignment(j["alignment"].get<std::string>());

                g_menuConfigs.push_back(config);
                printf("Loaded config for menu '%s'\n", config.menuName.c_str());
            }
        }
        catch (const std::exception& e) {
            printf("Failed to parse %s: %s\n",
                entry.path().string().c_str(), e.what());
        }
    }

    g_menuConfigLookup.clear();
    g_menuConfigLookup.reserve(g_menuConfigs.size());

    for (const auto& config : g_menuConfigs) {
        std::string lowerName = config.menuName;
        for (char& c : lowerName) {
            c = std::tolower(static_cast<unsigned char>(c));
        }
        g_menuConfigLookup[lowerName] = &config;
    }

}

// ============================================================================
// LOOKUP FUNCTIONS
// ============================================================================
const MenuConfig* FindMenuConfig(const char* menuName) {
    if (!menuName) return nullptr;

    std::string lowerName = menuName;
    for (char& c : lowerName) {
        c = std::tolower(static_cast<unsigned char>(c));
    }

    auto it = g_menuConfigLookup.find(lowerName);
    return (it != g_menuConfigLookup.end()) ? it->second : nullptr;
}

// ============================================================================
// ALIGNMENT PROCESSING - JSON VERSION
// ============================================================================
bool ProcessItemAlignment_FromJSON(itemDef_t* item, const char* menuName,
    AlignmentState* state) {
    if (!menuName) return false;

    state->originalText = item->text;
    state->originalRect = item->window.rect;
    state->wasModified = false;

    const MenuConfig* config = FindMenuConfig(menuName);
    if (!config) return false;

    float halfWidth = process_width() * 0.5f;
    float safeX = get_safeArea_horizontal();

    float height_hack = process_height_hack_safe() * 0.5f;


    if (config->alignment.h_left) {
        item->window.rect.x += (-halfWidth) * safeX;
        state->wasModified = true;
    }
    else if (config->alignment.h_right) {
        item->window.rect.x += (halfWidth)*safeX;
        state->wasModified = true;
    }
    else if (config->alignment.h_center) {
        state->wasModified = false;
    }

    if (config->alignment.v_top) {
         item->window.rect.y += height_hack;
        state->wasModified = true;
    }
    else if (config->alignment.v_bottom) {
        item->window.rect.y -= height_hack;
        state->wasModified = true;
    }
    else if (config->alignment.v_center) {
        // No Y adjustment for center
        state->wasModified = false;
    }

    return state->wasModified;
}


void __fastcall Item_Paint_Hook(itemDef_t* item, SafetyHookInline* hook, bool ui = false) {
    char textBuffer[1024];
    AlignmentState state = { 0 };

    const char* menuName = nullptr;
    menuDef_t* menu = nullptr;
    // Get menu name
    auto parent_ptr = sp_mp((uintptr_t)item->parent, item->textSavegameInfo);
    if (parent_ptr) {
        menu = (menuDef_t*)parent_ptr;
        if (menu && menu->window.name) {
            menuName = menu->window.name;
            //printf("menuName %s\n", menuName);
        }
    }

    // Try JSON-based alignment first (applies to entire menu)
    bool processedFromJSON = ProcessItemAlignment_FromJSON(item, menuName, &state);

    // If not found in JSON, fall back to text marker processing (per-item)
    if (!processedFromJSON) {
        ProcessItemAlignment(item, textBuffer, sizeof(textBuffer), &state);
    }


    auto config = FindMenuConfig(menuName);


    if (item && item->window.name) {
        //printf("item %s\n", item->window.name);
    }

    if (config && menu && menu->items && menu->items[0] == item /*|| item && item->window.name && (strcmp(item->window.name,"main_back_top") == 0)*/ ) {
        if (config && config->alignment.black_screen /*|| item && item->window.name && (strcmp(item->window.name, "main_back_top") == 0)*/) {
            static float black[4] = { 0.f,0.f,0.f,1.f };
            ui ? DrawStretch_ui(0.f, 0.f, 640.f, 480.f, black) : DrawStretch_stretched(0.f, 0.f, 640.f, 480.f, black);
        }
    }
    // Call original render
    hook->unsafe_thiscall(item);



    // Restore original state
    RestoreItemState(item, &state);
}

void __fastcall Item_Paint_cg_f(itemDef_t* item) {
    Item_Paint_Hook(item, Item_Paint_cg,false);
}

void __fastcall Item_Paint_ui_f(itemDef_t* item) {
    Item_Paint_Hook(item, Item_Paint_ui,true);
}

uintptr_t DrawSingleHudElem2dog = 0;

struct HudAlignmentState {
    alignx_e originalAlignX;
    aligny_e originalAlignY;
    float originalX;
    float originalY;
    bool wasModified;
};

void ProcessHudElemAlignment(hudelem_s* hud, HudAlignmentState* state) {
    if (!hud || !cg_hudelem_alignhack || !cg_hudelem_alignhack->base || !cg_hudelem_alignhack->base->integer) return;

    // Save original state
    state->originalAlignX = hud->alignx;
    state->originalAlignY = hud->aligny;
    state->originalX = (float)hud->x;
    state->originalY = (float)hud->y;
    state->wasModified = false;

    float halfWidth = process_width() * 0.5f;
    float safeX = get_safeArea_horizontal();
    // float halfHeight = process_height() * 0.5f;
    float height_hack = process_height_hack_safe() * 0.5f;

    // Process horizontal alignment (alignx)
    switch (hud->alignx) {
    case ALIGNX_LEFT:
        hud->x += (int32_t)((-halfWidth) * safeX);
        state->wasModified = true;
        break;

    case ALIGNX_RIGHT:
        hud->x += (int32_t)((halfWidth)*safeX);
        state->wasModified = true;
        break;

    case ALIGNX_CENTER:
        // No X adjustment for center
        state->wasModified = false;
        break;
    }
    // required otherwise will break "black" shader
    if ((hud->width < 640 && hud->height < 480)) {
        switch (hud->aligny) {
        case ALIGNY_TOP:
            hud->y += height_hack;
            state->wasModified = true;
            break;

        case ALIGNY_BOTTOM:
            hud->y -= height_hack;
            state->wasModified = true;
            break;

        case ALIGNY_MIDDLE:

            state->wasModified = false;
            break;
        }
    }
}

void RestoreHudElemState(hudelem_s* hud, const HudAlignmentState* state) {
    if (state->wasModified) {
        hud->alignx = state->originalAlignX;
        hud->aligny = state->originalAlignY;
        hud->x = (int32_t)state->originalX;
        hud->y = (int32_t)state->originalY;
    }
}

// ============================================================================
// UPDATED HOOK FUNCTION
// ============================================================================

int __fastcall DrawSingleHudElem2dHook(hudelem_s* thisa) {
    HudAlignmentState state = { };

    if (thisa) {

        ProcessHudElemAlignment(thisa, &state);
    }

    // Call original render
    auto result = thiscall_call<int>(DrawSingleHudElem2dog, thisa);

    // Restore original state
    if (thisa) {
        RestoreHudElemState(thisa, &state);
    }

    return result;
}

struct HudShaderConfig {
    std::string shaderName;
    Alignment alignment;
};

// Global storage for shader configs
std::vector<HudShaderConfig> g_hudShaderConfigs;
static std::unordered_map<std::string, const HudShaderConfig*> g_hudShaderLookup;
void LoadHudShaderConfigs() {
    char modulePath[MAX_PATH];
    GetModuleFileNameA(NULL, modulePath, MAX_PATH);

    std::filesystem::path exePath(modulePath);
    std::filesystem::path configPath = exePath.parent_path() / "menuwide" / "_hudelem_shaders.json";

    if (!std::filesystem::exists(configPath)) {
        printf("_hudelem_shaders.json not found: %s\n", configPath.string().c_str());
        return;
    }

    printf("Loading HUD shader configs from: %s\n", configPath.string().c_str());

    try {
        std::ifstream file(configPath);
        nlohmann::json j = nlohmann::json::parse(file);

        if (j.is_array()) {
            for (const auto& shaderJson : j) {
                HudShaderConfig config;
                config.shaderName = shaderJson["shaderName"].get<std::string>();
                config.alignment = ParseAlignment(shaderJson["alignment"].get<std::string>());

                g_hudShaderConfigs.push_back(config);
                printf("Loaded shader config for '%s' (stretch: %d)\n",
                    config.shaderName.c_str(), config.alignment.stretch);
            }
        }
    }
    catch (const std::exception& e) {
        printf("Failed to parse _hudelem_shaders.json: %s\n", e.what());
    }

    g_hudShaderLookup.clear();
    g_hudShaderLookup.reserve(g_hudShaderConfigs.size());

    for (const auto& config : g_hudShaderConfigs) {
        std::string lowerName = config.shaderName;
        for (char& c : lowerName) {
            c = std::tolower(static_cast<unsigned char>(c));
        }
        g_hudShaderLookup[lowerName] = &config;
    }

}


const HudShaderConfig* FindHudShaderConfig(const char* shaderName) {
    if (!shaderName) return nullptr;

    std::string lowerName = shaderName;
    for (char& c : lowerName) {
        c = std::tolower(static_cast<unsigned char>(c));
    }

    auto it = g_hudShaderLookup.find(lowerName);
    return (it != g_hudShaderLookup.end()) ? it->second : nullptr;
}
SafetyHookMid* DrawObjectives;

SafetyHookMid* DrawMissionObjectives;

void StaticInstructionPatches(cvar_s* safeArea_horizontal_ptr, bool DLLLoad) {

    auto safe_x = get_safeArea_horizontal();

    auto width_x = process_width() * 0.5f;

    auto x = width_x * safe_x;
    auto menuConfig = FindMenuConfig("Compass");
    if (cg(0x30026DC6) && menuConfig && menuConfig->alignment.h_left) {
        Memory::VP::Patch<float>(cg(0x30026DC6) + 0x1, 21.f - x);
        Memory::VP::Patch<float>(cg(0x30026DDC) + 0x1, 20.f - x);
        Memory::VP::Patch<float>(cg(0x30026F8D) + 0x4, 20.f - x);
        Memory::VP::Patch<float>(cg(0x30026E17) + 0x4, 20.f - x);

        if (DLLLoad) {
            CreateMidHook(cg(0x30026F36), [](SafetyHookContext& ctx) {
                float& checkbox_x = *(float*)(ctx.esp);

                auto menuConfig = FindMenuConfig("Compass");
                if (menuConfig && menuConfig->alignment.h_left) {
                    // Get actual screen width from scale
                    float scale = *(float*)cg(0x3024B740);
                    float screen_width = scale * 640.0f;
                    float offset = (screen_width / 2.0f) - 640.0f;
                    offset *= get_safeArea_horizontal();


                    //printf("scale=%.2f, screen_width=%.2f, offset=%.2f, checkbox_x before=%.2f, after=%.2f\n",
                    //    scale, screen_width, offset, checkbox_x, checkbox_x - offset);

                    checkbox_x -= process_widths() * get_safeArea_horizontal();
                }
                });
        }

    }

}
const char* UNKNOWN = "BADSTR";
const char* __cdecl String_Alloc_ui(const char* string_to_alloc) {
    auto ptr = ui(0x4000D400);

    if (ptr) {
        return cdecl_call<const char*>(ptr, string_to_alloc);
    }
    return UNKNOWN;
}



typedef struct vidmode_s
{
    const char* description;
    int width, height;
    float pixelAspect;
} vidmode_t;

struct vidmode_menu
{
    const char* description;
    int r_mode_setting;
};

struct DisplayModeKey {
    int width;
    int height;
    int bpp;
    int freq;

    bool operator<(const DisplayModeKey& other) const {
        if (width != other.width) return width > other.width;
        if (height != other.height) return height > other.height;
        if (bpp != other.bpp) return bpp > other.bpp;
        return freq > other.freq;
    }
};

struct ResolutionKey {
    int width;
    int height;

    bool operator<(const ResolutionKey& other) const {
        if (width != other.width) return width > other.width; // Descending
        return height > other.height;
    }

    bool operator==(const ResolutionKey& other) const {
        return width == other.width && height == other.height;
    }
};

std::vector<vidmode_t> r_vidModes_dynamic;
std::vector<vidmode_menu> r_vidModes_menu_dynamic;
std::vector<std::string> mode_descriptions;
std::vector<std::string> menu_descriptions;

std::vector<char*> allocated_strings;

char* AllocateString(const char* str) {
    size_t len = strlen(str) + 1;
    char* newStr = new char[len];
    strcpy(newStr, str);
    allocated_strings.push_back(newStr);
    return newStr;
}

void InitializeDisplayModes() {
    DEVMODE devMode;
    int modeNum = 0;
    std::set<ResolutionKey> uniqueResolutions;

    memset(&devMode, 0, sizeof(DEVMODE));
    devMode.dmSize = sizeof(DEVMODE);

    // Enumerate all display modes
    while (EnumDisplaySettings(NULL, modeNum, &devMode)) {
        if (devMode.dmPelsWidth >= 640 &&
            devMode.dmPelsHeight >= 480 &&
            devMode.dmBitsPerPel >= 16) {

            ResolutionKey key = {
                (int)devMode.dmPelsWidth,
                (int)devMode.dmPelsHeight
            };
            uniqueResolutions.insert(key);
        }
        modeNum++;
    }

    // Convert to r_vidModes array (with "Mode X:" prefix)
    int modeIndex = 0;
    for (const auto& res : uniqueResolutions) {
        char desc[64];
        float aspect = (float)res.width / (float)res.height;
        bool isWide = (aspect > 1.5f);

        if (isWide) {
            sprintf(desc, "Mode %2d: %dx%d (wide)", modeIndex, res.width, res.height);
        }
        else {
            sprintf(desc, "Mode %2d: %dx%d", modeIndex, res.width, res.height);
        }

        vidmode_t vidMode = {
            AllocateString(desc),
            res.width,
            res.height,
            1.0f
        };
        r_vidModes_dynamic.push_back(vidMode);
        modeIndex++;
    }

    // Convert to menu array (just "WxH")

    int menuCount = min((int)uniqueResolutions.size(), MAX_MULTI_CVARS);

    auto it = uniqueResolutions.begin();
    for (int i = 0; i < menuCount; i++, ++it) {
        char menuDesc[32];
        sprintf(menuDesc, "%dx%d", it->width, it->height);

        vidmode_menu menuMode = {
            AllocateString(menuDesc),
            i
        };
        r_vidModes_menu_dynamic.push_back(menuMode);
    }

    if (!r_vidModes_menu_dynamic.empty()) {
        vidmode_menu Custom = { AllocateString("@MENU_CUSTOM"),-1 };
            
        r_vidModes_menu_dynamic.push_back(Custom);
    }

    printf("Initialized %d unique display modes (%d available for menu)\n",
        (int)r_vidModes_dynamic.size(), menuCount);

    // Debug print
    for (int i = 0; i < min(5, (int)r_vidModes_menu_dynamic.size()); i++) {
        printf("Menu[%d]: %s -> Mode %d\n", i,
            r_vidModes_menu_dynamic[i].description,
            r_vidModes_menu_dynamic[i].r_mode_setting);



        vidmode_t* r_vidModes = nullptr;
        vidmode_menu* r_vidModes_menu = nullptr;
        int r_vidModes_count = 0;
        int r_vidModes_menu_count = 0;
    }
}
vidmode_t* r_vidModes = nullptr;
vidmode_menu* r_vidModes_menu = nullptr;
int r_vidModes_count = 0;
int r_vidModes_menu_count = 0;

void InitializeDisplayModesForGame() {
    if (!sp_mp(1))
        return;
    InitializeDisplayModes();

    r_vidModes_count = r_vidModes_dynamic.size();
    r_vidModes = new vidmode_t[r_vidModes_count];
    memcpy(r_vidModes, r_vidModes_dynamic.data(), r_vidModes_count * sizeof(vidmode_t));

    r_vidModes_menu_count = r_vidModes_menu_dynamic.size();
    r_vidModes_menu = new vidmode_menu[r_vidModes_menu_count];
    memcpy(r_vidModes_menu, r_vidModes_menu_dynamic.data(), r_vidModes_menu_count * sizeof(vidmode_menu));

    auto r_mode_read = exe(0x425F9E + 2,0x4C177B + 2);
    if (r_mode_read) {
        Memory::VP::Patch<void*>(r_mode_read, r_vidModes);
        Memory::VP::Patch<void*>(exe(0x425FC4 + 2,0x4C17BD + 2), r_vidModes);
        if (sp_mp(1)) {
            Memory::VP::Patch<void*>(exe(0x4AA35B + 1), r_vidModes);
            Memory::VP::Patch<void*>(exe(0x4AA39D + 1), r_vidModes);
        }
        *(int*)exe(0x0057B578,0x5CE908) = r_vidModes_count;
    }
}

void ui_hooks(HMODULE handle) {
    uintptr_t OFFSET = (uintptr_t)handle;
    ui_offset = OFFSET;
    if (sp_mp(1)) {
        auto cvar_version = Cvar_Find("shortversion");

        if (cvar_version && cvar_version->string) {
            char buffer[128]{};
            sprintf_s(buffer, sizeof(buffer), "%s " MOD_NAME " r%d %s", cvar_version->string, BUILD_NUMBER, COMMIT_HASH);
            hook_shortversion = Cvar_Get("hook_shortversion", buffer, CVAR_ROM);
        }

        Item_Paint_ui = CreateInlineHook(ui(0x40015400), Item_Paint_ui_f);
        CreateMidHook(ui(0x40017330), [](SafetyHookContext& ctx) {
            itemDef_s* item = *(itemDef_s**)(ctx.esp + 0x42C);
            if (item) {
                if (item->cvar && !strcmp("ui_r_mode", item->cvar)) {
                    printf("cvar %s\n", item->cvar);
                    multiDef_t* multiPtr = (multiDef_t*)sp_mp((uintptr_t)item->typeData, item->cursorPos);
                    if (multiPtr) {
                        multiPtr->count = 0;
                        // Use the menu-specific array (already limited to MAX_MULTI_CVARS)
                        for (int i = 0; i < r_vidModes_menu_count; i++) {
                            multiPtr->cvarList[multiPtr->count] = String_Alloc_ui(r_vidModes_menu[i].description);
                            multiPtr->cvarValue[multiPtr->count] = r_vidModes_menu[i].r_mode_setting;
                            multiPtr->count++;

                            if (multiPtr->count >= MAX_MULTI_CVARS)
                                break;

                        }
                    }


                }

                if (hook_shortversion && item->cvar && !strcmp("shortversion", item->cvar)) {

                    item->cvar = String_Alloc_ui("hook_shortversion");

                }

            }
            });
    }
}

uintptr_t G_CheckForPreventFriendlyFire_address;

void G_CheckForPreventFriendlyFire(uintptr_t unknown) {

    __asm {
        mov eax,unknown
        call G_CheckForPreventFriendlyFire_address
    }

}
bool is_enemy_crosshair = false;
void G_CheckForPreventFriendlyFire_hook(uintptr_t unknown) {
    uintptr_t original_eax;
    __asm mov original_eax,eax

    ////*ally_check_ptr = 0x85;

    //enemy_check[0] = true;

    ////*friendly_flag_check = 0x80000000;
    //G_CheckForPreventFriendlyFire(original_eax);
    //enemy_check[0] = false;
    ////printf("FIRST g_friendlyfireDist_read_addr %p ally_check_ptr %d friendly_flag_check 0x%X\n", *g_friendlyfireDist_read_addr, *ally_check_ptr, *friendly_flag_check);

    //*ally_check_ptr = 0x84;
    ////*friendly_flag_check = 0x1000000;

    is_enemy_crosshair = false;

    G_CheckForPreventFriendlyFire(original_eax);

    //printf("g_friendlyfireDist_read_addr %p ally_check_ptr %d friendly_flag_check 0x%X\n", *g_friendlyfireDist_read_addr, *ally_check_ptr, *friendly_flag_check);

}

cevar_s* g_enemyFireDist;
void game_hooks(HMODULE handle) {
    if(!sp_mp(1))
        return;
    uintptr_t OFFSET = (uintptr_t)handle;
    game_offset = OFFSET;
    //if(g_save_allowbadchecksum && g_save_allowbadchecksum->integer)
    //Memory::VP::Nop(g(0x200306C8), 5);


    if (player_sprintmult) {
        auto pattern = hook::pattern(handle, "? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? DF E0 F6 C4 ? 7A ? 8B 41 ? 83 E0 ? C7 44 81");
        if (!pattern.empty())
            Memory::VP::Patch<float*>(pattern.get_first(2), &player_sprintmult->value);

    }

    G_CheckForPreventFriendlyFire_address = g(0x20020010);

    //Memory::VP::InjectHook(g(0x200146D3), G_CheckForPreventFriendlyFire_stub,Memory::VP::HookType::Jump);
    CreateMidHook(g(0x200146D3), [](SafetyHookContext& ctx) {
        is_enemy_crosshair = false;
        });


    if (!g_enemyFireDist)
        g_enemyFireDist = Cevar_Get("g_enemyFireDist", 4096.f, CVAR_ARCHIVE,-1.f,FLT_MAX);



    CreateMidHook(g(0x2002019D), [](SafetyHookContext& ctx) {

        if (g_enemyFireDist->base->value == 0.f) {
            is_enemy_crosshair = false;
            return;
        }

        vector3* coords_1 = (vector3*)(ctx.edi + 0xE4);
        vector3* coords_2 = (vector3*)(ctx.esp + 0x70);

        vector3 difference = *coords_1 - *coords_2;

        bool allowed_to_tag = (difference.magnitude() <= g_enemyFireDist->base->value) || g_enemyFireDist->base->value == -1.f;

        if (allowed_to_tag && !(ctx.eflags & 0x40) == 0) {

            is_enemy_crosshair = true;
        }
        else {
            is_enemy_crosshair = false;
        }
        });

}

SafetyHookInline* SCR_DrawString_hook_possible1_detour;

int __cdecl SCR_DrawString_hook_possible1(float x, float y, float width, float height) {
    if (x >= 0 && x <= 120) {

        x -= (process_width() * get_safeArea_horizontal());

    }


    if (x >= 300 && x <= 640) {
        x += (process_width() * get_safeArea_horizontal());
    }

    return SCR_DrawString_hook_possible1_detour->unsafe_ccall<int>(x, y, width, height);
}

void HandleWeaponADS_hack(float* current_weapon_fov) {
    if (cg_fov) {
        if (cg_fov_fix_lowfovads && cg_fov_fix_lowfovads->base->integer == 1) {
            if (cg_fov->value < *current_weapon_fov) {
                float value = cg_fov->value / 80.f;
                *current_weapon_fov *= value;
            }
        }
        else if (cg_fov_fix_lowfovads && cg_fov_fix_lowfovads->base->integer >= 2) {
            if (cg_fov->value < 80.f) {
                float value = cg_fov->value / 80.f;
                *current_weapon_fov *= value;
            }
        }
    }

    if (cg_fovscale_ads) {
        *current_weapon_fov *= std::clamp(cg_fovscale_ads->value, 0.1f, FLT_MAX);
    }

}

int __cdecl trap_R_DrawStretchPic_center_cross(
    float x,
    float y,
    float w,
    float h,
    float s1,
    float t1,
    float s2,
    float t2,
    int hShader) {
    CG_AdjustFrom640(x, y, w, h);
    return cdecl_call<int>(trap_R_DrawStretchPic, x, y, w, h, s1, t1, s2, t2, hShader);
}

void AutoAnchor(float* x) {
    if (!x)
        return;
    float width_offset = (process_width() * 0.5f) * get_safeArea_horizontal();

    if (*x >= 450) {
        *x += width_offset;
        return;
    }
    else if (*x <= 20) {
        *x -= width_offset;
        }

}

void AutoAnchor(float& x) {
    AutoAnchor(&x);
}

void codDLLhooks(HMODULE handle) {
    // printf("run");
    uintptr_t OFFSET = (uintptr_t)handle;
    cg_game_offset = OFFSET;
    StaticInstructionPatches();
    Memory::VP::InterceptCall(OFFSET + LoadedGame->CG_DrawFlashImage_Draw, DrawStretch_og, DrawStretch_300135B0);
    Memory::VP::InterceptCall(cg(0x3001221A, 0x3001A8CF), DrawStretch_og, DrawStretch_300135B0);

    //SprintT4_lol(handle);

    unsigned int* res = (unsigned int*)LoadedGame->X_res_Addr;

    x_modded = res[0] + (res[0] - (res[1] * GetAspectRatio()));

    auto pat = hook::pattern(handle, "83 EC ? 8B 44 24 ? ? ? ? ? ? ? ? ? ? ? 8B 4C 24 ? 6A 00");

    if (!pat.empty()) {
        //SCR_DrawString_hook_possible1_detour = CreateInlineHook(pat.get_first(), SCR_DrawString_hook_possible1_detour);
    }

    if (sp_mp(0, 0)) {

    }

    pat = hook::pattern(handle, "? ? ? ? ? ? ? ? ? ? ? ? A1 ? ? ? ? ? ? ? ? ? ? 83 C8");

    if (!pat.empty()) {
        fov_world = *pat.get_first<vector2*>(2);
        printf("FOV WORLD IS UHH %p\n", fov_world);
    }

    static uint32_t DEFUALT_SCREEN_HEIGHT = 480;
    static uint32_t DEFUALT_SCREEN_WIDTH = 640;

    static float DEFAULT_1_0 = 1.f;

    auto y_scale_ptr = cg(0x30011ED9 + 2, 0x3001A40C + 2);
    auto x_scale_ptr = cg(0x30011F27 + 2, 0x3001A45A + 2);
    Memory::VP::Read(y_scale_ptr, cg_screenYScale);

    Memory::VP::Read(x_scale_ptr, cg_screenXScale);

    Memory::VP::Patch<void*>(y_scale_ptr, &DEFAULT_1_0);
    Memory::VP::Patch<void*>(x_scale_ptr, &DEFAULT_1_0);

    printf("DEFAULT_1_0 %p\n", &DEFAULT_1_0);

    Memory::VP::Patch<void*>(cg(0x30011F51 + 2,0x3001A484 + 2), &DEFUALT_SCREEN_WIDTH);
    Memory::VP::Patch<void*>(cg(0x30011F03 + 2,0x3001A436 + 2), &DEFUALT_SCREEN_HEIGHT);


    Memory::VP::InterceptCall(cg(0x30011F68, 0x3001A49B), crosshair_render_func, crosshair_render_hook);

    auto pattern = hook::pattern(handle, "? ? ? ? ? ? 8B 41 ? 83 F8 ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? 75");
    auto pattern2 = hook::pattern(handle, "? ? ? ? ? ? ? ? ? ? 8B 82 ? ? ? ? 50");
    if (!pattern.empty() && !pattern2.empty()) {
        Memory::VP::Patch<void*>(pattern.get_first(2), &DEFAULT_1_0);
        Memory::VP::Patch<void*>(pattern.get_first(22), &DEFAULT_1_0);

        Memory::VP::Patch<void*>(pattern2.get_first(2), &DEFUALT_SCREEN_HEIGHT);
        Memory::VP::Patch<void*>(pattern2.get_first(60 + 2), &DEFUALT_SCREEN_WIDTH);

        Memory::VP::InjectHook(pattern2.get_first(89), trap_R_DrawStretchPic_center_cross);
    }

    static cevar_s* cg_drawCrosshair_friendly_green;

    if (!cg_drawCrosshair_friendly_green)
        cg_drawCrosshair_friendly_green = Cevar_Get("cg_drawCrosshair_friendly_green",1,CVAR_ARCHIVE,0,1);

    if (sp_mp(1)) {
        CreateMidHook(cg(0x30011916), [](SafetyHookContext& ctx) {
            uint32_t* some_player_flags = (uint32_t*)cg(0x3026D9F0);
            if (is_enemy_crosshair) {
                ctx.eip = cg(0x30011922);
                return;
            }
            else if ((*some_player_flags & 0x1000000) != 0 && cg_drawCrosshair_friendly_green->base->integer) {
                vector3* crosshair_color = (vector3*)(ctx.esp + 0x10);
                crosshair_color->x = 0.f;
                crosshair_color->y = 0.80f;
                crosshair_color->z = 0.25f;
                ctx.eip = cg(0x30011932);
            }

            });
    }

    Memory::VP::InterceptCall(cg(0x30011222, 0x30019752), trap_R_DrawStretchPic, R_DrawStretchPic_leftsniper);

    Memory::VP::InterceptCall(cg(0x30011270, 0x300197A0), trap_R_DrawStretchPic, R_DrawStretchPic_rightsniper);

    Memory::VP::Nop(cg(0x30011205, 0x30019735), 2);

    Memory::VP::Nop(cg(0x30011247, 0x30019777), 2);

    CG_DrawPicAddr = cg(0x30013B70, 0x3001CAA0);


    if (sp_mp(0, 1)) {
        Memory::VP::Nop(cg(0, 0x3004000A), 8);
    }

    CG_GetViewFov_og_S = CreateInlineHook(OFFSET + LoadedGame->DLL_CG_GetViewFov_offset, &CG_GetViewFov_hook);
    //if (MH_CreateHook((void**)OFFSET + 0x2CC20, &CG_GetViewFov_hook, (void**)&CG_GetViewFov_og) != MH_OK) {
    //    MessageBoxW(NULL, L"FAILED TO HOOK", L"Error", MB_OK | MB_ICONERROR);
    //    return;
    //}

// CG_DrawWeaponSelect
    CreateMidHook(cg(0x30033A41, 0x30046C5B), [](SafetyHookContext& ctx) {
        auto config = FindMenuConfig("weaponinfo");
        if (config && config->alignment.h_right) {
            *(float*)(ctx.esp + 0x10) += ((process_width() * 0.5f) * get_safeArea_horizontal());
        }

        });

    Item_Paint_cg = CreateInlineHook(OFFSET + LoadedGame->Item_Paint, Item_Paint_cg_f);

    pat = hook::pattern(handle, "50 51 6A ? FF 15 ? ? ? ? 83 C4 ? 83 C4 ? C3 ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? 83 EC");
    if (!pat.empty()) {
        DrawObjectives = CreateMidHook(pat.get_first(), [](SafetyHookContext& ctx) {
            auto menuConfig = FindMenuConfig("Compass");
            if (menuConfig) {
                if (menuConfig->alignment.h_left) {
                    float halfWidth = process_width() * 0.5f;
                    float safeX = get_safeArea_horizontal();

                    ctx.ecx -= halfWidth * get_safeArea_horizontal();
                }

                if (menuConfig->alignment.v_bottom) {
                    ctx.eax -= (int)(process_height_hack_safe() * 0.5f);
                }

            }

            });
    }

    auto SingleHudElem_ptr = cg(0x3001FB14, 0x3002A4C4);
    if (SingleHudElem_ptr) {
        Memory::VP::InterceptCall(SingleHudElem_ptr, DrawSingleHudElem2dog, DrawSingleHudElem2dHook);
    }
    static auto mapname = Cvar_Find("mapname");
    DrawHudElemMaterial_mid = CreateMidHook(cg(0x3001F8D4, 0x3002A26C), [](SafetyHookContext& ctx) {
        const char* hud_elem_shader_name = (const char*)(ctx.esp + 0x1C);
        hudelem_s* elem = (hudelem_s*)ctx.edi;
        // Check if shader has a config
        const HudShaderConfig* shaderConfig = FindHudShaderConfig(hud_elem_shader_name);




        float* x = (float*)(ctx.esi);
        float* y = (float*)(ctx.esp + 0x10);
        float* width = (float*)(ctx.esp + 0x18);
        float* height = (float*)(ctx.esp + 0x14);
        
        if (sp_mp(0, 1)) {
            y = (float*)(ctx.esp + 0x14);
            height = (float*)(ctx.esp + 0x10);
        }

        auto isAlignX = [elem](const alignx_e alig_type) {
            if (!elem || !cg_hudelem_alignhack->base->integer) {
                return false;
            }

            return elem->alignx == alig_type;

            };

        auto isAlignY = [elem](const aligny_e alig_type) {
            if (!elem || !cg_hudelem_alignhack->base->integer) {
                return false;
            }

            return elem->aligny == alig_type;

            };


        // Hardcoded "black" OR config with stretch flag set
        if(cg_hudelem_printnames->base->integer)
        printf("name %s\n", hud_elem_shader_name);

        bool is_black_screen = (strcmp(hud_elem_shader_name, "black") == 0);

        bool shouldStretch = (is_black_screen && ((*width >= 640 && *height >= 480) || (cg_fixedAspect_blackshader_all && cg_fixedAspect_blackshader_all->base && cg_fixedAspect_blackshader_all->base->integer))  ) ||
            (shaderConfig && shaderConfig->alignment.stretch);
        if (shouldStretch) {

            *x += -process_width();

            static float fuckthis[2]{};
            *width += fuckthis[0];
            *height += fuckthis[1];

            *width += process_width() * 2.f;
        }

        // Apply alignment-based adjustments from config
        else if (shaderConfig) {


            float halfWidth = process_width() * 0.5f;
            float safeX = get_safeArea_horizontal();

            if (shaderConfig->alignment.h_left && !isAlignX(ALIGNX_LEFT)) {
                *x += (-halfWidth) * safeX;
            }
            else if (shaderConfig->alignment.h_right && !isAlignX(ALIGNX_RIGHT)) {
                *x += (halfWidth)*safeX;
            }
            float height_hack = process_height_hack_safe() * 0.5f;
            if (shaderConfig->alignment.v_bottom && !isAlignY(ALIGNY_BOTTOM) && !is_black_screen) {
                *y -= height_hack;
            }
            else if (shaderConfig->alignment.v_top && !isAlignY(ALIGNY_TOP) && !is_black_screen) {
                *y += height_hack;
            }

            // Add vertical when ready
            // if (shaderConfig->alignment.v_top) { ... }
            // if (shaderConfig->alignment.v_bottom) { ... }
        }

        });

    CreateMidHook(cg(0x3001F7DF), [](SafetyHookContext& ctx) {

        float& clock_x = *(float*)ctx.esi;
        clock_x -= (process_width() * 0.5f) * get_safeArea_horizontal();

        });

    CreateMidHook(cg(0x3001F64B, 0x30029FEB), [](SafetyHookContext& ctx) {

        float* text_x = (float*)&ctx.ecx;
        AutoAnchor(text_x);

        });

    cg_fov = Cvar_Find("cg_fov");

    if (sp_mp(0, 1)) {
        // make CG_FOV archive in MP
        Memory::VP::Patch<int>(cg(0, 0x3008523C), CVAR_ARCHIVE);
    }

    if (cg(0x3002CDCD)) {
        Memory::VP::Nop(cg(0x3002CDCD), 6);
        CreateMidHook(cg(0x3002CDCD), [](SafetyHookContext& ctx) {

            float current_weapon_ads = *(float*)(ctx.eax + 0x274);

            HandleWeaponADS_hack(&current_weapon_ads);

            FPU::FLD(current_weapon_ads);


            });
    }
    else if (cg(0, 0x30040167)) {
        CreateMidHook(cg(0,0x30040167), [](SafetyHookContext& ctx) {


            HandleWeaponADS_hack((float*)&ctx.eax);

            });
    }

    auto FOV_ads_2 = cg(0x3002CE48, 0x300401E5);
    if (FOV_ads_2) {
        Memory::VP::Nop(FOV_ads_2, 6);
        CreateMidHook(FOV_ads_2, [](SafetyHookContext& ctx) {
            bool isSP = sp_mp(1);

            auto ptr = isSP ? ctx.edx : ctx.ecx;

            float current_weapon_ads = *(float*)(ptr + 0x274);


            HandleWeaponADS_hack(&current_weapon_ads);

            FPU::FSUB(current_weapon_ads);


            });
    }


        cg_drawupperright_x = (float*)cg(0x30064FC8,0x3007C040);
        if (cg_drawupperright_x) {
            DWORD oldProtect;
            VirtualProtect(cg_drawupperright_x, sizeof(cg_drawupperright_x), PAGE_EXECUTE_READWRITE, &oldProtect);
        

        if(cg_fixedAspect)
        set_cg_drawupperright_x_wide(cg_fixedAspect->base->integer);
    }

}


SafetyHookInline Cvar_Set_og;
cvar_s* __cdecl Cvar_Set(const char* cvar_name, const char* value, BOOL force) {

    if (_stricmp(cvar_name, "cg_fov") == 0) {
        printf("CG_FOV from %p %s\n", _ReturnAddress(), value);
    }

    if (!cvar_name || !value) {
        return Cvar_Set_og.ccall<cvar_s*>(cvar_name, value, force);
    }

    // Try to find existing cvar to get cevar
    cvar_t* existing_cvar = Cvar_Find(cvar_name);
    cevar_t* cevar = existing_cvar ? Cevar_FromCvar(existing_cvar) : nullptr;

    std::string clamped_value = value;
    const char* oldValue = existing_cvar ? existing_cvar->string : nullptr;

    // Apply limits if cevar exists and has limits
    if (cevar && cevar->limits.has_limits) {
        if (cevar->limits.is_float) {
            float val = (float)atof(value);
            float clamped = std::clamp(val, cevar->limits.f.min, cevar->limits.f.max);

            // Only clamp if different
            if (val != clamped) {
                char buffer[32];
                sprintf(buffer, "%f", clamped);
                clamped_value = buffer;
                Com_Printf("^3[CEVAR]^7 '%s' clamped to %.2f (valid range: %.2f - %.2f)\n",
                    cvar_name, clamped, cevar->limits.f.min, cevar->limits.f.max);
            }
        }
        else {
            int val = atoi(value);
            int clamped = std::clamp(val, cevar->limits.i.min, cevar->limits.i.max);

            // Only clamp if different
            if (val != clamped) {
                char buffer[32];
                sprintf(buffer, "%d", clamped);
                clamped_value = buffer;
                Com_Printf("^3[CEVAR]^7 '%s' clamped to %d (valid range: %d - %d)\n",
                    cvar_name, clamped, cevar->limits.i.min, cevar->limits.i.max);
            }
        }
    }


    auto cvar = Cvar_Set_og.ccall<cvar_s*>(cvar_name, clamped_value.c_str(), force);

    // Check if value actually changed
    bool value_changed = false;
    if (cvar) {
        if (cvar->flags & CVAR_LATCH) {
            // Latched cvars update latchedString, not string
            if (cvar->latchedString && oldValue) {
                value_changed = (strcmp(cvar->latchedString, oldValue) != 0);
            }
        }
        else {
            // Normal cvars update string immediately
            if (cvar->string && oldValue) {
                value_changed = (strcmp(cvar->string, oldValue) != 0);
            }
        }
    }

    // Trigger callback if value changed
    if (value_changed && cevar && cevar->callback) {
        cevar->callback(cvar, oldValue);
    }

    // Special hardcoded callbacks
    if (cvar == safeArea_horizontal) {
        StaticInstructionPatches(NULL, false);
        if (cg_fixedAspect)
            set_cg_drawupperright_x_wide(cg_fixedAspect->base->integer);
    } else if (cg_fixedAspect && cg_fixedAspect->base == cvar)
        set_cg_drawupperright_x_wide(cvar->integer != 0);

    return cvar;
}

//SafetyHookInline SCR_AdjustFrom640_OG;
bool ConsoleDrawing;
void Con_DrawConsole() {
    //cdecl_call<int>(0x4D7D70);
    ConsoleDrawing = true;
    cdecl_call<void>(0x40A450);
    ConsoleDrawing = false;
}



//float* __cdecl SCR_AdjustFrom640(float* x, float* y, float* w, float* h) {
//
//    auto result = SCR_AdjustFrom640_OG.ccall<float*>(x, y, w, h);
//
//    if (ConsoleDrawing && x && h) {
//        static float fuckwit[2];
//
//        *x *= fuckwit[0];
//        *w *= fuckwit[1];
//    }
//
//}

uintptr_t qglTexParameteri_ptr;

bool GL_EXT_texture_filter_anisotropic_supported = false;

int __stdcall qglTexParameteri_aniso_hook1(int a1, int a2, int a3) {
    int filter_value = 1;
    if (r_qol_texture_filter_anisotropic) {
        filter_value = std::clamp(r_qol_texture_filter_anisotropic->base->integer, 1, 16);
    }
    if(GL_EXT_texture_filter_anisotropic_supported)
    stdcall_call<int>(*(int*)qglTexParameteri_ptr, a1, 0x84FE, filter_value);
    auto result = stdcall_call<int>(*(int*)qglTexParameteri_ptr, a1, a2, a3);
    return result;
    
}

int __stdcall qglTexParameteri_aniso_hook2(int a1, int a2, int a3) {
    if(GL_EXT_texture_filter_anisotropic_supported)
    stdcall_call<int>(*(int*)qglTexParameteri_ptr, a1, 0x84FE, 1);
    auto result = stdcall_call<int>(*(int*)qglTexParameteri_ptr, a1, a2, a3);
    return result;

}

SafetyHookInline com_initd;


int __cdecl com_init_hook(void* unknown) {
    auto result = com_initd.unsafe_ccall<int>(unknown);

    char buffer[512]{};

    auto version = Cvar_Find("version");

    if (version && version->string) {

        sprintf_s(buffer, sizeof(buffer), MOD_NAME" r%d %s %s", BUILD_NUMBER, BUILD_TIME_UTC,COMMIT_HASH);

        hook_version = Cvar_Get("hook_version", buffer, CVAR_ROM);

        auto pattern = hook::pattern("8B 15 ? ? ? ? 8B 42 ? 83 C4 ? 8D 78");
        if (!pattern.empty()) {
            Memory::VP::Patch<cvar_s**>(pattern.get_first(2), &hook_version);
        }

    }

    return result;

}

SafetyHookInline Cvar_getD;

static bool remove_cheat_flags = false;
cvar_s* disable_cheats;
cvar_t* Cvar_get_Hook(const char* name, const char* value, int flags) {

    if (!remove_cheat_flags) {
        remove_cheat_flags = true;
        disable_cheats = Cvar_getD.unsafe_ccall<cvar_s*>("disable_cheats_flags", "0", CVAR_ARCHIVE);
    }

    if (disable_cheats && disable_cheats->integer && flags & CVAR_CHEAT)
        flags &= ~CVAR_CHEAT;

    return Cvar_getD.unsafe_ccall<cvar_s*>(name,value,flags);


}

int cvar_show_com_printf(const char* format, const char* cvar_name, const char* cvar_value) {
    auto cevar = Cevar_FromCvar(cvar_name);
    int result = 0;
    if (cevar && cevar->base) {
        result = Com_Printf("  ^7%s = ^2%s^0", cvar_name, cvar_value);

        if (cevar && cevar->limits.has_limits) {
            if (cevar->limits.is_float) {
                const char* min_str = (cevar->limits.f.min == -FLT_MAX) ? "any" : nullptr;
                const char* max_str = (cevar->limits.f.max == FLT_MAX) ? "any" : nullptr;

                if (min_str && max_str) {
                    Com_Printf(" ^3Limits:^7 any ^7to^7 any\n");
                }
                else if (min_str) {
                    Com_Printf(" ^3Limits:^7 any ^7to^7 %.2f\n", cevar->limits.f.max);
                }
                else if (max_str) {
                    Com_Printf(" ^3Limits:^7 %.2f ^7to^7 any\n", cevar->limits.f.min);
                }
                else {
                    Com_Printf(" ^3Limits:^7 %.2f ^7to^7 %.2f\n",
                        cevar->limits.f.min, cevar->limits.f.max);
                }
            }
            else {
                const char* min_str = (cevar->limits.i.min == INT_MIN) ? "any" : nullptr;
                const char* max_str = (cevar->limits.i.max == INT_MAX) ? "any" : nullptr;

                if (min_str && max_str) {
                    Com_Printf(" ^3Limits:^7 any ^7to^7 any\n");
                }
                else if (min_str) {
                    Com_Printf(" ^3Limits:^7 any ^7to^7 %d\n", cevar->limits.i.max);
                }
                else if (max_str) {
                    Com_Printf(" ^3Limits:^7 %d ^7to^7 any\n", cevar->limits.i.min);
                }
                else {
                    Com_Printf(" ^3Limits:^7 %d ^7to^7 %d\n",
                        cevar->limits.i.min, cevar->limits.i.max);
                }
            }
        }
        else Com_Printf("\n");
        return result;
    }
    return Com_Printf(format, cvar_name, cvar_value);
}

inline bool IsModuleUAL(HMODULE mod)
{
    if (GetProcAddress(mod, "IsUltimateASILoader") != NULL)
        return true;
    return false;
}

bool IsUALPresent() {
    for (const auto& entry : std::stacktrace::current()) {
        HMODULE hModule = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)entry.native_handle(), &hModule)) {
            if (IsModuleUAL(hModule))
                return true;
        }
    }
    return false;
}
void LAACheck();

UINT GetPrivateProfileIntW(LPCWSTR lpAppName, LPCWSTR lpKeyName, INT nDefault, const std::vector<std::wstring>& fileNames)
{
    for (const auto& file : fileNames)
    {
        nDefault = GetPrivateProfileIntW(lpAppName, lpKeyName, nDefault, file.c_str());
    }
    return nDefault;
}

bool PathExists(auto szPath)
{
    try
    {
        auto path = std::filesystem::path(szPath);
        if (path.is_absolute())
            return std::filesystem::exists(path);
        else
            return std::filesystem::exists(std::filesystem::path(GetModulePath(NULL)).parent_path() / path);
    }
    catch (...)
    {
    }
    return false;
}

BOOL WritePrivateProfileStringW_UAL(LPCWSTR lpAppName, LPCWSTR lpKeyName, LPCWSTR lpString, std::vector<std::wstring> fileNames)
{
    bool anyExists = std::any_of(fileNames.begin(), fileNames.end(),
        [](const auto& f) { return PathExists(f.c_str()); });

    if (!anyExists)
    {
        HMODULE hModule = NULL;
        GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)&WritePrivateProfileStringW_UAL,
            &hModule
        );
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(hModule, path, MAX_PATH);
        auto dllDir = std::filesystem::path(path).parent_path();
        fileNames.clear();
        fileNames.emplace_back(dllDir / L"global.ini");
    }

    for (const auto& file : fileNames)
    {
        if (PathExists(file.c_str()))
        {
            return WritePrivateProfileStringW(lpAppName, lpKeyName, lpString, file.c_str());
        }
    }

    if (!fileNames.empty())
    {
        return WritePrivateProfileStringW(lpAppName, lpKeyName, lpString, fileNames[0].c_str());
    }

    return FALSE;
}
bool GameIsLargeAddressAware();
void InitHook() {
    CheckGame();
    if (!CheckGame()) {
        MessageBoxW(NULL, L"Unsupported game", TEXT(MOD_NAME), MB_OK | MB_ICONWARNING);
        return;
    }
    
    std::vector<std::wstring> iniPaths;


    if (sp_mp(1)) {
        // remove CVAR_ROM from sv_cheats in SP
        Memory::VP::Patch<uint8_t>((0x431CEF + 1), CVAR_SYSTEMINFO | CVAR_ARCHIVE);
        Memory::VP::Patch<uint8_t>((0x4AC6F1 + 1), CVAR_SYSTEMINFO | CVAR_ARCHIVE);
        Memory::VP::Patch<uint8_t>((0x44EB32 + 1), CVAR_SYSTEMINFO | CVAR_ARCHIVE);

        // dont set sv_cheats to 0 on /map load
        Memory::VP::Nop(0x0044A458, 5);

        // doesn't really seem needed but uncomment to have the effect
        //// increase MAX cvars
        //static cvar_s new_cvars_array[MAX_CVARS_NEW]{};

        //Memory::VP::Patch<uint32_t>((0x0043398B + 6), MAX_CVARS_NEW);
        //Memory::VP::Patch<cvar_s*>((0x004339B0 + 2), new_cvars_array);
        //Memory::VP::Patch<cvar_s*>((0x00433D58 + 2), new_cvars_array);
        //Memory::VP::Patch<cvar_s*>((0x00433E44 + 1), new_cvars_array);
        //Memory::VP::Patch<cvar_s*>((0x00433E92 + 2), new_cvars_array);
    }

    std::wstring modulePath = GetModulePath(NULL);
    std::wstring moduleName = modulePath.substr(modulePath.find_last_of(L"/\\") + 1);
    moduleName.resize(moduleName.find_last_of(L'.'));
    modulePath.resize(modulePath.find_last_of(L"/\\") + 1);

    iniPaths.emplace_back(modulePath + moduleName + L".ini");
    iniPaths.emplace_back(modulePath + L"global.ini");
    iniPaths.emplace_back(modulePath + L"scripts\\global.ini");
    iniPaths.emplace_back(modulePath + L"plugins\\global.ini");
    iniPaths.emplace_back(modulePath + L"update\\global.ini");
    auto nDontLoadFromDllMain = GetPrivateProfileIntW(TEXT("globalsets"), TEXT("dontloadfromdllmain"), TRUE, iniPaths);

    auto UALPresent = IsUALPresent();

    if (!UALPresent && thisModuleFileName() != LIBRARYW) {
        MessageBoxA(NULL, "It appears that Ulitmate ASI Loader is missing, it's highly recommended that you use it to load the mod, otherwise expect issues!", MOD_NAME, MB_OK | MB_ICONWARNING);
    }
    else if (UALPresent && thisModuleFileName() != LIBRARYW && nDontLoadFromDllMain) {
        MessageBoxA(NULL, "Detected loading via Ultimate ASI Loader, but DontLoadFromDllMain within global.ini is set to true, which is unsupported for " MOD_NAME " and will cause issues with the steam version!, this has been automatically disabled, please restart.", MOD_NAME, MB_OK | MB_ICONWARNING);
        WritePrivateProfileStringW_UAL(TEXT("globalsets"), TEXT("dontloadfromdllmain"), TEXT("0"), iniPaths);
    }

    if (!UALPresent) {
        (void)_mkdir("CrashDumps");
        SetUnhandledExceptionFilter(CustomUnhandledExceptionFilter);
    }




    LAACheck();
    component_loader::post_start();
    SetProcessDPIAware();

    rinput::Init();

    //GetProcAddressD = safetyhook::create_inline(GetProcAddress, stub_GetProcAddress);

    //FS_FOpenFileReadD = safetyhook::create_inline(0x423B90, FS_FOpenFileRead);

    Cvar_getD = safetyhook::create_inline(LoadedGame->Cvar_Get_Addr, Cvar_get_Hook);

    InitializeDisplayModesForGame();

    com_initd = safetyhook::create_inline(exe(0x00431CA0, 0x0043BC10), com_init_hook);

    auto pat = hook::pattern("FF 15 ? ? ? ? 8B 15 ? ? ? ? 52 E9");

    gexe::keyCatchers = (uint32_t*)exe(0x4842104);

    if (!pat.empty()) {

        uintptr_t call1 = (uintptr_t)pat.get_first();

        Memory::VP::Read<uintptr_t>(call1 + 2, qglTexParameteri_ptr);

        Memory::VP::Nop(call1, 6);

        pat = hook::pattern("FF 15 ? ? ? ? 68 ? ? ? ? 68 ? ? ? ? 56");

        if (!pat.empty()) {
            Memory::VP::Nop(pat.get_first(), 6);

            Memory::VP::InjectHook(call1, qglTexParameteri_aniso_hook1, Memory::VP::HookType::Call);
            Memory::VP::InjectHook(pat.get_first(), qglTexParameteri_aniso_hook2, Memory::VP::HookType::Call);
        }
        pat = hook::pattern("FF 15 ? ? ? ? 83 C4 ? 5F 5E 5B 59 C3 68");
        if (!pat.empty()) {
            static auto GL_EXT_texture_filter_anisotropic_check = safetyhook::create_mid(pat.get_first(-40), [](SafetyHookContext& ctx) {
                GL_EXT_texture_filter_anisotropic_supported = ctx.eax != 0;
                });

                Memory::VP::Nop(pat.get_first(), 6);
        }
    }

    if (GameIsLargeAddressAware()) {
        static const char* new_com_hunkMegs_default = "512";
        Memory::VP::Patch<const char*>(exe(0x0042BDCA + 1, 0x00435A1B + 1), new_com_hunkMegs_default);
    }
    

    pat = hook::pattern("E8 ? ? ? ? 8B 76 ? 83 C4 ? 85 F6 74");

    if (!pat.empty()) {
        Memory::VP::ReadCall(pat.get_first(), Com_Printf);

    #define FLAG_COLOR "^5"

        auto static print_flags = safetyhook::create_mid(pat.get_first(5), [](SafetyHookContext& ctx) {
            cvar_s* thiscvar = (cvar_s*)ctx.esi;
            if (thiscvar) {
                int flags = thiscvar->flags;
                static char buffer[2048];
                buffer[0] = '\0';
                bool first = true;

                // Print flags
                if (flags & CVAR_ARCHIVE) { if (!first) strcat(buffer, "^7, "); strcat(buffer, FLAG_COLOR "Archive"); first = false; }
                if (flags & CVAR_USERINFO) { if (!first) strcat(buffer, "^7, "); strcat(buffer, FLAG_COLOR "UserInfo"); first = false; }
                if (flags & CVAR_SERVERINFO) { if (!first) strcat(buffer, "^7, "); strcat(buffer, FLAG_COLOR "ServerInfo"); first = false; }
                if (flags & CVAR_SYSTEMINFO) { if (!first) strcat(buffer, "^7, "); strcat(buffer, FLAG_COLOR "SystemInfo"); first = false; }
                if (flags & CVAR_INIT) { if (!first) strcat(buffer, "^7, "); strcat(buffer, FLAG_COLOR "Init"); first = false; }
                if (flags & CVAR_LATCH) { if (!first) strcat(buffer, "^7, "); strcat(buffer, FLAG_COLOR "Latch"); first = false; }
                if (flags & CVAR_ROM) { if (!first) strcat(buffer, "^7, "); strcat(buffer, FLAG_COLOR "Rom"); first = false; }
                if (flags & CVAR_USER_CREATED) { if (!first) strcat(buffer, "^7, "); strcat(buffer, FLAG_COLOR "UserCreated"); first = false; }
                if (flags & CVAR_TEMP) { if (!first) strcat(buffer, "^7, "); strcat(buffer, FLAG_COLOR "Temp"); first = false; }
                if (flags & CVAR_CHEAT) { if (!first) strcat(buffer, "^7, "); strcat(buffer, FLAG_COLOR "Cheat"); first = false; }
                if (flags & CVAR_NORESTART) { if (!first) strcat(buffer, "^7, "); strcat(buffer, FLAG_COLOR "NoRestart"); first = false; }
                if (flags & CVAR_WOLFINFO) { if (!first) strcat(buffer, "^7, "); strcat(buffer, FLAG_COLOR "WolfInfo"); first = false; }
                if (flags & CVAR_UNSAFE) { if (!first) strcat(buffer, "^7, "); strcat(buffer, FLAG_COLOR "Unsafe"); first = false; }
                if (flags & CVAR_SERVERINFO_NOUPDATE) { if (!first) strcat(buffer, "^7, "); strcat(buffer, FLAG_COLOR "ServerInfoNoUpdate"); first = false; }

                // Check for cevar
                cevar_t* cevar = Cevar_FromCvar(thiscvar);
                if (cevar) {
                    if (!first) strcat(buffer, "^7, ");
                    strcat(buffer, "^2CEVAR");  // Green color for CEVAR
                    first = false;
                }

                Com_Printf(" %s^7\n", buffer);

                // Print limits on separate line if they exist
                if (cevar && cevar->limits.has_limits) {
                    if (cevar->limits.is_float) {
                        const char* min_str = (cevar->limits.f.min == -FLT_MAX) ? "any" : nullptr;
                        const char* max_str = (cevar->limits.f.max == FLT_MAX) ? "any" : nullptr;

                        if (min_str && max_str) {
                            Com_Printf(" ^3Limits:^7 any ^7to^7 any\n");
                        }
                        else if (min_str) {
                            Com_Printf(" ^3Limits:^7 any ^7to^7 %.2f\n", cevar->limits.f.max);
                        }
                        else if (max_str) {
                            Com_Printf(" ^3Limits:^7 %.2f ^7to^7 any\n", cevar->limits.f.min);
                        }
                        else {
                            Com_Printf(" ^3Limits:^7 %.2f ^7to^7 %.2f\n",
                                cevar->limits.f.min, cevar->limits.f.max);
                        }
                    }
                    else {
                        const char* min_str = (cevar->limits.i.min == INT_MIN) ? "any" : nullptr;
                        const char* max_str = (cevar->limits.i.max == INT_MAX) ? "any" : nullptr;

                        if (min_str && max_str) {
                            Com_Printf(" ^3Limits:^7 any ^7to^7 any\n");
                        }
                        else if (min_str) {
                            Com_Printf(" ^3Limits:^7 any ^7to^7 %d\n", cevar->limits.i.max);
                        }
                        else if (max_str) {
                            Com_Printf(" ^3Limits:^7 %d ^7to^7 any\n", cevar->limits.i.min);
                        }
                        else {
                            Com_Printf(" ^3Limits:^7 %d ^7to^7 %d\n",
                                cevar->limits.i.min, cevar->limits.i.max);
                        }
                    }
                }
            }
            });

        Memory::VP::InjectHook(exe(0x40D043, 0x40DE83), cvar_show_com_printf);

        // remove \n from ````"\"%s\" is:\"%s^7\" default:\"%s^7\"\n````
        Memory::VP::Patch((*(uintptr_t*)pat.get_first(-4)) +29, 0);
    }

    pat = hook::pattern("53 8B 5C 24 ? 56 8B 74 24 ? 57 53");
        
        if(!pat.empty())
        Cvar_Set_og = safetyhook::create_inline(pat.get_first(), Cvar_Set);
        pat = hook::pattern("E8 ? ? ? ? 83 C4 ? 8D 46 ? 5B 83 C4");
        if(!pat.empty())
        static auto blur_test = safetyhook::create_mid(pat.get_first(), [](SafetyHookContext& ctx) {

            static float fuck[4]{};
            float* args = (float*)(ctx.esp);


            args[0] -= process_widths();

            args[2] += process_widths() * 2.f;

            });
    
        pat = hook::pattern("FF 15 ? ? ? ? 8B 44 24 ? 8B 4C 24 ? 2B 4C 24");

        if (!pat.empty()) {
            // Borderless
            printf(" HOOKING window thing\n");
            static auto Borderless = safetyhook::create_mid(pat.get_first(), [](SafetyHookContext& ctx) {
                printf("window thing noborder cvar: %p\n", r_noborder);
                if (r_noborder && r_noborder->integer) {
                    auto borderless_style = WS_POPUP | WS_VISIBLE;
                    printf("first %p\n second %p\n", *(int*)(ctx.esp + 0x5C), *(int*)(ctx.esp + 0x4));
                    *(int*)(ctx.esp + 0x5C) = borderless_style;
                    *(int*)(ctx.esp + 0x4) = borderless_style;

                }

                });
        }

        pat = hook::pattern("8B 15 ? ? ? ? 8B 42 ? 5F");

        if (!pat.empty()) {
            static auto r_mode_auto_hook = safetyhook::create_mid(pat.get_first(), [](SafetyHookContext& ctx) {

                unsigned int* width = (unsigned int*)ctx.esi;
                unsigned int* height = (unsigned int*)ctx.edx;

                if (r_mode_auto && r_mode_auto->base->integer) {
                    *width = GetSystemMetrics(SM_CXSCREEN);
                    *height = GetSystemMetrics(SM_CYSCREEN);
                }

                });
        }

        pat = hook::pattern("C7 44 24 ? ? ? ? ? C7 44 24 ? ? ? ? ? EB ? 57");

        if (!pat.empty()) {
            Memory::VP::Patch<uint32_t>(pat.get_first(4), WS_EX_LEFT);
        }

    pat = hook::pattern("68 ? ? ? ? 56 FF 15 ? ? ? ? 83 C4 ? 5F");
    if (!pat.empty()) {
        static auto R_init_end = safetyhook::create_mid(pat.get_first(), [](SafetyHookContext& ctx) {
            if(cg_fixedAspect)
            Resolution_Static_mod(cg_fixedAspect->base);

            });
    }

    pat = hook::pattern("? ? ? ? ? ? 6A 00 6A 00 51 ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? 50");
    if (!pat.empty()) {
        Memory::VP::Patch<int*>(pat.get_first(2), resolution_modded);
    }

     Memory::VP::Patch<int*>(exe((0x0040A33F + 0x2), (0x0040B183 + 0x2)), resolution_modded);
    


        Memory::VP::Patch<int*>(exe((0x0040A20E + 1),(0x0040B08B + 2)), resolution_modded);
    

    pat = find_pattern("? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? E8 ? ? ? ? 83 F8 ? 89 44 24 ? 7D ? 89 6C 24","? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? E8 ? ? ? ? 83 F8 ? 89 44 24 ? 7D ? C7 44 24 ? 00 00 00 00");
    if (!pat.empty()) {
        Memory::VP::Patch<int*>(pat.get_first(2), resolution_modded);
    }
    pat = find_pattern("? ? ? ? ? ? 6A 00 6A 00 C7 44 24 ? ? ? ? ? ? ? ? ? ? ? C7 44 24","? ? ? ? ? ? C7 44 24 ? ? ? ? ? C7 44 24 ? ? ? ? ? C7 44 24 ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? 8B 44 24");
    if (!pat.empty()) {
        Memory::VP::Patch<int*>(pat.get_first(2), resolution_modded);
    }
    pat = hook::pattern("? ? ? ? ? ? 33 FF 85 D2 0F 95 C2");
    if (!pat.empty()) {
        Memory::VP::Patch<int*>(pat.get_first(2), resolution_modded);
    }

    pat = hook::pattern("A1 ? ? ? ? 3D ? ? ? ? 56");
    if (!pat.empty()) {
        Memory::VP::Patch<int*>(pat.get_first(1), resolution_modded);
    }

    SetUpFunctions();
    LoadMenuConfigs();
    LoadHudShaderConfigs();

    HHOOK windowsHook = SetWindowsHookExA(WH_CALLWNDPROC, [](int code, WPARAM w, LPARAM l) -> LRESULT {
    if (code < 0) return CallNextHookEx(NULL, code, w, l);

    CWPSTRUCT* p = reinterpret_cast<CWPSTRUCT*>(l);
    static HWND hConsole = NULL;
    static bool isWine = false;
    static char windowTitle[256];
    static constexpr char CODUOSPWINDOW[] = "CoD:United Offensive";
    static constexpr char CODUOMPWINDOW[] = "Multiplayer";
    static constexpr char EXCEPTIONTRACERWINDOW[] = "Application Crash";
    static constexpr char EXTERNALCONSOLEWINDOW[] = "Console";

    if (!hConsole) {
        hConsole = GetConsoleWindow();
        typedef const char* (CDECL *wine_get_version_t)(void);
        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        wine_get_version_t pwine_get_version = NULL;
        if (hNtdll) {
            pwine_get_version = (wine_get_version_t)GetProcAddress(hNtdll, "wine_get_version");
        }
        isWine = (pwine_get_version != NULL);
    }

    // Filter messages
    static std::unordered_map<HWND, uint8_t> windowsHookCache;
    // Bits: 0 = CoDUO(SP/MP), 1 = ExceptionTracer, 2 = ExternalConsole, 3 = AlreadyProcessed
    if (p->message != WM_CREATE && p->message != WM_SIZE) {
        return CallNextHookEx(NULL, code, w, l);
    }

    // Check the cache
    auto it = windowsHookCache.find(p->hwnd);
    if (it != windowsHookCache.end()) {
        uint8_t flags = it->second;
        if (flags & (1<<1)) return CallNextHookEx(NULL, code, w, l);  // ExceptionTracer
        if (p->message == WM_SIZE && (flags & 8)) return CallNextHookEx(NULL, code, w, l);  // AlreadyProcessed
    }

    // Check for the game's own windows
    GetWindowTextA(p->hwnd, windowTitle, sizeof(windowTitle));
    bool isCoDUOSP = (strcmp(windowTitle, CODUOSPWINDOW) == 0);
    bool isCoDUOMP = (strstr(windowTitle, CODUOMPWINDOW) != nullptr);
    bool isExceptionTracer = (strcmp(windowTitle, EXCEPTIONTRACERWINDOW) == 0);
    bool isExternalConsole = (strstr(windowTitle, EXTERNALCONSOLEWINDOW) != nullptr);

    // Cache the window type
    uint8_t flags = 0;
    if (isCoDUOSP || isCoDUOMP) flags |= 1;      // Bit 0: CoDUO(SP/MP)
    else if (isExceptionTracer) flags |= (1<<1); // Bit 1: ExceptionTracer
    else if (isExternalConsole) flags |= (1<<2); // Bit 2: ExternalConsole
    windowsHookCache[p->hwnd] = flags;

    if (isExceptionTracer) return CallNextHookEx(NULL, code, w, l);

    if (p->message == WM_CREATE) {
        // Set the game icon for all windows
        if (hConsole == NULL || isWine) {
            char modulePath[MAX_PATH];
            if (GetModuleFileNameA(NULL, modulePath, sizeof(modulePath))) {
                HICON hIcon = ExtractIconA(NULL, modulePath, 0);
                if (hIcon && hIcon != (HICON)INVALID_HANDLE_VALUE) {
                    HICON hDup = CopyIcon(hIcon);
                    SetClassLongPtrW(p->hwnd, GCLP_HICON, reinterpret_cast<LONG_PTR>(hDup));
                    DestroyIcon(hIcon);
                }
            }
        }
        else if (hConsole != p->hwnd) {
            HICON hIconBig = reinterpret_cast<HICON>(GetClassLongPtrA(hConsole, GCLP_HICON));
            HICON hIconSmall = reinterpret_cast<HICON>(GetClassLongPtrA(hConsole, GCLP_HICONSM));
            if (hIconBig) SetClassLongPtrA(p->hwnd, GCLP_HICON, reinterpret_cast<LONG_PTR>(hIconBig));
            if (hIconSmall) SetClassLongPtrA(p->hwnd, GCLP_HICONSM, reinterpret_cast<LONG_PTR>(hIconSmall));
        }

        // Exclude the splash screen
        if (!(GetWindowLongA(p->hwnd, GWL_STYLE) & WS_DLGFRAME)) {
            return CallNextHookEx(NULL, code, w, l);
        }

        if (isCoDUOSP || isCoDUOMP) {
        // Add minimize button for game windows
            LONG style = GetWindowLongA(p->hwnd, GWL_STYLE);
            SetWindowLongA(p->hwnd, GWL_STYLE, style | WS_MINIMIZEBOX);

        // Automatically apply dark titlebar to game windows
            if (!isWine && p->message == WM_CREATE) {
                BOOL darkMode = TRUE;
                // DWMWA_USE_IMMERSIVE_DARK_MODE
                if (hConsole && hConsole != p->hwnd) DwmGetWindowAttribute(hConsole, 20, &darkMode, sizeof(darkMode));
                if (FAILED(DwmSetWindowAttribute(p->hwnd, 20, &darkMode, sizeof(darkMode)))) {
                    // XP/Vista/7/DWM-off
                }
            }
        }

        // Unhide external console
        else if (isExternalConsole) {
            ShowWindow(p->hwnd, SW_SHOW);
            SetWindowPos(p->hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            PostMessage(p->hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
        }
    }

    // Move windows to the foreground
    else if (p->message == WM_SIZE && !isExternalConsole) {
        LONG style = GetWindowLongA(p->hwnd, GWL_STYLE);
        if ((style & WS_SYSMENU) == 0) return CallNextHookEx(NULL, code, w, l);

        windowsHookCache[p->hwnd] |= 8;  // Bit 3: AlreadyProcessed

        // Detect only for new windows
        auto it2 = windowsHookCache.find(p->hwnd);
        if (it2 != windowsHookCache.end() && (it2->second & 8)) {
            return CallNextHookEx(NULL, code, w, l);
        }

        WINDOWPLACEMENT wp = { sizeof(wp) };
        if (GetWindowPlacement(p->hwnd, &wp)) {
            AllowSetForegroundWindow(ASFW_ANY);
            if (wp.showCmd != SW_NORMAL) {
                ShowWindow(p->hwnd, SW_RESTORE);
            }
            SetWindowPos(p->hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
            SetWindowPos(p->hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
            SetForegroundWindow(p->hwnd);
        }
    }
    return CallNextHookEx(NULL, code, w, l); }, NULL, GetCurrentThreadId());

    //pat = hook::pattern("83 C4 ? 8D 4C 24 ? 68 ? ? ? ? 51 E8 ? ? ? ? 8D 54 24");
    //if(!pat.empty())
    //    static auto AfterCvars = safetyhook::create_mid(pat.get_first(), sub_431CA0);

    Memory::VP::InterceptCall(exe(0x4ADADF, 0x4C4EFF), cvar_init_og, Cvar_Init_hook);

    

    //Memory::VP::InjectHook(0x00411757, Con_DrawConsole);

    //SCR_AdjustFrom640_OG = safetyhook::create_inline(0x411070, SCR_AdjustFrom640);

    LoadLibraryD = safetyhook::create_inline(LoadLibraryA, LoadLibraryHook);

    FreeLibraryD = safetyhook::create_inline(FreeLibrary, FreeLibraryHook);



    pat = hook::pattern("FF 15 ? ? ? ? 68 ? ? ? ? FF 15 ? ? ? ? FF 15 ? ? ? ? B8");
    if (!pat.empty()) {
        Memory::VP::Read(pat.get_first(2), glOrtho_ptr);
        Memory::VP::Nop(pat.get_first(0), 6);
        Memory::VP::InjectHook(pat.get_first(0), glOrtho_detour, Memory::VP::HookType::Call);
    }

    pat = hook::pattern("85 C0 0F 85 ? ? ? ? A1 ? ? ? ? 85 C0 74 ? A1");
    if (!pat.empty()) {
        static auto shutdown = safetyhook::create_mid(pat.get_first(-5), [](SafetyHookContext& ctx) {
            is_shutdown = true;
            component_loader::pre_destroy();
            // Required for steam exes otherwise crashes on unhooking
            ShutdownAllHooks();

            });
    }

    pat = hook::pattern("A1 ? ? ? ? 8B 48 ? 53 33 DB 3B CB 74");

    if (!pat.empty()) {
        static auto vid_restart = safetyhook::create_mid(pat.get_first(), [](SafetyHookContext& ctx) {

            cg_game_offset = 0;
            ui_offset = 0;
            game_offset = 0;

            });
    }

}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        OpenConsole();
        InitHook();
        //CheckModule();
        HMODULE moduleHandle;
        // idk why but this makes it not DETATCH prematurely
        GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)DllMain, &moduleHandle);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:

        //is_shutdown = true;

        break;
    }
    return TRUE;
}

