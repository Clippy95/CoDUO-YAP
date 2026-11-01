#pragma once
#include "shared.h"
#include <unordered_map>
#include <string>

typedef struct cvar_s
{
    char* name;
    char* string;
    char* resetString;
    char* latchedString;
    int flags;
    qboolean modified;
    int modificationCount;
    float value;
    int integer;
    struct cvar_s* next;
    struct cvar_s* hashNext;
} cvar_t;

typedef void (*CvarCallback)(cvar_t* cvar, const char* oldValue);

cvar_s* __cdecl Cvar_Find(const char* a1);

struct cevar_limits {
    union {
        struct {
            float min;
            float max;
        } f;
        struct {
            int min;
            int max;
        } i;
    };
    bool has_limits;
    bool is_float;
};

typedef struct cevar_s {
    cvar_t* base;
    CvarCallback callback;
    cevar_limits limits;
} cevar_t;

// ============================================================================
// CEVAR SYSTEM - GLOBAL STORAGE
// ============================================================================
extern std::unordered_map<cvar_t*, cevar_t*> g_cevars;

inline float Cevar_Value(const cevar_t* c, float fallback = 0.f) {
    return (c && c->base) ? c->base->value : fallback;
}

inline int Cevar_Int(const cevar_t* c, int fallback = 0) {
    return (c && c->base) ? c->base->integer : fallback;
}

// ============================================================================
// CEVAR HELPER - Get cevar from cvar without modifying struct
// ============================================================================
inline cevar_t* Cevar_FromCvar(cvar_t* cvar) {
    if (!cvar) return nullptr;

    auto it = g_cevars.find(cvar);
    if (it != g_cevars.end()) {
        return it->second;
    }
    return nullptr;
}

inline cevar_t* Cevar_FromCvar(const char* cvar_name) {
    if (!cvar_name) return nullptr;
    cvar_t* cvar = Cvar_Find(cvar_name);
    return Cevar_FromCvar(cvar);
}

// ============================================================================
// CEVAR CREATION FUNCTIONS - STRING OVERLOADS
// ============================================================================
extern cevar_t* Cevar_Get(const char* var_name, const char* var_value, int flags,
    CvarCallback callback = nullptr);

// ============================================================================
// CEVAR CREATION FUNCTIONS - FLOAT OVERLOADS
// ============================================================================
extern cevar_t* Cevar_Get(const char* var_name, float var_value, int flags,
    CvarCallback callback = nullptr);

extern cevar_t* Cevar_Get(const char* var_name, float var_value, int flags,
    float min, float max, CvarCallback callback = nullptr);

// ============================================================================
// CEVAR CREATION FUNCTIONS - INT OVERLOADS
// ============================================================================
extern cevar_t* Cevar_Get(const char* var_name, int var_value, int flags,
    CvarCallback callback = nullptr);

extern cevar_t* Cevar_Get(const char* var_name, int var_value, int flags,
    int min, int max, CvarCallback callback = nullptr);

// ============================================================================
// CEVAR UTILITY FUNCTIONS
// ============================================================================

// Check if a cvar has an associated cevar
inline bool Cevar_Exists(cvar_t* cvar) {
    return Cevar_FromCvar(cvar) != nullptr;
}

// Get callback from cvar (returns nullptr if no cevar exists)
inline CvarCallback Cevar_GetCallback(cvar_t* cvar) {
    cevar_t* cevar = Cevar_FromCvar(cvar);
    return cevar ? cevar->callback : nullptr;
}

// Check if cvar has limits
inline bool Cevar_HasLimits(cvar_t* cvar) {
    cevar_t* cevar = Cevar_FromCvar(cvar);
    return cevar ? cevar->limits.has_limits : false;
}

// Get limits (returns false if no limits exist)
inline bool Cevar_GetFloatLimits(cvar_t* cvar, float* min, float* max) {
    cevar_t* cevar = Cevar_FromCvar(cvar);
    if (cevar && cevar->limits.has_limits && cevar->limits.is_float) {
        if (min) *min = cevar->limits.f.min;
        if (max) *max = cevar->limits.f.max;
        return true;
    }
    return false;
}

inline bool Cevar_GetIntLimits(cvar_t* cvar, int* min, int* max) {
    cevar_t* cevar = Cevar_FromCvar(cvar);
    if (cevar && cevar->limits.has_limits && !cevar->limits.is_float) {
        if (min) *min = cevar->limits.i.min;
        if (max) *max = cevar->limits.i.max;
        return true;
    }
    return false;
}

bool Cevar_Set(cevar_t* cevar, const char* value);

// Set from float (applies limits if they exist)
bool Cevar_Set(cevar_t* cevar, float value);

// Set from int (applies limits if they exist)
bool Cevar_Set(cevar_t* cevar, int value);

// Convenience: Set by cvar pointer (looks up cevar automatically)
inline bool Cevar_SetString(cvar_t* cvar, const char* value) {
    cevar_t* cevar = Cevar_FromCvar(cvar);
    return cevar ? Cevar_Set(cevar, value) : false;
}

inline bool Cevar_SetFloat(cvar_t* cvar, float value) {
    cevar_t* cevar = Cevar_FromCvar(cvar);
    return cevar ? Cevar_Set(cevar, value) : false;
}

inline bool Cevar_SetInt(cvar_t* cvar, int value) {
    cevar_t* cevar = Cevar_FromCvar(cvar);
    return cevar ? Cevar_Set(cevar, value) : false;
}