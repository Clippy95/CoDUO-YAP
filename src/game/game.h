#pragma once
#include <helper.hpp>
#include "..\structs.h"
#include "..\loader\component_loader.h"
#include <Hooking.Patterns.h>
#include "..\framework.h"
#define WEAK __declspec(selectany)

extern uintptr_t cg_game_offset;
extern uintptr_t ui_offset;
extern uintptr_t game_offset;

constexpr auto BASE_GAME = 0x20000000;
constexpr auto BASE_CGAME = 0x30000000;
constexpr auto BASE_UI = 0x40000000;

uintptr_t cg(uintptr_t CODUOSP, uintptr_t CODUOMP = 0);

template <typename T>
inline T* cg_ptr(uintptr_t CODUOSP, uintptr_t CODUOMP = 0)
{
	return reinterpret_cast<T*>(cg(CODUOSP, CODUOMP));
}

template <typename T>
inline T cg_read(uintptr_t CODUOSP, T fallback = T{}, uintptr_t CODUOMP = 0)
{
	T* p = cg_ptr<T>(CODUOSP, CODUOMP);
	return p ? *p : fallback;
}

uintptr_t ui(uintptr_t CODUOSP, uintptr_t CODUOMP = 0);

uintptr_t g(uintptr_t CODUOSP, uintptr_t CODUOMP = 0);


uintptr_t exe(uintptr_t CODUOSP, uintptr_t CODUOMP = 0);

uintptr_t sp_mp(uintptr_t CODUOSP, uintptr_t CODUOMP = 0);

namespace game
{


	template <typename T>
	class symbol
	{
	public:
		symbol(const size_t sp_address, const size_t mp_address = 0, const ptrdiff_t offset = 0) :
			sp_object(reinterpret_cast<T*>(sp_address)),
			mp_object(reinterpret_cast<T*>(mp_address)),
			offset(offset)
		{
		}

		T* get() const
		{
			T* ptr = exe(1, 0) ? sp_object : mp_object;
			uintptr_t base_address = 0;

			switch (offset)
			{
			case BASE_CGAME:
				base_address = cg_game_offset;
				break;
			case BASE_UI:
				base_address = ui_offset;
				break;
			default:
				return ptr;
			}

			return reinterpret_cast<T*>(base_address + (reinterpret_cast<uintptr_t>(ptr) - offset));
		}

		operator T* () const
		{
			return this->get();
		}

		T* operator->() const
		{
			return this->get();
		}
	private:
		T* sp_object;
		T* mp_object;
		ptrdiff_t offset;
	};

	enum game_module {
		EXE,
		CGAME,
		UI,
		GAME,
		MAX_GAME_MODULE,
	};

    template <typename T>
    class symbolp
    {
    public:
        symbolp(std::initializer_list<std::string_view> patterns, const signed int offset_val = 0, game_module module = EXE, bool dereference = false)
            : patterns(patterns), offset_val(offset_val), module(module), dereference(dereference), object(nullptr)
        {
            register_as_component();
        }

        symbolp(std::string_view pattern_str, const signed int offset_val = 0, game_module module = EXE, bool dereference = false)
            : patterns({ pattern_str }), offset_val(offset_val), module(module), dereference(dereference), object(nullptr)
        {
            register_as_component();
        }

        T* get() const
        {
            return object;
        }

        operator T* () const
        {
            return this->get();
        }

        T* operator->() const
        {
            return this->get();
        }

        //bool operator==(const T& other) const {
        //    return (this->get() == other);
        //}

    private:
        void register_as_component()
        {
            class symbolp_resolver : public component_interface
            {
            public:
                symbolp_resolver(symbolp* owner, game_module module) : owner(owner), module(module) {}

                void post_start() override
                {
                    if (module == EXE) {
                        owner->resolve();
                    }
                }

                void post_cgame() override {
                    if (module == CGAME)
                        owner->resolve();
                }

                void post_ui() override {
                    if (module == UI)
                        owner->resolve();
                }

                void post_game_sp() override {
                
                    if (module == GAME)
                        owner->resolve();

                }

            private:
                symbolp* owner;
                game_module module;
            };

            auto resolver = std::make_unique<symbolp_resolver>(this, module);
            component_loader::register_component(std::move(resolver));
        }

        void resolve()
        {
            uintptr_t search_base = (uintptr_t)GetModuleHandle(NULL);

            switch (module)
            {
            case CGAME:
                search_base = cg_game_offset;
                break;
            case UI:
                search_base = ui_offset;
                break;
            case GAME:
                search_base = game_offset;
                break;
            }


            for (const auto& pattern_str : patterns)
            {
                hook::pattern pattern((HMODULE)search_base, pattern_str);

                if (!pattern.empty())
                {
                    uintptr_t address = reinterpret_cast<uintptr_t>(pattern.get_first()) + offset_val;

                    if (dereference)
                    {
                        object = *reinterpret_cast<T**>(address);
                    }
                    else
                    {
                        object = reinterpret_cast<T*>(address);
                    }
                    return;
                }
            }

            // All patterns failed - build error message
            object = nullptr;

            std::string error_msg = "symbolp: All patterns failed to resolve!\nPatterns tried:\n";
            for (size_t i = 0; i < patterns.size(); ++i)
            {
                error_msg += std::to_string(i + 1) + ": \"" + std::string(patterns[i]) + "\"\n";
            }

            MessageBoxA(NULL, error_msg.c_str(), MOD_NAME": Error", MB_ICONSTOP);
        }

        std::vector<std::string_view> patterns;
        signed int offset_val;
        game_module module;
        bool dereference;
        mutable T* object;
    };

}


typedef int(__cdecl* Com_PrintfT)(const char* format, ...);
extern Com_PrintfT Com_Printf;
extern uint32_t* player_flags;

namespace game {
	// from https://gitlab.com/devcod-group/t1x/t1x-client/-/blob/stable/src/client/stock/types.h
	typedef struct weaponinfo_t
	{
		int number; //0x0000
		char* name; //0x0004
		char* displayName; //0x0008
		char* AIOverlayDescription; //0x000C
		char* gunModel; //0x0010
		char* handModel; //0x0014
		char pad_0018[4]; //0x0018
		char* idleAnim; //0x001C
		char* emptyIdleAnim; //0x0020
		char* fireAnim; //0x0024
		char* holdFireAnim; //0x0028
		char* lastShotAnim; //0x002C
		char pad_0030[4]; //0x0030
		char* meleeAnim; //0x0034
		char* reloadAnim; //0x0038
		char* reloadEmptyAnim; //0x003C
		char pad_0040[8]; //0x0040
		char* raiseAnim; //0x0048
		char* dropAnim; //0x004C
		char* altRaiseAnim; //0x0050
		char* altDropAnim; //0x0054
		char* adsFireAnim; //0x0058
		char* adsLastShotAnim; //0x005C
		char pad_0060[16]; //0x0060
		char* adsUpAnim; //0x0070
		char* adsDownAnim; //0x0074
		char* modeName; //0x0078
		char pad_007C[24]; //0x007C
		char* viewFlashEffect; //0x0094
		char* worldFlashEffect; //0x0098
		char* pickupSound; //0x009C
		char pad_00A0[8]; //0x00A0
		char* pullbackSound; //0x00A8
		char* fireSound; //0x00AC
		char pad_00B0[20]; //0x00B0
		char* reloadSound; //0x00C4
		char* reloadEmptySound; //0x00C8
		char pad_00CC[12]; //0x00CC
		char* altSwitchSound; //0x00D8
		char pad_00DC[36]; //0x00DC
		char* shellEjectEffect; //0x0100
		char pad_0104[4]; //0x0104
		char* reticle; //0x0108
		char* reticleSide; //0x010C
		char pad_0110[180]; //0x0110
		char* radiantName; //0x01C4
		char* worldModel; //0x01C8
		char pad_01CC[8]; //0x01CC
		char* hudIcon; //0x01D4
		char* modeIcon; //0x01D8
		char* ammoIcon; //0x01DC
		int startAmmo; //0x01E0
		char* ammoName;
		int ammoOrClipIndex; // not sure
		char* clipName;
		int clientIndex; //0x01F0 // could this be clipIndex?
		int maxAmmo; //0x01F4
		int clipSize; //0x01F8
		char* sharedAmmoCapName; //0x01FC
		char pad_0200[8]; //0x0200
		int damage; //0x0208
		char gap1[4];
		int minDamagePercent;
		int damageInnerRadius; //0x0214
		int damageOuterRadius; //0x0218
		char gap2[44];
		int reloadAddTime;
		char gap3[28];
		int fuseTime; //0x0268
		float moveSpeedScale; // 0x026c
		float adsSensitivity; // 0x0270
		float adsZoomFov; // 0x0274 <-----|
		float adsZoomInFrac; // 0x0278 <--|--- just guessed these and they seem to work
		float adsZoomOutFrac; // 0x027c <-|
		char* adsOverlayShader; // 0x0280
		char gap4[4]; // not sure if this is adsOverlayReticle
		float adsOverlayWidth;
		float adsOverlayHeight;
		float adsBobFactor;
		float adsViewBobMult;
		float hipSpreadStandMin;
		float hipSpreadDuckedMin;
		float hipSpreadProneMin;
		float hipSpreadMax;
		float hipSpreadDecayRate;
		float hipSpreadFireAdd;
		float hipSpreadTurnAdd;
		float hipSpreadMoveAdd;
		float hipSpreadDuckedDecay;
		float hipSpreadProneDecay;
		float hipReticleSidePos;
		int adsTransInTime;
		int adsTransOutTime;
		float adsIdleAmount;
		float hipIdleAmount;
		float idleCrouchFactor;
		float idleProneFactor;
		float gunMaxPitch;
		float gunMaxYaw;
		float swayMaxAngle;
		float swayLerpSpeed;
		float swayPitchScale;
		float swayYawScale;
		float swayHorizScale;
		float swayVertScale;
		float swayShellShockScale;
		float adsSwayMaxAngle;
		float adsSwayLerpSpeed;
		float adsSwayPitchScale;
		float adsSwayYawScale;
		float adsSwayHorizScale;
		float adsSwayVertScale;
		int twoHanded;
		int rifleBullet;
		int semiAuto;
		int boltAction;
		int aimDownSight;
		int rechamberWhileAds;
		float adsViewErrorMin;
		float adsViewErrorMax;
		char gap5[12];
		int wideListIcon;
		char gap6[8];
		char* killIcon; //0x0350
		char pad_0354[20]; //0x0354
		char* altWeapon; //0x0368
		char pad_036C[12]; //0x036C
		int explosionRadius; //0x0378
		int explosionInnerDamage; //0x037C
		int explosionOuterDamage; //0x0380
		char pad_0384[8]; //0x0384
		char* projectileModel; //0x038C
		char gap7[292];
		float OOPosAnimLength[2]; // 0x4b4
		//...
	} weaponinfo_t;
}

namespace game {
	//void __cdecl Cmd_AddCommand(const char* command_name, void* function);

	extern int FS_FOpenFileByMode(const char* qpath, fileHandle_t* f, fsMode_t mode);

	extern int FS_FCloseFile(fileHandle_t* f);
	extern int FS_GetFileList(const char* path, const char* extension, char* listbuf, int bufsize);
	extern void SCR_DrawStringExt(int x, int y, float size, const char* string, float* setColor, qboolean forceColor);
	extern void SCR_DrawString(float x, float y, int fontID, float scale, float* color, const char* text, float spaceBetweenChars, int maxChars, int arg9);

	//WEAK symbol<qhandle_t> whiteShader{ 0x48422E8 };

    WEAK symbolp<qhandle_t> whiteShader{ "A1 ? ? ? ? 8B 15 ? ? ? ? ? ? ? 83 C4",1,game::EXE,true};
    WEAK symbolp<void(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader)> drawStretchPic{ "8B 0D ? ? ? ? 8B 81 ? ? ? ? 83 C0 ? 3D ? ? ? ? 0F 87 ? ? ? ? 56 8D B4 08 ? ? ? ? 85 F6 89 81 ? ? ? ? 74" };

    WEAK symbolp<int(const char* command_name, void* function)> Cmd_AddCommand{ "85 C0 53 55 8B 6C 24 ? 56 57 8B F8 74 ? 8B 77",-5 };

	WEAK symbolp<void*(size_t size)> Hunk_AllocateTempMemory{ "85 C9 56 57 75 ? 8B 7C 24",-6 };

	//WEAK symbol<void(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader)> drawStretchPic{ 0x4DF240 };
	WEAK symbol<void(float *color)> SetColor{ 0x4DF1B0, 0x4F00F0 };
    WEAK symbolp<qhandle_t> cstate{ "A1 ? ? ? ? 83 E8 00",1,game::EXE,true };
    WEAK symbolp<qhandle_t> keycatchers{ "F6 05 ? ? ? ? ? 74 ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? DF E0",2,game::EXE,true };

	WEAK symbol<weaponinfo_t*> PlayerCurrentWeapon{ 0x30275DF0,0x30487980,BASE_CGAME };
	WEAK symbol<weaponinfo_t*> ADS_value{ 0x3026DAB4,0x304832A4,BASE_CGAME };

}
extern void retptr();
