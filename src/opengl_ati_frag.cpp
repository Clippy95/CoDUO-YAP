#include <helper.hpp>
#include <game.h>
#include "component_loader.h"
#include "cevar.h"
#include <Hooking.Patterns.h>
#include <filesystem>
#include <fstream>
#include "nlohmann/json.hpp"
#include <optional>
#include "GMath.h"
#include <buildnumber.h>
#include "framework.h"
#include "utils/common.h"
#include "GL\glew.h"
#include "utils/hooking.h"

SafetyHookInline* wglGetProcAddressD;

cevar_s* r_arb_fragment_shader_wrap_ati;
cevar_s* r_arb_fragment_shader_debug;
cevar_s* r_arb_fragment_fresnel_power;
cevar_s* r_arb_fragment_fresnel_bias;
cevar_s* r_arb_fragment_disable_fog;
cevar_s* r_arb_fragment_shader_debug_print;
cevar_s* r_fog_drawsun_workaround;


// Debug print macro for non-looping code (channel 0)
// Prints when r_ati_fragment_shader_debug_print >= 1
#define ATI_DEBUG_PRINT(format, ...) \
    do { \
        if (r_arb_fragment_shader_debug_print && r_arb_fragment_shader_debug_print->base->integer >= 1) { \
            printf("[ATI] " format, ##__VA_ARGS__); \
        } \
    } while(0)

// Debug print macro with channel support
// Channel 0: Non-looping code, prints when integer >= 1, uses Com_Printf
// Channel 1+: Looping code, prints when integer >= (channel + 1), uses printf only
#define ATI_DEBUG_PRINT_CHANNEL(channel, format, ...) \
    do { \
        if (r_arb_fragment_shader_debug_print && r_arb_fragment_shader_debug_print->base->integer >= ((channel) + 1)) { \
            if ((channel) == 0) { \
                Com_Printf("[ATI] " format, ##__VA_ARGS__); \
            } else { \
                printf("[ATI] " format, ##__VA_ARGS__); \
            } \
        } \
    } while(0)

enum ATIOpType { COLOR_OP, ALPHA_OP };

struct ATISource {
    GLuint index;
    GLuint rep;
    GLuint mod;
};

struct ATIInstruction {
    ATIOpType type;
    GLenum op;
    GLuint dst;
    GLuint dstMask;
    GLuint dstMod;
    int argCount;
    ATISource args[3];
};

struct ATISetupInst {
    bool isPassTexCoord;
    GLuint dst;
    GLuint src;
    GLenum swizzle;
};

struct ATIShader {
    std::vector<ATISetupInst> setup;
    std::vector<ATIInstruction> instructions;

    struct AnyInstruction {
        bool isSetup;
        union {
            ATISetupInst setup;
            ATIInstruction arith;
        };
    };
    std::vector<AnyInstruction> orderedInstructions;

    float constants[8][4];
    GLuint glsl_program;
    bool compiled;

    //struct FogState {
    //    GLboolean enabled;
    //    GLint mode;
    //    GLfloat density;
    //    GLfloat start;
    //    GLfloat end;
    //    GLfloat color[4];
    //} fogState;
};

struct CachedFogState {
    GLboolean enabled = GL_FALSE;
    GLint mode = GL_LINEAR;
    GLfloat density = 0.0f;
    GLfloat start = 0.0f;
    GLfloat end = 1000.0f;
    GLfloat color[4] = { 0, 0, 0, 0 };
} g_cached_fog;

PFNGLGETUNIFORMLOCATIONPROC fglGetUniformLocation = nullptr;
PFNGLUNIFORM1IPROC fglUniform1i = nullptr;
PFNGLUNIFORM1FPROC fglUniform1f = nullptr;  // NEW
PFNGLUNIFORM4FPROC fglUniform4f = nullptr;  // NEW

// Global state
std::map<GLuint, ATIShader> g_ati_shaders;
GLuint g_current_shader = 0;
bool g_building = false;
GLuint g_next_shader_id = 1;



PFNGLCREATESHADERPROC fglCreateShader = nullptr;
PFNGLSHADERSOURCEPROC fglShaderSource = nullptr;
PFNGLCOMPILESHADERPROC fglCompileShader = nullptr;
PFNGLCREATEPROGRAMPROC fglCreateProgram = nullptr;
PFNGLDELETEPROGRAMPROC fglDeleteProgram = nullptr;
PFNGLATTACHSHADERPROC fglAttachShader = nullptr;
PFNGLLINKPROGRAMPROC fglLinkProgram = nullptr;
PFNGLUSEPROGRAMPROC fglUseProgram = nullptr;
PFNGLDELETESHADERPROC fglDeleteShader = nullptr;
PFNGLGETSHADERIVPROC fglGetShaderiv = nullptr;
PFNGLGETSHADERINFOLOGPROC fglGetShaderInfoLog = nullptr;
PFNGLGETPROGRAMIVPROC fglGetProgramiv = nullptr;
PFNGLGETPROGRAMINFOLOGPROC fglGetProgramInfoLog = nullptr;


HMODULE opengl_addr;

// Helper: Convert GL enum to register string
std::string RegToString(GLuint reg) {
    if (reg >= GL_REG_0_ATI && reg <= GL_REG_5_ATI) {
        return "r" + std::to_string(reg - GL_REG_0_ATI);
    }
    if (reg >= GL_CON_0_ATI && reg <= GL_CON_7_ATI) {
        return "atiConst[" + std::to_string(reg - GL_CON_0_ATI) + "]";
    }
    if (reg == GL_PRIMARY_COLOR_ARB) {
        return "gl_Color";
    }
    if (reg == GL_SECONDARY_INTERPOLATOR_ATI) {
        return "gl_SecondaryColor";
    }
    if (reg == GL_ZERO) return "vec4(0.0)";
    if (reg == GL_ONE) return "vec4(1.0)";

    return "r0"; // fallback
}

// Helper: Translate source argument with modifiers
std::string TranslateArg(const ATISource& arg) {
    std::string base = RegToString(arg.index);

    // Apply swizzle/rep
    switch (arg.rep) {
    case GL_RED:   base += ".r"; break;
    case GL_GREEN: base += ".g"; break;
    case GL_BLUE:  base += ".b"; break;
    case GL_ALPHA: base += ".a"; break;
    case GL_NONE:  break; // use all components
    default: break;
    }

    // Apply modifiers
    std::string result = base;

    if (arg.mod & GL_COMP_BIT_ATI) {
        result = "(1.0 - " + result + ")";
    }
    if (arg.mod & GL_NEGATE_BIT_ATI) {
        result = "-(" + result + ")";
    }
    if (arg.mod & GL_BIAS_BIT_ATI) {
        result = "(" + result + " - 0.5)";
    }
    if (arg.mod & GL_2X_BIT_ATI) {
        result = "(" + result + " * 2.0)";
    }

    return result;
}

// Helper: Translate a single instruction
std::string TranslateInstruction(const ATIInstruction& inst) {
    std::stringstream ss;
    std::string dst = RegToString(inst.dst);

    // Get source operands
    std::string arg1 = TranslateArg(inst.args[0]);
    std::string arg2 = inst.argCount > 1 ? TranslateArg(inst.args[1]) : "";
    std::string arg3 = inst.argCount > 2 ? TranslateArg(inst.args[2]) : "";

    // Write mask for color ops
    std::string mask = "";
    if (inst.type == COLOR_OP && inst.dstMask != GL_NONE && inst.dstMask != 0) {
        mask = ".";
        if (inst.dstMask & GL_RED_BIT_ATI) mask += "r";
        if (inst.dstMask & GL_GREEN_BIT_ATI) mask += "g";
        if (inst.dstMask & GL_BLUE_BIT_ATI) mask += "b";
    }
    else if (inst.type == ALPHA_OP) {
        mask = ".a";
    }

    ss << "    " << dst << mask << " = ";

    // Translate operation
    std::string expr;
    switch (inst.op) {
    case GL_MOV_ATI:
        expr = arg1;
        break;
    case GL_ADD_ATI:
        expr = arg1 + " + " + arg2;
        break;
    case GL_MUL_ATI:
        expr = arg1 + " * " + arg2;
        break;
    case GL_SUB_ATI:
        expr = arg1 + " - " + arg2;
        break;
    case GL_DOT3_ATI:
        expr = "vec4(dot(" + arg1 + ".xyz, " + arg2 + ".xyz))";
        break;
    case GL_DOT4_ATI:
        expr = "vec4(dot(" + arg1 + ", " + arg2 + "))";
        break;
    case GL_MAD_ATI:
        expr = arg1 + " * " + arg2 + " + " + arg3;
        break;
    case GL_LERP_ATI:
        expr = "mix(" + arg2 + ", " + arg3 + ", " + arg1 + ")";
        break;
    case GL_CND_ATI:
        expr = "((" + arg1 + " > 0.5) ? " + arg2 + " : " + arg3 + ")";
        break;
    case GL_CND0_ATI:
        expr = "((" + arg1 + " >= 0.0) ? " + arg2 + " : " + arg3 + ")";
        break;
    case GL_DOT2_ADD_ATI:
        expr = "vec4(dot(" + arg1 + ".xy, " + arg2 + ".xy) + " + arg3 + ".z)";
        break;
    default:
        expr = "vec4(1.0, 0.0, 1.0, 1.0)"; // magenta = error
        ATI_DEBUG_PRINT_CHANNEL(0,"WARNING: Unknown opcode 0x%X\n", inst.op);
        break;
    }

    // Apply destination modifiers
    if (inst.dstMod & GL_2X_BIT_ATI) expr = "(" + expr + " * 2.0)";
    if (inst.dstMod & GL_4X_BIT_ATI) expr = "(" + expr + " * 4.0)";
    if (inst.dstMod & GL_8X_BIT_ATI) expr = "(" + expr + " * 8.0)";
    if (inst.dstMod & GL_HALF_BIT_ATI) expr = "(" + expr + " * 0.5)";
    if (inst.dstMod & GL_QUARTER_BIT_ATI) expr = "(" + expr + " * 0.25)";
    if (inst.dstMod & GL_EIGHTH_BIT_ATI) expr = "(" + expr + " * 0.125)";

    if (inst.dstMod & GL_SATURATE_BIT_ATI) {
        expr = "clamp(" + expr + ", 0.0, 1.0)";
    }

    ss << expr << ";\n";
    return ss.str();
}

// Helper: Translate setup instructions (PassTexCoord/SampleMap)
std::string TranslateSetup(const ATISetupInst& setup) {
    std::stringstream ss;
    std::string dst = RegToString(setup.dst);

    if (setup.isPassTexCoord) {
        // PassTexCoord - just pass texture coordinates
        if (setup.src >= GL_TEXTURE0_ARB && setup.src <= GL_TEXTURE7_ARB) {
            int unit = setup.src - GL_TEXTURE0_ARB;
            ss << "    " << dst << " = gl_TexCoord[" << unit << "];\n";
        }
        else if (setup.src >= GL_REG_0_ATI && setup.src <= GL_REG_5_ATI) {
            ss << "    " << dst << " = " << RegToString(setup.src) << ";\n";
        }
    }
    else {
        // SampleMap - sample a texture
        int texUnit = setup.dst - GL_REG_0_ATI;
        ss << "    " << dst << " = ";

        // Get coordinate source
        std::string coord;
        if (setup.src >= GL_TEXTURE0_ARB && setup.src <= GL_TEXTURE7_ARB) {
            int unit = setup.src - GL_TEXTURE0_ARB;
            coord = "gl_TexCoord[" + std::to_string(unit) + "]";
        }
        else if (setup.src >= GL_REG_0_ATI && setup.src <= GL_REG_5_ATI) {
            // Just use the register as-is - it's already been set
            coord = RegToString(setup.src);
        }
        else {
            coord = RegToString(setup.src);
        }

        // Apply swizzle and choose texture function
        if (texUnit == 0) {
            // tex0 is 2D texture - needs .xy coordinates
            ss << "texture2D(tex0, " << coord << ".xy);\n";
        }
        else {
            // Other textures are cube maps - need .xyz
            if (setup.swizzle == GL_SWIZZLE_STR_ATI) {
                ss << "textureCube(tex" << texUnit << ", " << coord << ".xyz);\n";
            }
            else if (setup.swizzle == GL_SWIZZLE_STQ_ATI) {
                ss << "textureCube(tex" << texUnit << ", " << coord << ".xyw);\n";
            }
            else {
                ss << "textureCube(tex" << texUnit << ", " << coord << ".xyz);\n";
            }
        }
    }

    return ss.str();
}

// Main translator
std::string TranslateToGLSL(const ATIShader& shader) {
    std::stringstream ss;

    ss << "uniform sampler2D tex0;\n";
    ss << "uniform samplerCube tex1, tex2, tex3, tex4, tex5;\n";
    ss << "uniform vec4 atiConst[8];\n";
    ss << "\n";
    ss << "// Custom fog uniforms\n";
    ss << "uniform int fogEnabled;\n";
    ss << "uniform int fogMode;\n";
    ss << "uniform float fogDensity;\n";
    ss << "uniform float fogStart;\n";
    ss << "uniform float fogEnd;\n";
    ss << "uniform vec4 fogColor;\n";
    ss << "uniform int debugMode;\n";
    ss << "uniform float fresnelPower;\n";
    ss << "uniform float fresnelBias;\n";
    ss << "uniform int disableFog;\n";

    ss << "\n";

    ss << "void main() {\n";

    ss << "    vec4 r0 = vec4(0.0);\n";
    ss << "    vec4 r1 = vec4(0.0);\n";
    ss << "    vec4 r2 = vec4(0.0);\n";
    ss << "    vec4 r3 = vec4(0.0);\n";
    ss << "    vec4 r4 = vec4(0.0);\n";
    ss << "    vec4 r5 = vec4(0.0);\n\n";

    // Process in recorded order
    for (const auto& any : shader.orderedInstructions) {
        if (any.isSetup) {
            ss << TranslateSetup(any.setup);
        }
        else {
            ss << TranslateInstruction(any.arith);
        }
    }

    ss << "\n    // Use simplified Fresnel (avoid discontinuities)\n";
    ss << "    vec3 normalVec = normalize((r0 * 2.0 - 1.0).xyz);\n";
    ss << "    \n";
    ss << "    // Use Z component of normal as approximation for view angle\n";
    ss << "    float fresnel = fresnelBias + (1.0 - fresnelBias) * pow(clamp(1.0 - abs(normalVec.z), 0.0, 1.0), fresnelPower);\n";
    ss << "    fresnel = clamp(fresnel, 0.0, 1.0);\n";
    ss << "    \n";
    ss << "\n    // Debug visualization and Fresnel blending\n";
    ss << "    if (debugMode == 0) {\n";
    ss << "        // Correct blend method\n";
    ss << "        r0 = mix(r3, r2, fresnel);\n";
    ss << "        r0 = r0 * gl_Color;\n";
    ss << "        r0.a = gl_Color.a;\n";
    ss << "    } else if (debugMode == -1) {\n";
    ss << "        // (incorrect) blend method\n";
    ss << "        r0 = mix(r2, r3, fresnel);\n";
    ss << "        r0 = r0 * gl_Color;\n";
    ss << "        r0.a = gl_Color.a;\n";
    ss << "    } else if (debugMode == 1) {\n";
    ss << "        gl_FragColor = vec4(r3.a, r3.a, r3.a, 1.0);  // Fresnel alpha\n";
    ss << "        return;\n";
    ss << "    } else if (debugMode == 2) {\n";
    ss << "        gl_FragColor = vec4(r3.rgb, 1.0);  // Specular RGB\n";
    ss << "        return;\n";
    ss << "    } else if (debugMode == 3) {\n";
    ss << "        gl_FragColor = vec4(r2.rgb, 1.0);  // Diffuse RGB\n";
    ss << "        return;\n";
    ss << "    } else if (debugMode == 4) {\n";
    ss << "        gl_FragColor = vec4(r3.xyz * 0.5 + 0.5, 1.0);  // Reflection vector\n";
    ss << "        return;\n";
    ss << "    } else if (debugMode == 5) {\n";
    ss << "        gl_FragColor = vec4(r2.xyz * 0.5 + 0.5, 1.0);  // Normal vector\n";
    ss << "        return;\n";
    ss << "    }\n";

    // Apply fog
    ss << "\n    // Apply fog\n";
    ss << "    if (fogEnabled != 0 && disableFog != 1) {\n";
    ss << "        float fogFactor = 1.0;\n";
    ss << "        const float LOG2 = 1.442695;\n";
    ss << "        \n";
    ss << "        if (fogMode == 0x0800) {\n";
    ss << "            fogFactor = exp2(-fogDensity * gl_FogFragCoord * LOG2);\n";
    ss << "        } else if (fogMode == 0x0801) {\n";
    ss << "            fogFactor = exp2(-fogDensity * fogDensity * gl_FogFragCoord * gl_FogFragCoord * LOG2);\n";
    ss << "        } else {\n";
    ss << "            fogFactor = (fogEnd - gl_FogFragCoord) / (fogEnd - fogStart);\n";
    ss << "        }\n";
    ss << "        \n";
    ss << "        fogFactor = clamp(fogFactor, 0.0, 1.0);\n";
    ss << "        r0.rgb = mix(fogColor.rgb, r0.rgb, fogFactor);\n";
    ss << "    }\n";
    ss << "\n    gl_FragColor = r0;\n";
    ss << "}\n";

    return ss.str();
}

// Compile GLSL shader
GLuint CompileGLSL(const std::string& fragSrc) {
    if (!fglCreateShader) {
        ATI_DEBUG_PRINT_CHANNEL(0,"[ERROR] GLSL functions not loaded!\n");
        return 0;
    }

    // Simple vertex shader
    const char* vsSrc =
        "void main() {\n"
        "    gl_Position = ftransform();\n"
        "    gl_TexCoord[0] = gl_MultiTexCoord0;\n"
        "    gl_TexCoord[1] = gl_MultiTexCoord1;\n"
        "    gl_TexCoord[2] = gl_MultiTexCoord2;\n"
        "    gl_TexCoord[3] = gl_MultiTexCoord3;\n"
        "    gl_FrontColor = gl_Color;\n"
        "    gl_BackColor = gl_SecondaryColor;\n"
        "    gl_FogFragCoord = gl_Position.z;\n"
        "}\n";

    GLuint vs = fglCreateShader(GL_VERTEX_SHADER);
    fglShaderSource(vs, 1, &vsSrc, NULL);
    fglCompileShader(vs);

    GLuint fs = fglCreateShader(GL_FRAGMENT_SHADER);
    const char* fsSrc = fragSrc.c_str();
    fglShaderSource(fs, 1, &fsSrc, NULL);
    fglCompileShader(fs);

    // Check compile status
    GLint success;
    fglGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[1024];
        fglGetShaderInfoLog(fs, 1024, NULL, log);
        ATI_DEBUG_PRINT_CHANNEL(0,"[ERROR] Fragment shader compile error:\n%s\n", log);
        ATI_DEBUG_PRINT_CHANNEL(0,"Source:\n%s\n", fragSrc.c_str());
    }

    GLuint program = fglCreateProgram();
    fglAttachShader(program, vs);
    fglAttachShader(program, fs);
    fglLinkProgram(program);

    // Check link status
    fglGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[1024];
        fglGetProgramInfoLog(program, 1024, NULL, log);
        // maybe use com_error here?
        ATI_DEBUG_PRINT_CHANNEL(0,"[ERROR] Program link error:\n%s\n", log);
    }

    fglDeleteShader(vs);
    fglDeleteShader(fs);

    ATI_DEBUG_PRINT_CHANNEL(0,"[ATI->GLSL] Compiled shader program: %d\n", program);
    return program;
}

// ATI Fragment Shader function hooks

GLuint WINAPI glGenFragmentShadersATI_hook(GLuint range) {
    ATI_DEBUG_PRINT_CHANNEL(1,"glGenFragmentShadersATI(%d)\n", range);
    GLuint first = g_next_shader_id;
    for (GLuint i = 0; i < range; i++) {
        g_ati_shaders[first + i] = ATIShader();
        g_ati_shaders[first + i].compiled = false;
    }
    g_next_shader_id += range;
    return first;
}

typedef void (WINAPI* PFNGLGETINTEGERVPROC)(GLenum pname, GLint* params);
typedef void (WINAPI* PFNGLGETFLOATVPROC)(GLenum pname, GLfloat* params);
typedef GLboolean(WINAPI* PFNGLISENABLEDPROC)(GLenum cap);
typedef const char*(WINAPI* PFNGLGETSTRINGPROC)(GLenum cap);

PFNGLGETSTRINGPROC fGlGetString;

typedef void (WINAPI* PFNGLENABLEPROC)(GLenum cap);

PFNGLGETINTEGERVPROC fglGetIntegerv = nullptr;
PFNGLGETFLOATVPROC fglGetFloatv = nullptr;
PFNGLISENABLEDPROC fglIsEnabled = nullptr;
PFNGLENABLEPROC fglEnable;
PFNGLENABLEPROC fglDisable;


void WINAPI glBindFragmentShaderATI_hook(GLuint id) {
    ATI_DEBUG_PRINT_CHANNEL(1,"glBindFragmentShaderATI(% d)\n", id);
    g_current_shader = id;

    if (id == 0) {
        if (fglUseProgram) {
            fglUseProgram(0);
        }
        ATI_DEBUG_PRINT_CHANNEL(1, "Disabled shaders (fixed function)\n");
    }
    else if (g_ati_shaders.count(id) > 0 && g_ati_shaders[id].compiled && g_ati_shaders[id].glsl_program != 0) {
        GLuint program = g_ati_shaders[id].glsl_program;

        if (fglUseProgram) {
            fglUseProgram(program);
        }

        if (fglGetUniformLocation && fglUniform1i && fglUniform1f && fglUniform4f) {
            GLint loc;

            // Capture current fog state
            GLboolean fogEnabled = fglIsEnabled(GL_FOG);
            GLint fogMode = 0;
            GLfloat fogDensity = 0, fogStart = 0, fogEnd = 0;
            GLfloat* fogColor = (GLfloat*)exe(0x47BDF80,0x4899F20);

            fglGetIntegerv(GL_FOG_MODE, &fogMode);
            fglGetFloatv(GL_FOG_DENSITY, &fogDensity);
            fglGetFloatv(GL_FOG_START, &fogStart);
            fglGetFloatv(GL_FOG_END, &fogEnd);
            //fglGetFloatv(GL_FOG_COLOR, fogColor);

            ATI_DEBUG_PRINT_CHANNEL(1, "[FOG DEBUG] Enabled=%d, Mode=0x%X, Start=%.2f, End=%.2f, Density=%.4f, Color=(%.2f,%.2f,%.2f,%.2f)\n",
                fogEnabled, fogMode, fogStart, fogEnd, fogDensity,
                fogColor[0], fogColor[1], fogColor[2], fogColor[3]);

            // Update fog uniforms
            loc = fglGetUniformLocation(program, "fogEnabled");
            if (loc >= 0) fglUniform1i(loc, fogEnabled ? 1 : 0);

            loc = fglGetUniformLocation(program, "fogMode");
            if (loc >= 0) fglUniform1i(loc, fogMode);

            loc = fglGetUniformLocation(program, "fogDensity");
            if (loc >= 0) fglUniform1f(loc, fogDensity);

            loc = fglGetUniformLocation(program, "fogStart");
            if (loc >= 0) fglUniform1f(loc, fogStart);

            loc = fglGetUniformLocation(program, "fogEnd");
            if (loc >= 0) fglUniform1f(loc, fogEnd);

            loc = fglGetUniformLocation(program, "fogColor");
            if (loc >= 0) fglUniform4f(loc, fogColor[0], fogColor[1], fogColor[2], fogColor[3]);

            loc = fglGetUniformLocation(program, "debugMode");
            if (loc >= 0) fglUniform1i(loc, (GLint)Cevar_Int(r_arb_fragment_shader_debug));

            loc = fglGetUniformLocation(program, "debugMode");
            if (loc >= 0) fglUniform1i(loc, Cevar_Int(r_arb_fragment_shader_debug));

            loc = fglGetUniformLocation(program, "fresnelPower");
            if (loc >= 0) fglUniform1f(loc, Cevar_Value(r_arb_fragment_fresnel_power));

            loc = fglGetUniformLocation(program, "fresnelBias");
            if (loc >= 0) fglUniform1f(loc, Cevar_Value(r_arb_fragment_fresnel_bias));

            loc = fglGetUniformLocation(program, "disableFog");
            if (loc >= 0) fglUniform1i(loc, Cevar_Int(r_arb_fragment_disable_fog));

        }

        ATI_DEBUG_PRINT_CHANNEL(1, " Activated shader program %d\n", program);
    }
}

void WINAPI glDeleteFragmentShaderATI_hook(GLuint id) {
    ATI_DEBUG_PRINT_CHANNEL(1, "glDeleteFragmentShaderATI(%d)\n", id);
    if (g_ati_shaders.count(id)) {
        if (g_ati_shaders[id].glsl_program != 0) {
            if (fglDeleteProgram) {
                fglDeleteProgram(g_ati_shaders[id].glsl_program);
                ATI_DEBUG_PRINT_CHANNEL(0, "Deleted GLSL program %d for ATI shader %d\n",
                    g_ati_shaders[id].glsl_program, id);
            }
        }

        // If deleting the currently bound shader, unbind it
        if (g_current_shader == id) {
            g_current_shader = 0;
            if (fglUseProgram) {
                fglUseProgram(0);
            }
        }

        g_ati_shaders.erase(id);
    }
}

void DeleteAllATIFragmentShaders() {
    ATI_DEBUG_PRINT_CHANNEL(0, "Deleting all ATI fragment shaders (%d total)\n",
        (int)g_ati_shaders.size());

    for (auto& pair : g_ati_shaders) {
        if (pair.second.glsl_program != 0) {
            if (fglDeleteProgram) {
                fglDeleteProgram(pair.second.glsl_program);
                ATI_DEBUG_PRINT_CHANNEL(1, "Deleted GLSL program %d for ATI shader %d\n",
                    pair.second.glsl_program, pair.first);
            }
        }
    }

    g_ati_shaders.clear();

    // Possible 
    g_next_shader_id = 1;
    g_current_shader = 0;

    ATI_DEBUG_PRINT_CHANNEL(0, "All ATI fragment shaders deleted\n");
}

void WINAPI glBeginFragmentShaderATI_hook() {
    ATI_DEBUG_PRINT_CHANNEL(1, "glBeginFragmentShaderATI()\n");
    g_building = true;
    g_ati_shaders[g_current_shader].setup.clear();
    g_ati_shaders[g_current_shader].instructions.clear();
    g_ati_shaders[g_current_shader].orderedInstructions.clear();  // NEW
    g_ati_shaders[g_current_shader].compiled = false;
}

uintptr_t R_DeleteFragmentShaders_ptr;

void R_DeleteFragmentShaders_hook() {
    cdecl_call<void>(R_DeleteFragmentShaders_ptr);
    DeleteAllATIFragmentShaders();
}

void WINAPI glEndFragmentShaderATI_hook() {
    ATI_DEBUG_PRINT_CHANNEL(1, "[ATI] glEndFragmentShaderATI()\n");
    g_building = false;

    ATIShader& shader = g_ati_shaders[g_current_shader];



    ATI_DEBUG_PRINT_CHANNEL(0, "Shader %d: %d ordered instructions\n",
        g_current_shader,
        (int)shader.orderedInstructions.size());

    // Translate to GLSL
    std::string glslSource = TranslateToGLSL(shader);
    auto logfile = Cvar_Find("logfile");

    if (logfile && logfile->integer) {
        ATI_DEBUG_PRINT_CHANNEL(0, "[ATI->GLSL] Generated shader : \n % s\n", glslSource.c_str());
    }
    // Compile GLSL
    shader.glsl_program = CompileGLSL(glslSource);
    shader.compiled = true;

    // Activate it
    if (shader.glsl_program != 0) {
        fglUseProgram(shader.glsl_program);


        // Get uniform locations and set them
        auto glGetUniformLocation = (GLint(WINAPI*)(GLuint, const GLchar*))
            wglGetProcAddressD->unsafe_stdcall<void*>("glGetUniformLocation");
        auto glUniform1i = (void(WINAPI*)(GLint, GLint))
            wglGetProcAddressD->unsafe_stdcall<void*>("glUniform1i");

        if (glGetUniformLocation && glUniform1i) {
            GLint loc;
            loc = glGetUniformLocation(shader.glsl_program, "tex0");
            if (loc >= 0) glUniform1i(loc, 0);

            loc = glGetUniformLocation(shader.glsl_program, "tex1");
            if (loc >= 0) glUniform1i(loc, 1);

            loc = glGetUniformLocation(shader.glsl_program, "tex2");
            if (loc >= 0) glUniform1i(loc, 2);

            loc = glGetUniformLocation(shader.glsl_program, "tex3");
            if (loc >= 0) glUniform1i(loc, 3);

            loc = glGetUniformLocation(shader.glsl_program, "tex4");
            if (loc >= 0) glUniform1i(loc, 4);

            loc = glGetUniformLocation(shader.glsl_program, "tex5");
            if (loc >= 0) glUniform1i(loc, 5);

            ATI_DEBUG_PRINT_CHANNEL(0, "Bound texture uniforms\n");
        }

        ATI_DEBUG_PRINT_CHANNEL(0, "Shader %d compiled and activated (program %d)\n",
            g_current_shader, shader.glsl_program);
    }
}

void WINAPI glPassTexCoordATI_hook(GLuint dst, GLuint coord, GLenum swizzle) {
    ATI_DEBUG_PRINT_CHANNEL(0, " glPassTexCoordATI(dst=%d, coord=%d, swizzle=0x%X)\n", dst, coord, swizzle);
    ATISetupInst inst;
    inst.isPassTexCoord = true;
    inst.dst = dst;
    inst.src = coord;
    inst.swizzle = swizzle;
    g_ati_shaders[g_current_shader].setup.push_back(inst);

    // Add to ordered list
    ATIShader::AnyInstruction any;
    any.isSetup = true;
    any.setup = inst;
    g_ati_shaders[g_current_shader].orderedInstructions.push_back(any);
}

void WINAPI glSampleMapATI_hook(GLuint dst, GLuint interp, GLenum swizzle) {
    ATI_DEBUG_PRINT_CHANNEL(0, "glSampleMapATI(dst=%d, interp=%d, swizzle=0x%X)\n", dst, interp, swizzle);
    ATISetupInst inst;
    inst.isPassTexCoord = false;
    inst.dst = dst;
    inst.src = interp;
    inst.swizzle = swizzle;
    g_ati_shaders[g_current_shader].setup.push_back(inst);

    // Add to ordered list
    ATIShader::AnyInstruction any;
    any.isSetup = true;
    any.setup = inst;
    g_ati_shaders[g_current_shader].orderedInstructions.push_back(any);
}

void WINAPI glColorFragmentOp1ATI_hook(GLenum op, GLuint dst, GLuint dstMask,
    GLuint dstMod, GLuint arg1, GLuint arg1Rep,
    GLuint arg1Mod) {
    ATI_DEBUG_PRINT_CHANNEL(0, "[ATI] glColorFragmentOp1ATI(op=0x%X, dst=%d)\n", op, dst);
    ATIInstruction inst;
    inst.type = COLOR_OP;
    inst.op = op;
    inst.dst = dst;
    inst.dstMask = dstMask;
    inst.dstMod = dstMod;
    inst.argCount = 1;
    inst.args[0] = { arg1, arg1Rep, arg1Mod };
    g_ati_shaders[g_current_shader].instructions.push_back(inst);

    // Add to ordered list
    ATIShader::AnyInstruction any;
    any.isSetup = false;
    any.arith = inst;
    g_ati_shaders[g_current_shader].orderedInstructions.push_back(any);
}

void WINAPI glColorFragmentOp2ATI_hook(GLenum op, GLuint dst, GLuint dstMask,
    GLuint dstMod, GLuint arg1, GLuint arg1Rep,
    GLuint arg1Mod, GLuint arg2, GLuint arg2Rep,
    GLuint arg2Mod) {
    ATI_DEBUG_PRINT_CHANNEL(0, "glColorFragmentOp2ATI(op=0x%X, dst=%d)\n", op, dst);
    ATIInstruction inst;
    inst.type = COLOR_OP;
    inst.op = op;
    inst.dst = dst;
    inst.dstMask = dstMask;
    inst.dstMod = dstMod;
    inst.argCount = 2;
    inst.args[0] = { arg1, arg1Rep, arg1Mod };
    inst.args[1] = { arg2, arg2Rep, arg2Mod };
    g_ati_shaders[g_current_shader].instructions.push_back(inst);

    // Add to ordered list
    ATIShader::AnyInstruction any;
    any.isSetup = false;
    any.arith = inst;
    g_ati_shaders[g_current_shader].orderedInstructions.push_back(any);
}

void WINAPI glColorFragmentOp3ATI_hook(GLenum op, GLuint dst, GLuint dstMask,
    GLuint dstMod, GLuint arg1, GLuint arg1Rep,
    GLuint arg1Mod, GLuint arg2, GLuint arg2Rep,
    GLuint arg2Mod, GLuint arg3, GLuint arg3Rep,
    GLuint arg3Mod) {
    ATI_DEBUG_PRINT_CHANNEL(0, "glColorFragmentOp3ATI(op=0x%X, dst=%d)\n", op, dst);
    ATIInstruction inst;
    inst.type = COLOR_OP;
    inst.op = op;
    inst.dst = dst;
    inst.dstMask = dstMask;
    inst.dstMod = dstMod;
    inst.argCount = 3;
    inst.args[0] = { arg1, arg1Rep, arg1Mod };
    inst.args[1] = { arg2, arg2Rep, arg2Mod };
    inst.args[2] = { arg3, arg3Rep, arg3Mod };
    g_ati_shaders[g_current_shader].instructions.push_back(inst);

    // Add to ordered list
    ATIShader::AnyInstruction any;
    any.isSetup = false;
    any.arith = inst;
    g_ati_shaders[g_current_shader].orderedInstructions.push_back(any);
}

void WINAPI glAlphaFragmentOp1ATI_hook(GLenum op, GLuint dst, GLuint dstMod,
    GLuint arg1, GLuint arg1Rep, GLuint arg1Mod) {
    ATI_DEBUG_PRINT_CHANNEL(0, "glAlphaFragmentOp1ATI(op=0x%X, dst=%d)\n", op, dst);
    ATIInstruction inst;
    inst.type = ALPHA_OP;
    inst.op = op;
    inst.dst = dst;
    inst.dstMask = 0;
    inst.dstMod = dstMod;
    inst.argCount = 1;
    inst.args[0] = { arg1, arg1Rep, arg1Mod };
    g_ati_shaders[g_current_shader].instructions.push_back(inst);

    // Add to ordered list
    ATIShader::AnyInstruction any;
    any.isSetup = false;
    any.arith = inst;
    g_ati_shaders[g_current_shader].orderedInstructions.push_back(any);
}

void WINAPI glAlphaFragmentOp2ATI_hook(GLenum op, GLuint dst, GLuint dstMod,
    GLuint arg1, GLuint arg1Rep, GLuint arg1Mod,
    GLuint arg2, GLuint arg2Rep, GLuint arg2Mod) {
    ATI_DEBUG_PRINT_CHANNEL(0, "glAlphaFragmentOp2ATI(op=0x%X, dst=%d)\n", op, dst);
    ATIInstruction inst;
    inst.type = ALPHA_OP;
    inst.op = op;
    inst.dst = dst;
    inst.dstMask = 0;
    inst.dstMod = dstMod;
    inst.argCount = 2;
    inst.args[0] = { arg1, arg1Rep, arg1Mod };
    inst.args[1] = { arg2, arg2Rep, arg2Mod };
    g_ati_shaders[g_current_shader].instructions.push_back(inst);

    // Add to ordered list
    ATIShader::AnyInstruction any;
    any.isSetup = false;
    any.arith = inst;
    g_ati_shaders[g_current_shader].orderedInstructions.push_back(any);
}

void WINAPI glAlphaFragmentOp3ATI_hook(GLenum op, GLuint dst, GLuint dstMod,
    GLuint arg1, GLuint arg1Rep, GLuint arg1Mod,
    GLuint arg2, GLuint arg2Rep, GLuint arg2Mod,
    GLuint arg3, GLuint arg3Rep, GLuint arg3Mod) {
    ATI_DEBUG_PRINT_CHANNEL(0, "glAlphaFragmentOp3ATI(op=0x%X, dst=%d)\n", op, dst);
    ATIInstruction inst;
    inst.type = ALPHA_OP;
    inst.op = op;
    inst.dst = dst;
    inst.dstMask = 0;
    inst.dstMod = dstMod;
    inst.argCount = 3;
    inst.args[0] = { arg1, arg1Rep, arg1Mod };
    inst.args[1] = { arg2, arg2Rep, arg2Mod };
    inst.args[2] = { arg3, arg3Rep, arg3Mod };
    g_ati_shaders[g_current_shader].instructions.push_back(inst);

    // Add to ordered list
    ATIShader::AnyInstruction any;
    any.isSetup = false;
    any.arith = inst;
    g_ati_shaders[g_current_shader].orderedInstructions.push_back(any);
}

void WINAPI glSetFragmentShaderConstantATI_hook(GLuint dst, const GLfloat* value) {
    ATI_DEBUG_PRINT_CHANNEL(1, "glSetFragmentShaderConstantATI(dst=%d, value=[%.2f, %.2f, %.2f, %.2f])\n",
        dst, value[0], value[1], value[2], value[3]);
    int idx = dst - GL_CON_0_ATI;
    if (idx >= 0 && idx < 8) {
        memcpy(g_ati_shaders[g_current_shader].constants[idx], value, sizeof(float) * 4);
    }
}

bool ATI_FRAGMENT_SHADER_VALID = false;
void* __stdcall wglGetProcAddress_hook(const char* name) {

    if (ATI_FRAGMENT_SHADER_VALID) {
        if (strcmp(name, "glGenFragmentShadersATI") == 0) {
            printf("[ATI] Intercepting glGenFragmentShadersATI\n");
            return (void*)glGenFragmentShadersATI_hook;
        }
        if (strcmp(name, "glBindFragmentShaderATI") == 0) {
            printf("[ATI] Intercepting glBindFragmentShaderATI\n");
            return (void*)glBindFragmentShaderATI_hook;
        }
        if (strcmp(name, "glDeleteFragmentShaderATI") == 0) {
            return (void*)glDeleteFragmentShaderATI_hook;
        }
        if (strcmp(name, "glBeginFragmentShaderATI") == 0) {
            return (void*)glBeginFragmentShaderATI_hook;
        }
        if (strcmp(name, "glEndFragmentShaderATI") == 0) {
            return (void*)glEndFragmentShaderATI_hook;
        }
        if (strcmp(name, "glPassTexCoordATI") == 0) {
            return (void*)glPassTexCoordATI_hook;
        }
        if (strcmp(name, "glSampleMapATI") == 0) {
            return (void*)glSampleMapATI_hook;
        }
        if (strcmp(name, "glColorFragmentOp1ATI") == 0) {
            return (void*)glColorFragmentOp1ATI_hook;
        }
        if (strcmp(name, "glColorFragmentOp2ATI") == 0) {
            return (void*)glColorFragmentOp2ATI_hook;
        }
        if (strcmp(name, "glColorFragmentOp3ATI") == 0) {
            return (void*)glColorFragmentOp3ATI_hook;
        }
        if (strcmp(name, "glAlphaFragmentOp1ATI") == 0) {
            return (void*)glAlphaFragmentOp1ATI_hook;
        }
        if (strcmp(name, "glAlphaFragmentOp2ATI") == 0) {
            return (void*)glAlphaFragmentOp2ATI_hook;
        }
        if (strcmp(name, "glAlphaFragmentOp3ATI") == 0) {
            return (void*)glAlphaFragmentOp3ATI_hook;
        }
        if (strcmp(name, "glSetFragmentShaderConstantATI") == 0) {
            return (void*)glSetFragmentShaderConstantATI_hook;
        }
    }
    return wglGetProcAddressD->stdcall<void*>(name);
}

//void GLDISABLE(GLenum cap) {
//    stdcall_call<void>(*(int*)0x047BCED8, cap);
//}
//
//void R_FogOff() {
//    stdcall_call<void>(0x004CD410);
//}


GLuint g_sun_shader_program = 0;
bool g_sun_shader_initialized = false;

GLuint CreateSunShader() {
    if (!fglCreateShader || !fglUseProgram) {
        ATI_DEBUG_PRINT_CHANNEL(0, "[ERROR] GLSL functions not available for sun shader!\n");
        return 0;
    }

    const char* vsSrc =
        "void main() {\n"
        "    gl_Position = ftransform();\n"
        "    gl_TexCoord[0] = gl_MultiTexCoord0;\n"
        "    gl_FrontColor = gl_Color;\n"
        "}\n";

    const char* fsSrc =
        "uniform sampler2D texture0;\n"
        "void main() {\n"
        "    gl_FragColor = texture2D(texture0, gl_TexCoord[0].xy) * gl_Color;\n"
        "}\n";

    GLuint vs = fglCreateShader(GL_VERTEX_SHADER);
    fglShaderSource(vs, 1, &vsSrc, NULL);
    fglCompileShader(vs);


    GLint success;
    fglGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[1024];
        fglGetShaderInfoLog(vs, 1024, NULL, log);
        ATI_DEBUG_PRINT_CHANNEL(0, "[ERROR] Sun vertex shader compile error:\n%s\n", log);
    }

    GLuint fs = fglCreateShader(GL_FRAGMENT_SHADER);
    fglShaderSource(fs, 1, &fsSrc, NULL);
    fglCompileShader(fs);

    fglGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[1024];
        fglGetShaderInfoLog(fs, 1024, NULL, log);
        ATI_DEBUG_PRINT_CHANNEL(0, "[ERROR] Sun fragment shader compile error:\n%s\n", log);
    }

    GLuint program = fglCreateProgram();
    fglAttachShader(program, vs);
    fglAttachShader(program, fs);
    fglLinkProgram(program);


    fglGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[1024];
        fglGetProgramInfoLog(program, 1024, NULL, log);
        ATI_DEBUG_PRINT_CHANNEL(0, "[ERROR] Sun shader program link error:\n%s\n", log);
    }

    fglDeleteShader(vs);
    fglDeleteShader(fs);

    ATI_DEBUG_PRINT_CHANNEL(0, "[SUN SHADER] Compiled shader program: %d\n", program);
    return program;
}

uintptr_t RB_DrawSunSprite_addr;
void hooked_RB_DrawSunSprite() {

    if (!g_sun_shader_initialized) {
        g_sun_shader_program = CreateSunShader();
        g_sun_shader_initialized = true;

        if (g_sun_shader_program == 0) {
            ATI_DEBUG_PRINT_CHANNEL(0, "[ERROR] Failed to create sun shader, using original rendering\n");
             cdecl_call<void*>(RB_DrawSunSprite_addr);
             return;
        }
    }


    if (g_sun_shader_program != 0 && fglUseProgram && r_fog_drawsun_workaround && r_fog_drawsun_workaround->base->integer) {
        fglUseProgram(g_sun_shader_program);


        if (fglGetUniformLocation && fglUniform1i) {
            GLint texLoc = fglGetUniformLocation(g_sun_shader_program, "texture0");
            if (texLoc >= 0) {
                fglUniform1i(texLoc, 0);
            }
        }

        ATI_DEBUG_PRINT_CHANNEL(1, "[SUN SHADER] Activated shader for sun sprite\n");
    }


    cdecl_call<void>(RB_DrawSunSprite_addr);


    if (fglUseProgram && r_fog_drawsun_workaround && r_fog_drawsun_workaround->base->integer) {
        fglUseProgram(0);
        ATI_DEBUG_PRINT_CHANNEL(1, "[SUN SHADER] Deactivated shader, back to fixed-function\n");
    }

}

namespace opengl_ati_frag {

    bool ExtensionExists(const char* glextension) {
        auto cextensions = *(const char**)exe(0x047BE08C, 0x489A02C);
        if (cextensions) {
            return strstr(cextensions, glextension) != nullptr;
        }
        return false;
    }

    // Might not be 100% accurate but this should be 22.7.1+ and should detect if the OpenGL fixes have been implemented by AMD
    bool AMD_PostJuneDriver() {
        return (ExtensionExists("GL_ATI_meminfo") || ExtensionExists("GL_AMD_debug_output")) && ExtensionExists("GL_ARB_fragment_shader") && !ExtensionExists("GL_ATI_fragment_shader");
    }


    SafetyHookMid GL_ATI_fragment_shader_force_jump;
    uintptr_t saved_addr = 0;
    class component final : public component_interface
    {
    public:

        void post_unpack() override {
            auto pattern = hook::pattern("E8 ? ? ? ? ? ? ? 52 E8 ? ? ? ? 83 C4 ? 59");
            if(!pattern.empty())
            Memory::VP::InterceptCall(pattern.get_first(), RB_DrawSunSprite_addr, hooked_RB_DrawSunSprite);

            r_arb_fragment_shader_wrap_ati = Cevar_Get("r_arb_fragment_shader_wrap_ati", 1, CVAR_ARCHIVE | CVAR_LATCH, 0, 2);
            r_arb_fragment_shader_debug = Cevar_Get("r_arb_fragment_shader_debug", 0, CVAR_ARCHIVE, -1, 6);
            r_arb_fragment_shader_debug_print = Cevar_Get("r_arb_fragment_shader_debug_print",1, CVAR_ARCHIVE,0,3);
            r_arb_fragment_fresnel_power = Cevar_Get("r_arb_fragment_fresnel_power",2.0f, CVAR_ARCHIVE);  // current Default: 2.0
            r_arb_fragment_fresnel_bias = Cevar_Get("r_arb_fragment_fresnel_bias", 0.f, CVAR_ARCHIVE);      // current Default: 0.0
            r_arb_fragment_disable_fog = Cevar_Get("r_arb_fragment_disable_fog", 0, 0, 0,1);  // default 0 (fog enabled)
            //pattern = hook::pattern("57 33 FF 3B C7 0F 84 ? ? ? ? 8B 15");
            //if(!pattern.empty())
            //    if (!pattern.empty())
            //        static auto RE_beinframe = safetyhook::create_mid(pattern.get_first(-5), [](SafetyHookContext& ctx) {
            //        // Capture fog state once per frame
            //        g_cached_fog.enabled = fglIsEnabled(GL_FOG);
            //        fglGetIntegerv(GL_FOG_MODE, &g_cached_fog.mode);
            //        fglGetFloatv(GL_FOG_DENSITY, &g_cached_fog.density);
            //        fglGetFloatv(GL_FOG_START, &g_cached_fog.start);
            //        fglGetFloatv(GL_FOG_END, &g_cached_fog.end);
            //        fglGetFloatv(GL_FOG_COLOR, g_cached_fog.color);
            //            });

             pattern = hook::pattern("0F 84 ? ? ? ? 8B 0D ? ? ? ? 39 71 ? 0F 84 ? ? ? ? 68 ? ? ? ? FF 15 ? ? ? ? 68 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? FF 15 ? ? ? ? 68 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? FF 15 ? ? ? ? 68 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? FF 15 ? ? ? ? 68 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? FF 15 ? ? ? ? 68 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? FF 15 ? ? ? ? 68 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? FF 15 ? ? ? ? 68 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? FF 15 ? ? ? ? 68 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? FF 15 ? ? ? ? 68 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? FF 15 ? ? ? ? 68 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? FF 15 ? ? ? ? 68");
            if (pattern.empty())
                return;
            GL_ATI_fragment_shader_force_jump = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& ctx) {

                if (!fglCreateShader || !fglShaderSource || !fglCompileShader) {
                    printf("[ERROR] Failed to load GLSL functions! OpenGL 2.0 not available?\n");
                    return;
                }
                auto r_nv_register_combiners = Cvar_Find("r_nv_register_combiners");
                auto r_nv_texture_shader = Cvar_Find("r_nv_texture_shader");
                bool GL_NV_fragment_combo = ExtensionExists("GL_NV_texture_shader") || ExtensionExists("GL_NV_register_combiners");
                bool GL_ATI_FRAGMENT_SHADER_exists = ExtensionExists("GL_ATI_fragment_shader");


                bool skip_due_to_nvidia = GL_NV_fragment_combo &&
                    ((r_nv_register_combiners && r_nv_register_combiners->integer > 0) || (r_nv_texture_shader && r_nv_texture_shader->integer > 0));
                bool skip_due_to_ati = GL_ATI_FRAGMENT_SHADER_exists &&
                    (Cevar_Int(r_arb_fragment_shader_wrap_ati) <= 2);

                if (!skip_due_to_nvidia && !skip_due_to_ati) {
                    if ((fglCreateShader && fglUseProgram) && Cevar_Int(r_arb_fragment_shader_wrap_ati)) {
                        ctx.eip = (GL_ATI_fragment_shader_force_jump.target_address() + 6);
                        ATI_FRAGMENT_SHADER_VALID = true;
                        Com_Printf("[" MOD_NAME "] " "Force Enabling GL_ATI_fragment_shader by translating it to GL_ARB_FRAGMENT_SHADER\n");
                    }
                }
                else {
                    ATI_FRAGMENT_SHADER_VALID = false;
                    Com_Printf("[" MOD_NAME "] " "Skipping wrapped GL_ARB_FRAGMENT_SHADER because GL_NV_texture_shader or GL_NV_register_combiners or GL_ATI_fragment_shader already exists, disable r_nv_register_combiners or r_nv_texture_shader\nas for GL_ATI_FRAGMENT_SHADER use r_arb_fragment_shader_wrap_ati 2 to force wrapping it!\n");
                }

                });


        }

        void on_ogl_load(HMODULE tOHGL) override
        {
            static bool inited = false;
            opengl_addr = tOHGL;
            if (!inited) {
                wglGetProcAddressD = CreateInlineHook((void*)GetProcAddress(tOHGL, "wglGetProcAddress"), wglGetProcAddress_hook);
                inited = true;
            }

            fglGetIntegerv = (PFNGLGETINTEGERVPROC)GetProcAddress(tOHGL, "glGetIntegerv");
            fglGetFloatv = (PFNGLGETFLOATVPROC)GetProcAddress(tOHGL, "glGetFloatv");
            fglIsEnabled = (PFNGLISENABLEDPROC)GetProcAddress(tOHGL, "glIsEnabled");
            fglEnable = (PFNGLENABLEPROC)GetProcAddress(tOHGL, "glEnable");
            fglDisable = (PFNGLENABLEPROC)GetProcAddress(tOHGL, "glDisable");


            fGlGetString = (PFNGLGETSTRINGPROC)GetProcAddress(tOHGL, "glGetString");


            auto pattern1 = hook::pattern("51 53 56 33 F6 57 68");
            if (!pattern1.empty())
                saved_addr = (uintptr_t)pattern1.get_first();
            if (saved_addr)
                static auto whatever = safetyhook::create_mid(saved_addr, [](SafetyHookContext& ctx) {
                auto realWglGetProcAddress = (void* (__stdcall*)(const char*))GetProcAddress(opengl_addr, "wglGetProcAddress");
                // Load GLSL functions manually - game won't request these

                if(!r_fog_drawsun_workaround)
                    r_fog_drawsun_workaround = Cevar_Get("r_fog_drawsun_workaround", 1, CVAR_ARCHIVE, 0, 1);

                fglCreateShader = (PFNGLCREATESHADERPROC)realWglGetProcAddress("glCreateShader");
                fglShaderSource = (PFNGLSHADERSOURCEPROC)realWglGetProcAddress("glShaderSource");
                fglCompileShader = (PFNGLCOMPILESHADERPROC)realWglGetProcAddress("glCompileShader");
                fglCreateProgram = (PFNGLCREATEPROGRAMPROC)realWglGetProcAddress("glCreateProgram");
                fglAttachShader = (PFNGLATTACHSHADERPROC)realWglGetProcAddress("glAttachShader");
                fglLinkProgram = (PFNGLLINKPROGRAMPROC)realWglGetProcAddress("glLinkProgram");
                fglUseProgram = (PFNGLUSEPROGRAMPROC)realWglGetProcAddress("glUseProgram");
                fglDeleteProgram = (PFNGLDELETEPROGRAMPROC)realWglGetProcAddress("glDeleteProgram");
                fglDeleteShader = (PFNGLDELETESHADERPROC)realWglGetProcAddress("glDeleteShader");
                fglGetShaderiv = (PFNGLGETSHADERIVPROC)realWglGetProcAddress("glGetShaderiv");
                fglGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)realWglGetProcAddress("glGetShaderInfoLog");
                fglGetProgramiv = (PFNGLGETPROGRAMIVPROC)realWglGetProcAddress("glGetProgramiv");
                fglGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)realWglGetProcAddress("glGetProgramInfoLog");

                fglGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)realWglGetProcAddress("glGetUniformLocation");
                fglUniform1i = (PFNGLUNIFORM1IPROC)realWglGetProcAddress("glUniform1i");
                fglUniform1f = (PFNGLUNIFORM1FPROC)realWglGetProcAddress("glUniform1f");
                fglUniform4f = (PFNGLUNIFORM4FPROC)realWglGetProcAddress("glUniform4f");

                printf("[OpenGL] Loaded GLSL function pointers:\n");
                printf("  fglCreateShader: %p\n", fglCreateShader);
                printf("  fglShaderSource: %p\n", fglShaderSource);
                printf("  fglCompileShader: %p\n", fglCompileShader);
                printf("  fglCreateProgram: %p\n", fglCreateProgram);
                printf("  fglLinkProgram: %p\n", fglLinkProgram);
                printf("  fglUseProgram: %p\n", fglUseProgram);

                    });

        }
    };
}

REGISTER_COMPONENT(opengl_ati_frag::component);