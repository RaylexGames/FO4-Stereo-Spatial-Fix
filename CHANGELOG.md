# Changelog

## v0.2.1

- Added a unified FOMOD installer with one required Next Gen / Old Gen selection.
- Added Next Gen support for Fallout 4 1.11.221 / F4SE 0.7.8.
- Added Old Gen support for Fallout 4 1.10.163 / F4SE 0.6.23.
- Added real output-topology tracking through `X3DAudioInitialize`.
- Added explicit true 2.1 handling: FL/FR correction with LFE preserved.
- Stereo and headphone output continue to use the bounded constant-power correction.
- Native 5.1 and 7.1 X3DAudio matrices are now preserved instead of being folded into stereo.
- Unknown/custom multichannel layouts use safe passthrough.
- Added lightweight output-topology diagnostics.
- Both game-generation builds now share one audio engine and differ only in their F4SE loader adapter.
- No ESP, Papyrus scripts, audio replacements, Address Library or executable offsets.

## v0.1.1

- First public release.
- Added the native F4SE X3DAudio matrix hook.
- Added the stereo directional-volume correction.
