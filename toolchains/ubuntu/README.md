# Instructions
Clone the repository recursively so all the submodules are installed.
```git clone https://github.com/FiendChain/SDRPlusPlus-DAB-Radio-Plugin```

## Steps
1. ```./toolchains/ubuntu/install_packages.sh```.
2. ```./toolchains/ubuntu/cmake_configure.sh```.
3. ```./toolchains/ubuntu/build.sh```.
4. ```./toolchains/ubuntu/create_package.sh```.

## Install files
Copy the contents of ```plugin_package/``` into ```/usr/lib/sdrpp/plugins/```.

Make sure that the version of SDR++ in ```vendor/sdrplusplus``` is the same as your SDR++ installation. 

