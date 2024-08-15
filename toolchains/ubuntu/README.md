# Instructions
Clone the repository recursively so all the submodules are installed.
```git clone https://github.com/williamyang98/SDRPlusPlus-DAB-Radio-Plugin.git --recurse-submodules```

## Steps
1. Install packages: ```./toolchains/ubuntu/install_packages.sh```.
2. Configure cmake: ```cmake . -B build --preset gcc -DCMAKE_BUILD_TYPE=Release```.
3. Build: ```cmake --build build --config Release```.

## Install files
Copy ```build/src/libdab_plugin.so``` into ```/usr/lib/sdrpp/plugins/```.

Make sure that the version of SDR++ in ```vendor/sdrplusplus``` is the same as your SDR++ installation. 

