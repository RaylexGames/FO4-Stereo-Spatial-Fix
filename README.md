# FO4 Stereo Spatial Fix

Lightweight native x64 **F4SE plugin** that corrects directional volume loss and problematic stereo positional mixing in Fallout 4.

**Current release:** v0.1.1  
**Maintainer:** RaylexGames

## What it does

Fallout 4 uses X3DAudio to calculate how positional sounds are distributed across output channels. FO4 Stereo Spatial Fix hooks `X3DAudioCalculate`, lets Microsoft's original calculation run first, then adjusts only the resulting output matrix.

The plugin is designed to reduce cases where a positional sound becomes noticeably quieter simply because the player turns to face it directly.

## Compatibility

| Audio layout | v0.1.1 status |
|---|---|
| 2.0 stereo / headphones | **Tested / primary target** |
| 2.1 device exposed to Windows as stereo | **Expected to behave as stereo** |
| True 3-channel L/R/LFE | **Not handled by v0.1.1** |
| Native 5.1 / 7.1 | **Not validated as native surround support** |

Tested game configuration:

- Fallout 4 runtime `1.11.221`
- F4SE `0.7.8`
- Windows x64
- Stereo output

## Technical characteristics

- No ESP/ESL/ESM
- No Papyrus scripts
- No save dependency
- No audio file replacements
- No CommonLibF4
- No Address Library
- No hardcoded Fallout 4 executable offsets

## Installation

Install the compiled plugin to:

```text
Data\F4SE\Plugins\FO4StereoSpatialFix.dll
```

Launch Fallout 4 through F4SE.

## Source availability

The source is published for transparency, auditing and reproducibility. See `LICENSE` for permissions.

## Credits

- **RaylexGames** — project author, testing, maintenance and release
- **F4SE Team** — Fallout 4 Script Extender and plugin API
- **Microsoft** — X3DAudio and Windows APIs
