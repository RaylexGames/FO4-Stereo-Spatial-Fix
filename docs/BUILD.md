# Build instructions

FO4 Stereo Spatial Fix is intentionally small and can be built as a freestanding Windows x64 DLL with `clang-cl` and `lld-link`.

## Requirements

- Windows x64
- LLVM/Clang toolchain with `clang-cl`
- `lld-link`
- Windows SDK providing `kernel32.lib` (recommended)

## Build

From the repository root:

```bat
clang-cl --target=x86_64-pc-windows-msvc /c /O2 /GS- /GR- /Zl /Fo:FO4StereoSpatialFix.obj src\FO4StereoSpatialFix.c
lld-link /DLL /NOENTRY /MACHINE:X64 /NODEFAULTLIB /DEF:src\FO4StereoSpatialFix.def /OUT:FO4StereoSpatialFix.dll FO4StereoSpatialFix.obj kernel32.lib
```

`src/kernel32.def` is also included as a minimal import definition for freestanding/custom toolchain setups.

## Output

The resulting plugin should be installed at:

```text
Data\F4SE\Plugins\FO4StereoSpatialFix.dll
```

## Notes

- The project intentionally avoids the CRT.
- No CommonLibF4 or Address Library is required.
- v0.1.1 uses a PE import-table hook rather than hardcoded Fallout 4 runtime offsets.
