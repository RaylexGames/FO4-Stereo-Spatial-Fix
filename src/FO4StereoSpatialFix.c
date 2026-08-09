// FO4 Stereo Spatial Fix v0.2.1 — Output Topology Update
// F4SE plugin. Runtime-independent PE/IAT hook. No CommonLib, Address Library, or CRT.

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
#define X3DAUDIO_CALCULATE_MATRIX 0x00000001u

#define SPEAKER_FRONT_LEFT            0x00000001u
#define SPEAKER_FRONT_RIGHT           0x00000002u
#define SPEAKER_FRONT_CENTER          0x00000004u
#define SPEAKER_LOW_FREQUENCY         0x00000008u
#define SPEAKER_BACK_LEFT             0x00000010u
#define SPEAKER_BACK_RIGHT            0x00000020u
#define SPEAKER_FRONT_LEFT_OF_CENTER  0x00000040u
#define SPEAKER_FRONT_RIGHT_OF_CENTER 0x00000080u
#define SPEAKER_BACK_CENTER           0x00000100u
#define SPEAKER_SIDE_LEFT             0x00000200u
#define SPEAKER_SIDE_RIGHT            0x00000400u

#define SPEAKER_STEREO (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT)
#define SPEAKER_2POINT1 (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_LOW_FREQUENCY)
#define SPEAKER_5POINT1 (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT)
#define SPEAKER_5POINT1_SURROUND (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT)
#define SPEAKER_7POINT1 (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT | SPEAKER_FRONT_LEFT_OF_CENTER | SPEAKER_FRONT_RIGHT_OF_CENTER)
#define SPEAKER_7POINT1_SURROUND (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT)

__declspec(dllimport) HMODULE WINAPI GetModuleHandleW(const U16* name);
__declspec(dllimport) void* WINAPI GetProcAddress(HMODULE module, const char* name);
__declspec(dllimport) BOOL WINAPI VirtualProtect(LPVOID address, U64 size, U32 newProtect, U32* oldProtect);
__declspec(dllimport) BOOL WINAPI FlushInstructionCache(HANDLE process, const void* address, U64 size);
__declspec(dllimport) HANDLE WINAPI GetCurrentProcess(void);
__declspec(dllimport) void WINAPI OutputDebugStringA(const char* text);

// Keep clang/lld freestanding.
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

typedef LONG (CDECL *PFN_X3DAudioInitialize)(U32 speakerChannelMask,
    float speedOfSound,
    U8* instance);

typedef void (CDECL *PFN_X3DAudioCalculate)(const U8* instance,
    const X3DAUDIO_LISTENER* listener,
    const X3DAUDIO_EMITTER* emitter,
    U32 flags,
    X3DAUDIO_DSP_SETTINGS* settings);

typedef enum OUTPUT_TOPOLOGY {
    OUTPUT_TOPOLOGY_UNKNOWN = 0,
    OUTPUT_TOPOLOGY_STEREO,
    OUTPUT_TOPOLOGY_2POINT1,
    OUTPUT_TOPOLOGY_5POINT1,
    OUTPUT_TOPOLOGY_7POINT1,
    OUTPUT_TOPOLOGY_MULTICHANNEL,
    OUTPUT_TOPOLOGY_OTHER
} OUTPUT_TOPOLOGY;

typedef struct TOPOLOGY_RECORD {
    const U8* instance;
    U32 speakerMask;
    U32 channelCount;
    U32 generation;
} TOPOLOGY_RECORD;

#define TOPOLOGY_RECORD_COUNT 8u

static PFN_X3DAudioInitialize g_originalInitialize = 0;
static PFN_X3DAudioCalculate g_originalCalculate = 0;
static TOPOLOGY_RECORD g_topologies[TOPOLOGY_RECORD_COUNT];
static U32 g_topologyGeneration = 0;
static U32 g_hooksInstalled = 0;
static int g_loggedUnknownStereoFallback = 0;
static int g_loggedUnknownMultichannel = 0;

static int ascii_ieq(const char* a, const char* b) {
    U8 ca, cb;
    if (!a || !b) return 0;
    for (;;) {
        ca = (U8)*a++;
        cb = (U8)*b++;
        if (ca >= 'A' && ca <= 'Z') ca = (U8)(ca + ('a' - 'A'));
        if (cb >= 'A' && cb <= 'Z') cb = (U8)(cb + ('a' - 'A'));
        if (ca != cb) return 0;
        if (!ca) return 1;
    }
}

static float f_abs(float x) { return x < 0.0f ? -x : x; }

static float fast_sqrt(float x) {
    float g;
    int i;
    if (x <= 0.0f) return 0.0f;
    g = x > 1.0f ? x : 1.0f;
    for (i = 0; i < 6; ++i) g = 0.5f * (g + x / g);
    return g;
}

static U32 popcount32(U32 v) {
    U32 n = 0;
    while (v) {
        v &= (v - 1u);
        ++n;
    }
    return n;
}

static OUTPUT_TOPOLOGY classify_topology(U32 mask) {
    U32 channels = popcount32(mask);
    if (mask == SPEAKER_STEREO) return OUTPUT_TOPOLOGY_STEREO;
    if (mask == SPEAKER_2POINT1) return OUTPUT_TOPOLOGY_2POINT1;
    if (mask == SPEAKER_5POINT1 || mask == SPEAKER_5POINT1_SURROUND) return OUTPUT_TOPOLOGY_5POINT1;
    if (mask == SPEAKER_7POINT1 || mask == SPEAKER_7POINT1_SURROUND) return OUTPUT_TOPOLOGY_7POINT1;
    if (channels >= 4u) return OUTPUT_TOPOLOGY_MULTICHANNEL;
    if (channels > 0u) return OUTPUT_TOPOLOGY_OTHER;
    return OUTPUT_TOPOLOGY_UNKNOWN;
}

static char* append_text(char* p, const char* end, const char* text) {
    while (p < end && *text) *p++ = *text++;
    return p;
}

static char* append_hex32(char* p, const char* end, U32 value) {
    static const char digits[] = "0123456789ABCDEF";
    int shift;
    for (shift = 28; shift >= 0 && p < end; shift -= 4) {
        *p++ = digits[(value >> shift) & 0xFu];
    }
    return p;
}

static char* append_u32(char* p, const char* end, U32 value) {
    char tmp[10];
    U32 count = 0;
    if (!value) {
        if (p < end) *p++ = '0';
        return p;
    }
    while (value && count < 10u) {
        tmp[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    while (count && p < end) *p++ = tmp[--count];
    return p;
}

static const char* topology_label(OUTPUT_TOPOLOGY topology) {
    switch (topology) {
        case OUTPUT_TOPOLOGY_STEREO: return "stereo";
        case OUTPUT_TOPOLOGY_2POINT1: return "2.1";
        case OUTPUT_TOPOLOGY_5POINT1: return "5.1";
        case OUTPUT_TOPOLOGY_7POINT1: return "7.1";
        case OUTPUT_TOPOLOGY_MULTICHANNEL: return "multichannel";
        case OUTPUT_TOPOLOGY_OTHER: return "custom";
        default: return "unknown";
    }
}

static const char* topology_mode(OUTPUT_TOPOLOGY topology) {
    if (topology == OUTPUT_TOPOLOGY_STEREO) return "constant-power FL/FR correction";
    if (topology == OUTPUT_TOPOLOGY_2POINT1) return "constant-power FL/FR correction, LFE preserved";
    return "native X3DAudio passthrough";
}

static void log_topology(U32 mask) {
    char buffer[224];
    char* p = buffer;
    const char* end = buffer + sizeof(buffer) - 2;
    OUTPUT_TOPOLOGY topology = classify_topology(mask);

    p = append_text(p, end, "FO4 Stereo Spatial Fix v0.2.1: topology mask=0x");
    p = append_hex32(p, end, mask);
    p = append_text(p, end, ", channels=");
    p = append_u32(p, end, popcount32(mask));
    p = append_text(p, end, ", layout=");
    p = append_text(p, end, topology_label(topology));
    p = append_text(p, end, ", mode=");
    p = append_text(p, end, topology_mode(topology));
    if (p < end) *p++ = '\n';
    *p = 0;
    OutputDebugStringA(buffer);
}

static void record_topology(const U8* instance, U32 mask) {
    U32 i;
    U32 oldest = 0;
    U32 oldestGeneration = 0xFFFFFFFFu;
    U32 channels;

    if (!instance || !mask) return;
    channels = popcount32(mask);
    if (!channels || channels > 18u) return;

    ++g_topologyGeneration;
    if (!g_topologyGeneration) ++g_topologyGeneration;

    for (i = 0; i < TOPOLOGY_RECORD_COUNT; ++i) {
        if (g_topologies[i].instance == instance) {
            g_topologies[i].speakerMask = mask;
            g_topologies[i].channelCount = channels;
            g_topologies[i].generation = g_topologyGeneration;
            log_topology(mask);
            return;
        }
        if (!g_topologies[i].instance) {
            g_topologies[i].speakerMask = mask;
            g_topologies[i].channelCount = channels;
            g_topologies[i].generation = g_topologyGeneration;
            g_topologies[i].instance = instance;
            log_topology(mask);
            return;
        }
        if (g_topologies[i].generation < oldestGeneration) {
            oldestGeneration = g_topologies[i].generation;
            oldest = i;
        }
    }

    g_topologies[oldest].instance = 0;
    g_topologies[oldest].speakerMask = mask;
    g_topologies[oldest].channelCount = channels;
    g_topologies[oldest].generation = g_topologyGeneration;
    g_topologies[oldest].instance = instance;
    log_topology(mask);
}

static const TOPOLOGY_RECORD* find_topology(const U8* instance) {
    U32 i;
    if (!instance) return 0;
    for (i = 0; i < TOPOLOGY_RECORD_COUNT; ++i) {
        if (g_topologies[i].instance == instance) return &g_topologies[i];
    }
    return 0;
}

static void constant_power_front_pair(float* matrix, U32 src) {
    U32 s;
    for (s = 0; s < src; ++s) {
        float l = matrix[s];
        float r = matrix[src + s];
        float sum = f_abs(l) + f_abs(r);
        float energy = fast_sqrt(l*l + r*r);

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

static LONG CDECL Hook_X3DAudioInitialize(U32 speakerChannelMask,
    float speedOfSound,
    U8* instance) {

    PFN_X3DAudioInitialize original = g_originalInitialize;
    LONG result;
    if (!original) return -1;

    result = original(speakerChannelMask, speedOfSound, instance);
    if (result >= 0) record_topology(instance, speakerChannelMask);
    return result;
}

static void CDECL Hook_X3DAudioCalculate(const U8* instance,
    const X3DAUDIO_LISTENER* listener,
    const X3DAUDIO_EMITTER* emitter,
    U32 flags,
    X3DAUDIO_DSP_SETTINGS* settings) {

    U32 src, dst;
    const TOPOLOGY_RECORD* record;
    OUTPUT_TOPOLOGY topology;
    PFN_X3DAudioCalculate original = g_originalCalculate;
    if (!original) return;

    original(instance, listener, emitter, flags, settings);

    if (!settings || !(flags & X3DAUDIO_CALCULATE_MATRIX) || !settings->pMatrixCoefficients) return;
    src = settings->SrcChannelCount;
    dst = settings->DstChannelCount;
    if (!src || src > 16u || !dst || dst > 18u) return;

    record = find_topology(instance);
    if (record) {
        topology = classify_topology(record->speakerMask);

        // A topology record is only trusted when its speaker count agrees with
        // the destination matrix supplied by X3DAudioCalculate.
        if (record->channelCount != dst) return;

        if (topology == OUTPUT_TOPOLOGY_STEREO && dst == 2u) {
            constant_power_front_pair(settings->pMatrixCoefficients, src);
            return;
        }

        if (topology == OUTPUT_TOPOLOGY_2POINT1 && dst == 3u) {
            // Channel order follows the speaker-mask bit order: FL, FR, LFE.
            // Only the first two destination rows are modified.
            constant_power_front_pair(settings->pMatrixCoefficients, src);
            return;
        }

        // Native surround and custom layouts preserve Microsoft's complete
        // X3DAudio matrix. No stereo normalization or fold-down is performed.
        return;
    }

    // If initialization happened before this plugin was able to observe it,
    // only a two-channel destination receives the established stereo fix.
    // Three-channel and larger unknown layouts remain untouched rather than
    // guessing their speaker semantics from channel count alone.
    if (dst == 2u) {
        if (!g_loggedUnknownStereoFallback) {
            g_loggedUnknownStereoFallback = 1;
            OutputDebugStringA("FO4 Stereo Spatial Fix v0.2.1: topology unavailable; using safe 2-channel stereo fallback.\n");
        }
        constant_power_front_pair(settings->pMatrixCoefficients, src);
    } else if (!g_loggedUnknownMultichannel) {
        g_loggedUnknownMultichannel = 1;
        OutputDebugStringA("FO4 Stereo Spatial Fix v0.2.1: topology unavailable for multichannel output; preserving native matrix.\n");
    }
}

static int patch_iat_slot(U64* slot, U64 hookAddress, U64* originalAddress) {
    U32 oldProtect;
    U32 ignored;
    if (!slot || !*slot || !hookAddress || !originalAddress) return 0;
    if (*slot == hookAddress) return 1;
    *originalAddress = *slot;
    if (!VirtualProtect(slot, 8, PAGE_READWRITE, &oldProtect)) return 0;
    *slot = hookAddress;
    VirtualProtect(slot, 8, oldProtect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), slot, 8);
    return 1;
}

static U32 patch_x3daudio_iat(void) {
    U8* base = (U8*)GetModuleHandleW(0);
    U32 peOff, importRva;
    U8* nt;
    U8* optional;
    U8* desc;
    U32 patched = 0;

    if (!base) return 0;
    if (*(U16*)base != 0x5A4D) return 0;
    peOff = *(U32*)(base + 0x3C);
    nt = base + peOff;
    if (*(U32*)nt != 0x00004550u) return 0;
    optional = nt + 24;
    if (*(U16*)optional != 0x20B) return 0;

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
        if (!nameRva || !firstThunk) {
            desc += 20;
            continue;
        }

        dllName = (const char*)(base + nameRva);
        if (!ascii_ieq(dllName, "x3daudio1_7.dll")) {
            desc += 20;
            continue;
        }

        names = originalFirstThunk ? (U64*)(base + originalFirstThunk) : 0;
        slots = (U64*)(base + firstThunk);

        {
            static const U16 x3dName[] = { 'x','3','d','a','u','d','i','o','1','_','7','.','d','l','l',0 };
            HMODULE x3d = GetModuleHandleW(x3dName);
            U64 realInitialize = x3d ? (U64)GetProcAddress(x3d, "X3DAudioInitialize") : 0;
            U64 realCalculate = x3d ? (U64)GetProcAddress(x3d, "X3DAudioCalculate") : 0;

            for (i = 0; slots[i]; ++i) {
                int isInitialize = 0;
                int isCalculate = 0;

                if (names && names[i] && !(names[i] & 0x8000000000000000ULL)) {
                    const char* funcName = (const char*)(base + (U32)names[i] + 2);
                    if (ascii_ieq(funcName, "X3DAudioInitialize")) isInitialize = 1;
                    else if (ascii_ieq(funcName, "X3DAudioCalculate")) isCalculate = 1;
                }

                if (!isInitialize && realInitialize && slots[i] == realInitialize) isInitialize = 1;
                if (!isCalculate && realCalculate && slots[i] == realCalculate) isCalculate = 1;

                if (isInitialize && !(patched & 2u)) {
                    U64 original = 0;
                    if (patch_iat_slot(&slots[i], (U64)(void*)&Hook_X3DAudioInitialize, &original)) {
                        g_originalInitialize = (PFN_X3DAudioInitialize)original;
                        patched |= 2u;
                    }
                    continue;
                }

                if (isCalculate && !(patched & 1u)) {
                    U64 original = 0;
                    if (patch_iat_slot(&slots[i], (U64)(void*)&Hook_X3DAudioCalculate, &original)) {
                        g_originalCalculate = (PFN_X3DAudioCalculate)original;
                        patched |= 1u;
                    }
                }
            }
        }
        break;
    }

    return patched;
}

// Runtime adapter. The audio engine above is shared by both builds.

#if defined(FO4SSF_OLDGEN)

// F4SE 0.6.x Old Gen plugin ABI. Only the leading fields used by Query are
// declared here; their layout is stable in the 0.6.x PluginAPI.
typedef struct F4SEInterfaceLegacy {
    U32 f4seVersion;
    U32 runtimeVersion;
    U32 editorVersion;
    U32 isEditor;
} F4SEInterfaceLegacy;

typedef struct PluginInfoLegacy {
    U32 infoVersion;
    const char* name;
    U32 version;
} PluginInfoLegacy;

#define F4SE_LEGACY_PLUGIN_INFO_VERSION 1u
#define FALLOUT4_RUNTIME_1_10_163 0x010A0A30u

EXPORT U8 CDECL F4SEPlugin_Query(const F4SEInterfaceLegacy* f4se, PluginInfoLegacy* info) {
    if (!info) return 0;

    info->infoVersion = F4SE_LEGACY_PLUGIN_INFO_VERSION;
    info->name = "FO4 Stereo Spatial Fix";
    info->version = 0x00020100u;

    if (!f4se) return 0;
    if (f4se->isEditor) return 0;

    // This DLL is intentionally runtime-specific. The Next Gen build
    // uses the F4SE 0.7.x declarative ABI instead.
    if (f4se->runtimeVersion != FALLOUT4_RUNTIME_1_10_163) return 0;

    return 1;
}

EXPORT U8 CDECL F4SEPlugin_Load(const void* f4se) {
    (void)f4se;
    if (g_hooksInstalled & 1u) return 1;

    g_hooksInstalled = patch_x3daudio_iat();
    if (!(g_hooksInstalled & 1u)) {
        OutputDebugStringA("FO4 Stereo Spatial Fix v0.2.1 Old Gen: X3DAudioCalculate hook installation failed; plugin disabled.\n");
        return 0;
    }

    if (g_hooksInstalled & 2u) {
        OutputDebugStringA("FO4 Stereo Spatial Fix v0.2.1 Old Gen: output-topology and matrix hooks installed.\n");
    } else {
        OutputDebugStringA("FO4 Stereo Spatial Fix v0.2.1 Old Gen: matrix hook installed; topology hook unavailable, safe fallback active.\n");
    }
    return 1;
}

#else

// F4SE 0.7.x declarative plugin version data.
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
    0x00020100u,
    "FO4 Stereo Spatial Fix",
    "RaylexGames",
    1u,
    1u,
    {0},
    0,
    0,
    0,
    {0}
};

EXPORT U8 CDECL F4SEPlugin_Load(const void* f4se) {
    (void)f4se;
    if (g_hooksInstalled & 1u) return 1;

    g_hooksInstalled = patch_x3daudio_iat();
    if (!(g_hooksInstalled & 1u)) {
        OutputDebugStringA("FO4 Stereo Spatial Fix v0.2.1: X3DAudioCalculate hook installation failed; plugin disabled.\n");
        return 0;
    }

    if (g_hooksInstalled & 2u) {
        OutputDebugStringA("FO4 Stereo Spatial Fix v0.2.1: output-topology and matrix hooks installed.\n");
    } else {
        OutputDebugStringA("FO4 Stereo Spatial Fix v0.2.1: matrix hook installed; topology hook unavailable, safe fallback active.\n");
    }
    return 1;
}

#endif

BOOL WINAPI DllMain(HMODULE module, U32 reason, LPVOID reserved) {
    (void)module;
    (void)reason;
    (void)reserved;
    return TRUE;
}
