# Instructions
Clone the repository recursively so all the submodules are installed.
```git clone https://github.com/williamyang98/SDRPlusPlus-DAB-Radio-Plugin.git --recurse-submodules```

## Steps
1. Update brew: ```brew update```
2. Install packages: ```./toolchains/macos/install_packages.sh```
3. Install mako: ```pip3 install mako```
4. Install volk: 
    - Change to volk: ```cd vendor/volk```
    - Configure cmake: ```cmake . -B build -DCMAKE_BUILD_TYPE=Release -G Ninja```
    - Build and install: ```sudo cmake --build build --config Release --target install```
5. Configure cmake: ```cmake . -B build --preset clang -DCMAKE_BUILD_TYPE=Release```
6. Build: ```cmake --build build --config Release --target dab_plugin```

## Install files
Copy ```build/src/dab_plugin.dylib``` into your SDR++ modules directory.

Make sure that the version of SDR++ in ```vendor/sdrplusplus``` is the same as your SDR++ installation. 

