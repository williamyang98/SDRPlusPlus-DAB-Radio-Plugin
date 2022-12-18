## Introduction

Code copy and pasted from https://github.com/FiendChain/DAB-Radio.

Changes made
- Move cmake/Find*.cmake to root of plugin directory
- Change libfaad to compile as static library instead of dynamic library to avoid introducing an extra DLL
- Added reset capability to basic radio