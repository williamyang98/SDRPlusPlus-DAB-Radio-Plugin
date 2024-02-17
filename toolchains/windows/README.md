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

### Modify CMakeLists.txt and vcpkg.json for SSE2, AVX or AVX2 
We are using the MSVC C++ compiler since SDR++ doesn't compile with clang on windows. This requires changing the ```/arch:XXX``` option to use the correct architecture.
Modify [CMakeLists.txt](https://github.com/williamyang98/SDRPlusPlus-DAB-Radio-Plugin/blob/6a4ce5587dd53fcf1ff4d1122c3f74d97798508a/CMakeLists.txt#L76-L77) which is located at the root of the project.
- ```/arch:AVX2```
- ```/arch:AVX```
- Remove the ```/arch:XXX``` flag entirely for SSE2 builds

*(Optional)* FFTW3 is built by default with AVX2 instructions. Modify [vcpkg.json](https://github.com/williamyang98/SDRPlusPlus-DAB-Radio-Plugin/blob/6a4ce5587dd53fcf1ff4d1122c3f74d97798508a/vcpkg.json#L17) so fftw3 uses the correct "features" for your CPU. This might not be necessary since it uses ```cpu_features``` to determine and dispatch calls at runtime.
- ```"features": ["avx"]```
- ```"features": ["sse2"]``` 
- ```"features": ["sse"]```

## Building plugin
If the vcpkg toolchain file is in a different location modify the ```cmake_configure.sh``` file.

1. Setup Visual Studio C++ **CMD** development environment. Can be found under ```Visual Studio 20XX``` in start menu.
2. Create python virtual environment: ```python -m venv venv```
3. Activate python virtual environment: ```.\venv\Scripts\activate.bat```
4. Install mako: ```pip install mako```
5. Configure cmake: ```cmake . -B build-windows -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:\tools\vcpkg\scripts\buildsystem\vcpkg.cmake``` 
    - Adjust ```-DCMAKE_TOOLCHAIN_FILE``` to point to your vcpkg installation directory.
6. Build plugin: ```cmake --build build-windows --config Release --target dab_plugin```
7. Copy ```dab_plugin.dll``` from build output directory
    - Ninja: ```.\build-windows\src\dab_plugin.dll```
    - MsBuild: ```.\build-windows\src\Release\dab_plugin.dll```
8. Copy ```dab_plugin.dll``` to ```SDRPP_INSTALL_LOCATION/modules/dab_plugin.dll```

## Additional notes
Make sure that the version of SDR++ in ```vendor/sdrplusplus``` is the same as your SDR++ installation. For nightly v1.1.0 builds you might not need to have the exact version but breaking changes between v1.1.0 nightly builds is expected for SDR++.

