# Building FO4 Stereo Spatial Fix v0.2.1

The project builds two DLLs from the same C source file.

Requirements:

- LLVM/Clang with `clang-cl`
- `lld-link`
- x64 Windows target

## 1. Create the minimal KERNEL32 import library

```bat
lld-link /lib /machine:x64 /def:src\kernel32.def /out:kernel32.lib
```

## 2. Build Next Gen

Targets Fallout 4 1.11.221 and the F4SE 0.7.x declarative plugin ABI.

```bat
clang-cl --target=x86_64-pc-windows-msvc /c /O2 /GS- /GR- /Zl /Fo:FO4StereoSpatialFix.nextgen.obj src\FO4StereoSpatialFix.c

lld-link /DLL /NOENTRY /MACHINE:X64 /NODEFAULTLIB ^
  /DEF:src\FO4StereoSpatialFix.nextgen.def ^
  /OUT:FO4StereoSpatialFix.nextgen.dll ^
  FO4StereoSpatialFix.nextgen.obj kernel32.lib
```

## 3. Build Old Gen

Targets Fallout 4 1.10.163 and the F4SE 0.6.x Query/Load ABI.

```bat
clang-cl --target=x86_64-pc-windows-msvc /DFO4SSF_OLDGEN=1 /c /O2 /GS- /GR- /Zl /Fo:FO4StereoSpatialFix.oldgen.obj src\FO4StereoSpatialFix.c

lld-link /DLL /NOENTRY /MACHINE:X64 /NODEFAULTLIB ^
  /DEF:src\FO4StereoSpatialFix.oldgen.def ^
  /OUT:FO4StereoSpatialFix.oldgen.dll ^
  FO4StereoSpatialFix.oldgen.obj kernel32.lib
```

The two builds share the same topology and matrix-correction implementation. Only the F4SE loader adapter differs.
