@rem rd /S /Q build

@rem cmake -G "Visual Studio 16 2019" -A x64 -Thost=x64 -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -Wno-dev -DEMBREE_ISPC_SUPPORT=FALSE -DEMBREE_TASKING_SYSTEM=INTERNAL -DEMBREE_TUTORIALS=OFF -DEMBREE_STATIC_LIB=ON
@rem cmake -G "Visual Studio 17 2022" -A x64 -Thost=x64 -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -Wno-dev -DEMBREE_ISPC_SUPPORT=FALSE -DEMBREE_TASKING_SYSTEM=INTERNAL -DEMBREE_TUTORIALS=OFF -DEMBREE_STATIC_LIB=ON
cmake -G "Visual Studio 17 2022" -A x64 -Thost=x64 -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -Wno-dev -DERHE_PROFILE_LIBRARY=tracy
@rem --graphviz=erhe.dot