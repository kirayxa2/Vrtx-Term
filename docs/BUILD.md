# Building VrtxTerm

## Prerequisites

You need a Windows 10 (build 1809+) or Windows 11 machine with the following:

### 1. Visual Studio 2022 Build Tools (free, no full VS install required)

Download the standalone installer from
<https://visualstudio.microsoft.com/visual-cpp-build-tools/>.

When the installer launches, on the **Workloads** tab tick:

- **Desktop development with C++**

That single workload pulls everything we need:

- MSVC v143 toolchain (cl.exe, link.exe)
- Windows 11 SDK (which also covers Windows 10 APIs)
- C++ CMake tools for Windows
- ATL/MFC are *not* required

Total disk: about 6–8 GB.

### 2. Git

Any recent version. <https://git-scm.com/download/win>

### 3. (Optional) CMake CLI

The Build Tools workload already ships CMake at
`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin`.
You can also install a standalone CMake from <https://cmake.org/download/> and add it to
PATH.

## Building

Open the **x64 Native Tools Command Prompt for VS 2022** (Start menu → search). Then:

```bat
git clone https://github.com/kirayxa2/VrtxTerm.git
cd VrtxTerm

cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The binary lands at `build\bin\Release\VrtxTerm.exe`.

For a debug build:

```bat
cmake --build build --config Debug
```

## Common issues

### `error MSB8020: The build tools for Visual Studio 2022 cannot be found`

You opened a regular `cmd.exe` instead of the *x64 Native Tools Command Prompt*. Either
use that prompt, or set `vcvarsall.bat x64` in your shell first.

### `fatal error C1083: Cannot open include file: 'd2d1_3.h'`

The Windows SDK was not installed. Re-run the VS Installer and make sure the **Windows 11
SDK** component is checked under "Desktop development with C++".

### Black or fully transparent window after launch

Acrylic backdrop requires:
- Windows 10 build 1803 or newer
- Desktop composition enabled (it always is on Win10/11; only disabled on RDP without GPU
  acceleration)

If you see a flat tinted window without blur, check the Settings → System → Display →
Graphics settings: GPU acceleration must be enabled for `VrtxTerm.exe`.

## Running tests

There are no tests yet. Once the terminal core lands we will add unit tests for the VT
parser via Catch2.
