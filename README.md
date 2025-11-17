psensor
- adjustment to avoid too small sensorlist area
- fix memory leak.
- support CMakefile.

psensor — CMake build

This repository has been migrated from autotools to CMake. The CMake build
produces two targets:

- psensor — the GUI application
- psensor-server — the HTTP server

Quick build

```bash
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

Install

```bash
# install to default prefix (/usr/local)
sudo cmake --install .

# or configure prefix explicitly
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make -j
sudo make install
```

Options and feature toggles

You can enable/disable optional features using CMake options when configuring:

- WITH_GTOP=ON|OFF — enable libgtop support (default ON)
- WITH_NVIDIA=ON|OFF — try to detect and enable NVIDIA support (default ON)
- WITH_ATI=ON|OFF — try to enable ATI ADL support (default OFF)

If required development packages are missing, the build system will silently
exclude platform-specific source files and compile with the fallback stubs.

Notes

- The CMake build provides compile-time definitions that mimic the autoconf
  macros (HAVE_LIBSENSORS, PACKAGE_DATA_DIR, DATADIR, etc.).
- The original autotools files are still present in the repo. If you want
  me to move or remove them, tell me and I will back them up first.

Build .deb for Ubuntu:
```bash
make-deb.sh
``` 

