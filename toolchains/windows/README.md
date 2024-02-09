# Instructions
Clone the repository recursively so all the submodules are installed.
```git clone https://github.com/FiendChain/SDRPlusPlus-DAB-Radio-Plugin --recurse-submodules```

## Prerequistes
### Volk
Volk requires the following dependencies.
- python 3.4 or greater
- ```mako``` which can be installed through ```pip install mako```

### Visual Studio
Install Visual Studio 2022 with C++ build kit.

### vcpkg package manager
Install vcpkg and integrate install. Refer to instructions [here](https://github.com/microsoft/vcpkg#quick-start-windows)

### Modify vcpkg.json and CMakeLists.txt for SSE, AVX or AVX2 
FFTW3 is built by default with AVX2 instructions. Modify ```vcpkg.json``` so fftw3 uses the correct "features" for your CPU. 
- ```"features": ["avx"]```
- ```"features": ["sse2"]``` 
- ```"features": ["sse"]```

We are using the MSVC C++ compiler since SDR++ doesn't compile with clang on windows. This requires changing the ```/arch:XXX``` option to use the correct architecture.
- ```/arch:AVX2```
- ```/arch:AVX```
- Remove the ```/arch:XXX``` flag entirely for SSE2 builds

## Building plugin
If the vcpkg toolchain file is in a different location modify the ```cmake_configure.sh``` file.

1. Setup Visual Studio C++ CMD development environment. Can be found under ```Visual Studio 20XX``` in start menu.
2. Create python virtual environment: ```python -m venv venv```
3. Activate python virtual environment: ```.\venv\Scripts\activate.bat```
4. Install mako: ```pip install mako```
5. Configure cmake: ```cmake . -B build-windows -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:\tools\vcpkg\scripts\buildsystem\vcpkg.cmake``` 
    - Adjust ```-DCMAKE_TOOLCHAIN_FILE``` to point to your vcpkg installation directory.
6. Build plugin: ```cmake --build build-windows --config Release --target dab_plugin```
7. Copy ```dab_plugin.dll``` from ```./build-windows```
    - Ninja: ```.\build-windows\src\dab_plugin.dll```
    - MsBuild: ```.\build-windows\src\Release\dab_plugin.dll```

## Install files
If you are using Ninja as the buildsystem, run ```./toolchains/windows/create_package.sh``` to place plugin files into folder. (Uses git-bash to run bash scripts).

Copy the contents of ```plugin_package/``` into your ```modules``` folder in your SDR++ installation.

Make sure that the version of SDR++ in ```vendor/sdrplusplus``` is the same as your SDR++ installation. 

