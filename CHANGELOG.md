# Changelog

## 0.1.1

- Replaced the experimental root `x3daudio1_7.dll` proxy from 0.1.0 with an F4SE plugin.
- Added runtime-independent IAT hooking for `X3DAudioCalculate`.
- Original Microsoft X3DAudio calculation always executes first.
- Added corrective fold-down for internal 4/5.1/7.1 destination matrices.
- Added bounded constant-power correction for true stereo matrices.
- No hardcoded Fallout 4 addresses or game structure access.
- Confirmed working in-game on Fallout 4 `1.11.221` / F4SE `0.7.8` in stereo output testing.

## 0.1.0 - Withdrawn

- Experimental X3DAudio proxy build.
- Withdrawn because it could prevent Fallout 4 from launching on a real installation.
