// FO4 Stereo Spatial Fix v0.1.1
// F4SE plugin, runtime-independent IAT hook. No CommonLib, no Address Library, no CRT.

#if !defined(_WIN64)
#error Win64 only
#endif

#define EXPORT __declspec(dllexport)
#define WINAPI __stdcall
#define CDECL __cdecl

typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;
typedef unsigned long long U64;
typedef long LONG;
typedef int BOOL;
typedef void* HANDLE;
typedef void* HMODULE;
typedef void* LPVOID;

#define TRUE 1
#define FALSE 0
#define PAGE_READWRITE 0x04u
#define IMAGE_ORDINAL_FLAG64 0x8000000000000000ULL
#define X3DAUDIO_CALCULATE_MATRIX 0x00000001u

__declspec(dllimport) HMODULE WINAPI GetModuleHandleW(const U16* name);
__declspec(dllimport) void* WINAPI GetProcAddress(HMODULE module, const char* name);
__declspec(dllimport) BOOL WINAPI VirtualProtect(LPVOID address, U64 size, U32 newProtect, U32* oldProtect);
__declspec(dllimport) BOOL WINAPI FlushInstructionCache(HANDLE process, const void* address, U64 size);
__declspec(dllimport) HANDLE WINAPI GetCurrentProcess(void);
__declspec(dllimport) void WINAPI OutputDebugStringA(const char* text);

// Keep clang/lld freestanding: MSVC-style floating point marker.
int _fltused = 0;

typedef struct X3DAUDIO_LISTENER X3DAUDIO_LISTENER;
typedef struct X3DAUDIO_EMITTER X3DAUDIO_EMITTER;

typedef struct X3DAUDIO_DSP_SETTINGS {
    float* pMatrixCoefficients;
    float* pDelayTimes;
    U32 SrcChannelCount;
    U32 DstChannelCount;
    float LPFDirectCoefficient;
    float LPFReverbCoefficient;
    float ReverbLevel;
    float DopplerFactor;
    float EmitterToListenerAngle;
    float EmitterToListenerDistance;
    float EmitterVelocityComponent;
    float ListenerVelocityComponent;
} X3DAUDIO_DSP_SETTINGS;

typedef void (CDECL *PFN_X3DAudioCalculate)(const U8* instance,
    const X3DAUDIO_LISTENER* listener,
    const X3DAUDIO_EMITTER* emitter,
    U32 flags,
    X3DAUDIO_DSP_SETTINGS* settings);

static PFN_X3DAudioCalculate g_originalCalculate = 0;
static int g_hookInstalled = 0;

static int ascii_ieq(const char* a, const char* b) {
    U8 ca, cb;
    if (!a || !b) return 0;
    for (;;) {
        ca = (U8)*a++; cb = (U8)*b++;
        if (ca >= 'A' && ca <= 'Z') ca = (U8)(ca + ('a' - 'A'));
        if (cb >= 'A' && cb <= 'Z') cb = (U8)(cb + ('a' - 'A'));
        if (ca != cb) return 0;
        if (!ca) return 1;
    }
}

static float f_abs(float x) { return x < 0.0f ? -x : x; }

// CRT-free square-root helper.
static float fast_sqrt(float x) {
    float g;
    int i;
    if (x <= 0.0f) return 0.0f;
    g = x > 1.0f ? x : 1.0f;
    for (i = 0; i < 6; ++i) g = 0.5f * (g + x / g);
    return g;
}

static void constant_power_stereo(float* matrix, U32 src) {
    U32 s;
    for (s = 0; s < src; ++s) {
        float l = matrix[s];
        float r = matrix[src + s];
        float sum = f_abs(l) + f_abs(r);
        float energy = fast_sqrt(l*l + r*r);
        // Compensate center-position energy loss while preserving coefficient sum
        // as an approximation of distance attenuation. Boost is capped at sqrt(2).
        if (sum > 0.00001f && energy > 0.00001f) {
            float scale = sum / energy;
            if (scale > 1.41421356f) scale = 1.41421356f;
            if (scale > 1.0f) {
                matrix[s] = l * scale;
                matrix[src + s] = r * scale;
            }
        }
    }
}

static void rescue_multichannel_to_front(float* m, U32 src, U32 dst) {
    U32 s, d;
    // WAVEFORMATEXTENSIBLE channel order used by X3DAudio/XAudio2:
    // 0 FL, 1 FR, 2 FC, 3 LFE, 4 BL/SL, 5 BR/SR, 6 SL, 7 SR for 7.1.
    // Exclude LFE from directional stereo fold-down.
    for (s = 0; s < src; ++s) {
        float l = m[s];
        float r = m[src + s];
        if (dst > 2) {
            float c = m[2 * src + s];
            l += 0.70710678f * c;
            r += 0.70710678f * c;
        }
        if (dst > 4) l += 0.70710678f * m[4 * src + s];
        if (dst > 5) r += 0.70710678f * m[5 * src + s];
        if (dst > 6) l += 0.70710678f * m[6 * src + s];
        if (dst > 7) r += 0.70710678f * m[7 * src + s];

        // Clear destination rows and write the corrected front stereo pair.
        for (d = 0; d < dst; ++d) m[d * src + s] = 0.0f;
        m[s] = l;
        if (dst > 1) m[src + s] = r;
    }
    constant_power_stereo(m, src);
}

static void CDECL Hook_X3DAudioCalculate(const U8* instance,
    const X3DAUDIO_LISTENER* listener,
    const X3DAUDIO_EMITTER* emitter,
    U32 flags,
    X3DAUDIO_DSP_SETTINGS* settings) {

    U32 src, dst;
    PFN_X3DAudioCalculate original = g_originalCalculate;
    if (!original) return;

    // Run the original X3DAudio calculation before applying matrix correction.
    original(instance, listener, emitter, flags, settings);

    if (!settings || !(flags & X3DAUDIO_CALCULATE_MATRIX) || !settings->pMatrixCoefficients) return;
    src = settings->SrcChannelCount;
    dst = settings->DstChannelCount;
    if (!src || src > 16 || !dst || dst > 16) return;

    if (dst == 2) {
        constant_power_stereo(settings->pMatrixCoefficients, src);
    } else if (dst >= 4) {
        rescue_multichannel_to_front(settings->pMatrixCoefficients, src, dst);
    }
}

static int patch_x3daudio_iat(void) {
    U8* base = (U8*)GetModuleHandleW(0);
    U32 peOff, importRva;
    U8* nt;
    U8* optional;
    U8* desc;

    if (!base) return 0;
    if (*(U16*)base != 0x5A4D) return 0; // MZ
    peOff = *(U32*)(base + 0x3C);
    nt = base + peOff;
    if (*(U32*)nt != 0x00004550u) return 0; // PE\0\0
    optional = nt + 24;
    if (*(U16*)optional != 0x20B) return 0; // PE32+

    // PE32+ import-directory RVA.
    importRva = *(U32*)(optional + 112 + 8);
    if (!importRva) return 0;
    desc = base + importRva;

    for (;;) {
        U32 originalFirstThunk = *(U32*)(desc + 0);
        U32 nameRva = *(U32*)(desc + 12);
        U32 firstThunk = *(U32*)(desc + 16);
        U64* names;
        U64* slots;
        U32 i;
        const char* dllName;

        if (!originalFirstThunk && !nameRva && !firstThunk) break;
        if (!nameRva || !firstThunk) { desc += 20; continue; }
        dllName = (const char*)(base + nameRva);
        if (!ascii_ieq(dllName, "x3daudio1_7.dll")) { desc += 20; continue; }
        names = originalFirstThunk ? (U64*)(base + originalFirstThunk) : 0;
        slots = (U64*)(base + firstThunk);
        {
            static const U16 x3dName[] = { 'x','3','d','a','u','d','i','o','1','_','7','.','d','l','l',0 };
            HMODULE x3d = GetModuleHandleW(x3dName);
            U64 realByAddress = x3d ? (U64)GetProcAddress(x3d, "X3DAudioCalculate") : 0;

            for (i = 0; slots[i]; ++i) {
                int match = 0;
                U32 oldProtect;
                if (names && names[i]) {
                    U64 nameThunk = names[i];
                    if (nameThunk & IMAGE_ORDINAL_FLAG64) {
                        // Accept ordinal 1 as a fallback for legacy import libraries.
                        if ((nameThunk & 0xFFFFu) == 1u) match = 1;
                    } else {
                        const char* funcName = (const char*)(base + (U32)nameThunk + 2);
                        if (ascii_ieq(funcName, "X3DAudioCalculate")) match = 1;
                    }
                }
                if (!match && realByAddress && slots[i] == realByAddress) match = 1;
                if (!match) continue;

                g_originalCalculate = (PFN_X3DAudioCalculate)(U64)slots[i];
                if (!g_originalCalculate) return 0;
                if (!VirtualProtect(&slots[i], 8, PAGE_READWRITE, &oldProtect)) return 0;
                slots[i] = (U64)(void*)&Hook_X3DAudioCalculate;
                {
                    U32 ignored;
                    VirtualProtect(&slots[i], 8, oldProtect, &ignored);
                }
                FlushInstructionCache(GetCurrentProcess(), &slots[i], 8);
                return 1;
            }
        }
        return 0;
    }
    return 0;
}

// F4SE 0.7.x plugin version declaration.
typedef struct F4SEPluginVersionData {
    U32 dataVersion;
    U32 pluginVersion;
    char name[256];
    char author[256];
    U32 addressIndependence;
    U32 structureIndependence;
    U32 compatibleVersions[16];
    U32 seVersionRequired;
    U32 reservedNonBreaking;
    U32 reservedBreaking;
    U8 reserved[512];
} F4SEPluginVersionData;

EXPORT F4SEPluginVersionData F4SEPlugin_Version = {
    1,
    0x00010100u,
    "FO4 Stereo Spatial Fix",
    "FO4 Stereo Spatial Fix Project",
    1u, // kAddressIndependence_Signatures
    1u, // kStructureIndependence_NoStructs
    {0},
    0,
    0,
    0,
    {0}
};

EXPORT U8 CDECL F4SEPlugin_Load(const void* f4se) {
    (void)f4se;
    if (g_hookInstalled) return 1;
    g_hookInstalled = patch_x3daudio_iat();
    if (g_hookInstalled) {
        OutputDebugStringA("FO4 Stereo Spatial Fix v0.1.1: X3DAudio IAT hook installed.\n");
        return 1;
    }
    OutputDebugStringA("FO4 Stereo Spatial Fix v0.1.1: hook installation failed; plugin disabled.\n");
    return 0;
}

BOOL WINAPI DllMain(HMODULE module, U32 reason, LPVOID reserved) {
    (void)module; (void)reason; (void)reserved;
    return TRUE;
}
