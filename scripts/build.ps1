$ErrorActionPreference = "Stop"
Set-Location (Split-Path -Parent $PSScriptRoot)

cmake -B build -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Debug