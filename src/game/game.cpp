#include "game.h"
#include "../framework.h"
#include "../structs.h"
#include "../loader/component_loader.h"
#include "Hooking.Patterns.h"
uintptr_t cg_game_offset = 0;
uintptr_t ui_offset = 0;
uintptr_t game_offset = 0;
uint32_t* player_flags = nullptr;
Com_PrintfT Com_Printf = (Com_PrintfT)NULL;

__declspec(naked) void retptr() {
	__asm {
		ret
	}
}

namespace game {
	//void __cdecl Cmd_AddCommand(const char* command_name, void* function) {
	//	if(exe(0x00422860))
	//	cdecl_call<int>(exe(0x00422860), command_name, function);
	//}

	int FS_FOpenFileByMode(const char* qpath, fileHandle_t* f, fsMode_t mode) {
		return cdecl_call<int>(exe(0x00427240, 0x430E90), qpath, f, mode);
	}

	int FS_FCloseFile(fileHandle_t* f) {
		return cdecl_call<int>(exe(0x00423510, 0x42D0D0), f);
	}

	int FS_GetFileList_CALL(const char* path, const char* extension, char* listbuf, int bufsize, uintptr_t CALLADDRESS)
	{
		int result;
		__asm
		{
			mov eax,path
			push bufsize
			push listbuf
			push extension
			call CALLADDRESS
			add esp, 0xC
			mov result, eax
		}
		return result;
	}

	int FS_GetFileList(const char* path, const char* extension, char* listbuf, int bufsize)
	{
		return FS_GetFileList_CALL(path, extension, listbuf,bufsize ,exe(0x425730, 0x42F380));
	}

	void SCR_DrawStringExt(int x, int y, float size, const char* string, float* setColor, qboolean forceColor) {
		auto ptr = exe(0x4E1120, 0x4E9C70);
		if (ptr) {
			cdecl_call<void>(ptr, x, y, size, string, setColor, forceColor);
		}
	}

	uintptr_t SCR_DrawString_addr;
	void SCR_DrawString(float x, float y, int fontID, float scale, float* color, const char* text, float spaceBetweenChars, int maxChars, int arg9) {
		if (!SCR_DrawString_addr)
			return;
		cdecl_call<void>(SCR_DrawString_addr, x, y, fontID, scale, color, text, spaceBetweenChars, maxChars, arg9);
	}

	class component final : public component_interface
	{
	public:
		void post_unpack() override
		{
			auto pattern = hook::pattern("8B 44 24 ? 8B 4C 24 ? 8B 54 24 ? 50 8B 44 24 ? 51 8B 4C 24 ? 52 8B 54 24 ? 6A 00");
			if (!pattern.empty()) {
				SCR_DrawString_addr = (uintptr_t)pattern.get_first();
			}
			

		}
	};

}
REGISTER_COMPONENT(game::component);