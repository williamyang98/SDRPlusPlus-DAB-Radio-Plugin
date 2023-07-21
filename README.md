## Introduction
SDR++ DAB radio plugin. 

Before installing this plugin, make sure that your version of SDR++ is the same as the one in ```vendor/sdrplusplus```. If it isn't refer to the build instructions to compile for your version of SDR++.

![Screenshot](docs/plugin_screenshot.png)

Core algorithms for DAB radio are used from https://github.com/FiendChain/DAB-Radio.

SDR++ is at https://github.com/AlexandreRouma/SDRPlusPlus/tree/master/core.

| Directory | Description |
| --- | --- |
| src | Plugin code to use dab algorithms as a SDR++ plugin |
| vendor | third party dependencies for dab code |
| vendor/sdrplusplus | SDR++ core library used to link against for DLL module |
| vendor/DAB-Radio | Core algorithms used for DAB radio decoding |
| cmake | Find*.cmake files for third party cmake targets |

## Download instructions
Download from releases page or build using the instructions below. Make sure you download the correct version if available.

## Usage instructions
1. Paste ```dab_plugin.dll``` into ```modules/``` folder inside your SDR++ install.
2. Open SDR++.
3. Inside ```Module Manager``` tab on the left panel add ```dab_decoder``` as a plugin.
4. Browse to a valid DAB frequency using the following link: https://www.wohnort.org/dab/.
5. Check the OFDM/State tab to see if the OFDM demodulator is active.
6. Select the DAB/Channels tab and select one of the DAB+ channels.
7. If no sound is present, check the ```Sinks``` tab and make sure that ```DAB Radio``` is set to ```Audio``` and to the correct audio device.
8. You may also copy ```fftw3f.dll``` into the main SDR++ folder since it is compiled with AVX2 which may improve performance. Be sure to backup the original file before replacing it.

## Build instructions
Refer to ```./toolchains/README.md``` for build instructions.

## TODO
- Improve the user interface so that you can view as much information as the original GUI found [here](https://github.com/FiendChain/DAB-Radio).
- Improve integration with SDR++.
- Determine how to make this build crossplatform with all the necessary dependencies.
