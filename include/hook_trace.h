#pragma once

// diagnostic logging (hook trace and stability messages), Debug builds only
#ifdef _DEBUG
#define YAP_HOOK_TRACE 1
#else
#define YAP_HOOK_TRACE 0
#endif

#if YAP_HOOK_TRACE
#include <cstdio>
#define YAP_LOG(...) printf(__VA_ARGS__)
#else
#define YAP_LOG(...) ((void)0)
#endif

#if YAP_HOOK_TRACE
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <source_location>

inline void yap_trace_target(const char* kind, const void* address, const char* status, const char* file = nullptr, unsigned line = 0)
{
	char module_path[MAX_PATH] = "?";
	const char* name = module_path;
	uintptr_t rva = (uintptr_t)address;
	HMODULE module = nullptr;
	if (address && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)address, &module) && module) {
		GetModuleFileNameA(module, module_path, MAX_PATH);
		name = strrchr(module_path, '\\');
		name = name ? name + 1 : module_path;
		rva = (uintptr_t)address - (uintptr_t)module;
	}

	if (file) {
		const char* base = strrchr(file, '\\');
		base = base ? base + 1 : file;
		printf("[hook] %-12s %-4s %s+0x%X (%s:%u)\n", kind, status, name, (unsigned)rva, base, line);
	}
	else
		printf("[hook] %-12s %-4s %s+0x%X\n", kind, status, name, (unsigned)rva);
}

inline bool yap_trace_check(const char* kind, uintptr_t address, bool valid, bool write, const std::source_location& loc)
{
	if (!valid)
		printf("[hook] %-12s SKIP address %08X (%s:%u)\n", kind, (unsigned)address, strrchr(loc.file_name(), '\\') ? strrchr(loc.file_name(), '\\') + 1 : loc.file_name(), (unsigned)loc.line());
	else if (write)
		yap_trace_target(kind, (const void*)address, "ok", loc.file_name(), (unsigned)loc.line());
	return valid;
}
#endif
