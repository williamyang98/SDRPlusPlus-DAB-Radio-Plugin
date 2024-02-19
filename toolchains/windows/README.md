# Instructions
Clone the repository recursively so all the submodules are installed.

```git clone https://github.com/williamyang98/SDRPlusPlus-DAB-Radio-Plugin.git --recurse-submodules```

## Prerequistes
### Volk
Volk requires the following dependencies.
- python 3.4 or greater
- ```mako``` which can be installed through ```pip install mako```

### Visual Studio
Install Visual Studio 2022 with C++ build kit.

### vcpkg package manager
Install vcpkg and integrate install. Refer to instructions [here](https://github.com/microsoft/vcpkg#quick-start-windows)

## Building plugin
If the vcpkg toolchain file is in a different location modify the ```cmake_configure.sh``` file.

1. Setup Visual Studio C++ **CMD** development environment. Can be found under ```Visual Studio 20XX``` in start menu.
2. Create python virtual environment: ```python -m venv venv```
3. Activate python virtual environment: ```.\venv\Scripts\activate.bat```
4. Install mako: ```pip install mako```
5. Configure cmake: ```cmake . -B build --preset windows-msvc -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:\tools\vcpkg\scripts\buildsystem\vcpkg.cmake``` 
    - Adjust ```-DCMAKE_TOOLCHAIN_FILE``` to point to your vcpkg installation directory.
    - Use ```--preset windows-msvc-*``` instead to change compile flags for ```[sse2,avx,avx2]``` for better architecture specific optimisations.
6. Build plugin: ```cmake --build build --config Release --target dab_plugin```
7. Copy ```dab_plugin.dll``` from build output directory
    - Ninja: ```.\build\src\dab_plugin.dll```
    - MsBuild: ```.\build\src\Release\dab_plugin.dll```
8. Copy ```dab_plugin.dll``` to ```<SDRPP_INSTALL_LOCATION>/modules/dab_plugin.dll```

## Modifying build 
### Compile for SSE2, AVX or AVX2
We are using the MSVC C++ compiler since SDR++ doesn't compile with clang on windows.
- Compile for your architecture using in the configure cmake step (shown below) using the ```--preset windows-msvc-*``` option. 
    - avx2: ```--preset windows-msvc-avx2```
    - avx: ```--preset windows-msvc-avx```
    - sse2: ```--preset windows-msvc-sse2```
- You can also modify ```CMakePresets.json``` or create your own ```CMakeUserPresets.json``` with your own compiler options.

*(Optional)* FFTW3 is built by default with AVX2 instructions. Modify [vcpkg.json](https://github.com/williamyang98/SDRPlusPlus-DAB-Radio-Plugin/blob/6a4ce5587dd53fcf1ff4d1122c3f74d97798508a/vcpkg.json#L17) so fftw3 uses the correct "features" for your CPU. This might not be necessary since it uses ```cpu_features``` to determine and dispatch calls at runtime.
- ```"features": ["avx"]```
- ```"features": ["sse2"]``` 
- ```"features": ["sse"]```

### Use different build generator
You can also modify ```CMakePresets.json``` or create your own ```CMakeUserPresets.json``` with your own ```"generator": ?``` choice if Ninja is not available on your system.
Possible generators can be listed using ```cmake --help```.

## Additional notes
Make sure that the version of SDR++ in ```vendor/sdrplusplus``` is the same as your SDR++ installation. For nightly v1.1.0 builds you might not need to have the exact version but breaking changes between v1.1.0 nightly builds is expected for SDR++.

