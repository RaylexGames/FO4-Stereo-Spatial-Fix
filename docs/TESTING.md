# Testing notes

## Next Gen

Target:

- Fallout 4 1.11.221
- F4SE 0.7.8

Status: tested in-game with stereo output.

Recommended smoke test:

1. Install the Next Gen option through the FOMOD.
2. Launch Fallout 4 through F4SE.
3. Stand near a stable positional source such as a radio, generator or machine.
4. Rotate slowly through 360 degrees.
5. Confirm that facing the source no longer produces the original stereo volume dip.
6. Confirm normal distance attenuation and positional movement.

## Old Gen

Target:

- Fallout 4 1.10.163
- F4SE 0.6.23

Status: ABI and runtime guard implemented; broader in-game community validation requested.

Verify:

1. F4SE loads the plugin without rejecting it.
2. Fallout 4 reaches gameplay without a crash.
3. The same 360-degree positional-audio test behaves correctly.
4. Stereo/2.1 behavior matches Next Gen.
5. 5.1/7.1 users should confirm that native surround placement remains intact.

## Output layouts

- 2.0: correction applied.
- True 2.1: FL/FR corrected; LFE remains untouched.
- 5.1 / 7.1: native X3DAudio matrix preserved.
- Unknown/custom multichannel: native matrix preserved.
