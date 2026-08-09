# FO4 Stereo Spatial Fix

Native x64 F4SE plugin that corrects directional volume loss in Fallout 4 stereo output while preserving the game's original positional audio behavior.

**Version:** 0.2.1  
**Author:** RaylexGames

## What it fixes

Some positional sounds in Fallout 4 can lose perceived volume as the listener turns toward the sound source. The plugin lets Microsoft's original X3DAudio calculation run first, then applies a bounded constant-power correction only when the detected output topology is stereo or true 2.1.

It does not replace sound files and does not alter saves.

## One download, two game generations

The Nexus package uses a FOMOD installer with one required choice:

### Next Gen

- Fallout 4 runtime `1.11.221`
- F4SE `0.7.8`
- Modern F4SE declarative plugin interface

### Old Gen

- Fallout 4 runtime `1.10.163`
- F4SE `0.6.23`
- F4SE Query/Load plugin interface
- The Old Gen DLL rejects unsupported runtimes during `F4SEPlugin_Query`

Both builds use the same audio and output-topology engine.

## Output topology behavior

| Output | Behavior |
|---|---|
| Stereo / headphones (2.0) | Constant-power FL/FR correction |
| True 2.1 (FL/FR/LFE) | Constant-power FL/FR correction; LFE preserved |
| 5.1 | Native X3DAudio matrix preserved |
| 7.1 | Native X3DAudio matrix preserved |
| Unknown/custom multichannel | Safe native passthrough |

The plugin hooks `X3DAudioInitialize` when available to capture the real `SpeakerChannelMask`. It then matches that topology to subsequent `X3DAudioCalculate` calls.

If topology information is unavailable, only a two-channel destination receives the established stereo correction. Unknown multichannel layouts are left untouched.

## Installation

Install the Nexus archive through Vortex or another FOMOD-capable mod manager.

Choose exactly one option:

- **Next Gen — Fallout 4 1.11.221 / F4SE 0.7.8**
- **Old Gen — Fallout 4 1.10.163 / F4SE 0.6.23**

The installer places one file at:

```text
Data\F4SE\Plugins\FO4StereoSpatialFix.dll
```

Do not install both DLL builds at the same time.

## Technical characteristics

- Native x64 F4SE plugin
- No ESP/ESL/ESM
- No Papyrus scripts
- No save dependency
- No audio file replacements
- No CommonLibF4
- No Address Library
- No hardcoded Fallout 4 executable offsets
- Runtime PE import-table hook for X3DAudio
- Unknown multichannel configurations fail safe by preserving the native matrix

## Runtime status

**Next Gen 1.11.221:** tested in-game.  
**Old Gen 1.10.163:** build and F4SE ABI are implemented; broader in-game community validation is requested.

## Diagnostics

The plugin writes lightweight diagnostic messages through `OutputDebugStringA`, including detected topology and the correction mode selected.

## Source

Source code is provided for transparency, auditing and reproducibility.

## Credits

- **RaylexGames** — project author, testing, maintenance and releases
- **F4SE Team** — Fallout 4 Script Extender and plugin API
- **Microsoft** — X3DAudio and Windows APIs
