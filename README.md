## Introduction
SDR++ DAB radio plugin. 

NOTE: The release build was compiled against a nightly SDR++ build.

![Screenshot](docs/plugin_screenshot.png)

Core algorithms for DAB radio are used from https://github.com/FiendChain/DAB-Radio.

SDR++ core code was taken from https://github.com/AlexandreRouma/SDRPlusPlus/tree/master/core.

| Directory | Description |
| --- | --- |
| core | SDR++ core library used to link against for DLL module |
| dab | Core algorithms used for DAB radio decoding |
| dab_plugin | Plugin code to use dab algorithms as a SDR++ plugin |
| vendor | third party dependencies for dab code |
| cmake | Find*.cmake files for third party cmake targets |

## Download instructions
1. Download from releases page or build using the instructions below.

## Usage instructions
1. Paste ```dab_plugin.dll``` into ```modules/``` folder inside your SDR++ install.
2. Open SDR++.
3. Inside ```Module Manager``` tab on the left panel add ```dab_decoder``` as a plugin.
4. Browse to a valid DAB frequency using the following link: https://www.wohnort.org/dab/.
5. Check the OFDM/State tab to see if the OFDM demodulator is active.
6. Select the DAB/Channels tab and select one of the DAB+ channels.

## Build instructions
Refer to ```./toolchains/README.md``` for build instrutions.

## TODO
- Improve the user interface so that you can view as much information as the original GUI found [here](https://github.com/FiendChain/DAB-Radio).
- Improve integration with SDR++.
- Determine how to make this build crossplatform with all the necessary dependencies.
