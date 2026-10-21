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
namespace sound {
#define CHANNEL_AUTO 0
#define CHANNEL_MENU 1
#define CHANNEL_WEAPON 2
#define CHANNEL_VOICE 3
#define CHANNEL_ITEM 4
#define CHANNEL_BODY 5
#define CHANNEL_LOCAL 6
#define CHANNEL_MUSIC 7
#define CHANNEL_ANNOUNCER 8
#define CHANNEL_SHELLSHOCK 9

	enum g_snd_channelvol_array {
		AUTO,
		MENU,
		WEAPON,
		VOICE,
		ITEM,
		BODY,
		LOCAL,
		MUSIC,
		ANNOUNCER,
		SHELLSHOCK,
		MAX_CHANNELVOL_ARRAY
	};

	cevar_s* snd_test_volume[MAX_CHANNELVOL_ARRAY]{};
	cevar_s* mss_volume_music{};
	cevar_s* mss_volume_effects{};
	cevar_s* mss_volume_voice{};

	const char* channelNames[] = {
		"auto",
		"menu",
		"weapon",
		"voice",
		"item",
		"body",
		"local",
		"music",
		"announcer",
		"shellshock"
	};

	float GetChannelGain(int index) {
		if (index < 0 || index >= MAX_CHANNELVOL_ARRAY || !snd_test_volume[index] ||
			!snd_test_volume[index]->base || !mss_volume_music || !mss_volume_music->base ||
			!mss_volume_effects || !mss_volume_effects->base ||
			!mss_volume_voice || !mss_volume_voice->base) {
			return 1.0f;
		}

		float category_multiplier = 1.0f;

		if (index == MUSIC || index == MENU) {
			category_multiplier = mss_volume_music->base->value;
		}
		else if (index == VOICE || index == ANNOUNCER) {
			category_multiplier = mss_volume_voice->base->value;
		}
		else {
			category_multiplier = mss_volume_effects->base->value;
		}

		return category_multiplier * snd_test_volume[index]->base->value;
	}

	uintptr_t MSS_FadeSelectSounds_original = 0x445F90;
	void MSS_FadeSelectSounds(float* volumes, float unk) {
		__asm
		{
			mov eax, volumes
			push unk
			call MSS_FadeSelectSounds_original
			add esp, 4
		}
	}

	void MSS_FadeSelectSounds_hook(float* volumes, float unk) {
		if (!volumes) {
			MSS_FadeSelectSounds(volumes, unk);
			return;
		}

		float channelvolume[MAX_CHANNELVOL_ARRAY]{};
		memcpy(channelvolume, volumes, sizeof(channelvolume));

		for (int i = 0; i < MAX_CHANNELVOL_ARRAY; i++) {
			channelvolume[i] *= GetChannelGain(i);
		}

		MSS_FadeSelectSounds(channelvolume, unk);
	}

	uintptr_t MSS_FadeSelectSounds_return_jmp = 0x40268A;
	void __declspec(naked) MSS_FadeSelectSounds_stub() {
		__asm
		{
			push eax
			call MSS_FadeSelectSounds_hook
			add esp, 4
			jmp MSS_FadeSelectSounds_return_jmp
		}
	}

	class component final : public component_interface
	{
	public:
		void post_unpack() override
		{
			for (int i = 0; i < MAX_CHANNELVOL_ARRAY; i++) {
				auto string = std::string("snd_volume_") + channelNames[i];
				char* cstr = new char[string.length() + 1];
				std::strcpy(cstr, string.c_str());
				snd_test_volume[i] = Cevar_Get(cstr, 1.f, 0, 0.f, 5.f);
			}

			mss_volume_music = Cevar_Get("mss_volume_music", 1.f, CVAR_ARCHIVE, 0.f, 1.f);
			mss_volume_effects = Cevar_Get("mss_volume_effects", 1.f, CVAR_ARCHIVE, 0.f, 1.f);
			mss_volume_voice = Cevar_Get("mss_volume_voice", 1.f, CVAR_ARCHIVE, 0.f, 1.f);

			MSS_FadeSelectSounds_return_jmp = exe(0x40268A, 0x402A60);
			MSS_FadeSelectSounds_original = exe(0x445F90, 0x453420);
			Memory::VP::InjectHook(exe(0x402685, 0x402A5B), MSS_FadeSelectSounds_stub, Memory::VP::HookType::Jump);
		}
	};
}
REGISTER_COMPONENT(sound::component);