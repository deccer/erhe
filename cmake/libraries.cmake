include(FetchContent)

FetchContent_Declare(
    bullet3
    #GIT_REPOSITORY https://github.com/bulletphysics/bullet3.git
    GIT_REPOSITORY https://github.com/tksuoran/bullet3.git
    GIT_TAG        origin/cmake-fetchcontent
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    bvh
    GIT_REPOSITORY https://github.com/madmann91/bvh.git
    GIT_TAG        origin/master
    #GIT_REPOSITORY https://github.com/tksuoran/bvh.git
    #GIT_TAG        dev
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    cgltf
    GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
    GIT_TAG        origin/master
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    concurrentqueue
    GIT_REPOSITORY https://github.com/cameron314/concurrentqueue.git
    GIT_TAG        origin/master
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    cpp-terminal
    GIT_REPOSITORY https://github.com/jupyter-xeus/cpp-terminal.git
    GIT_TAG        origin/master
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    cxxopts
    GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
    GIT_TAG        origin/master
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    embree
    GIT_REPOSITORY https://github.com/embree/embree.git
    GIT_TAG        v3.13.3
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)


FetchContent_Declare(
    etl
    GIT_REPOSITORY https://github.com/ETLCPP/etl.git
    GIT_TAG        20.35.10
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    flecs
    GIT_REPOSITORY https://github.com/SanderMertens/flecs.git
    GIT_TAG        origin/master
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    #GIT_TAG        origin/master
    GIT_TAG        9.1.0
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    freetype
    #GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
    GIT_REPOSITORY https://github.com/freetype/freetype.git
    GIT_TAG        VER-2-13-0
)

# Not in use yet
#FetchContent_Declare(
#    GeometricTools
#    GIT_REPOSITORY https://github.com/tksuoran/GeometricTools.git
#    GIT_TAG        cmake # GTE-version-5.14
#    GIT_SHALLOW    TRUE
#    GIT_PROGRESS   TRUE
#)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        origin/master
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    glslang
    GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
    GIT_TAG        origin/master
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

#FetchContent_Declare(
#    glTF-SDK
#    GIT_REPOSITORY https://github.com/microsoft/glTF-SDK.git
#    GIT_TAG        master
#    GIT_SHALLOW    TRUE
#    GIT_PROGRESS   TRUE
#)

# Not yet in use
#FetchContent_Declare(
#    googletest
#    GIT_REPOSITORY https://github.com/google/googletest.git
#    GIT_TAG        master
#    GIT_SHALLOW    TRUE
#    GIT_PROGRESS   TRUE
#)

FetchContent_Declare(
    GSL
    GIT_REPOSITORY https://github.com/microsoft/GSL.git
    GIT_TAG        origin/main
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    harfbuzz
    #GIT_REPOSITORY https://github.com/harfbuzz/harfbuzz.git
    #GIT_TAG        main
    GIT_REPOSITORY https://github.com/tksuoran/harfbuzz.git
    GIT_TAG        origin/cmake-fix
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

#FetchContent_Declare(
#    ImGui
#    GIT_REPOSITORY https://github.com/ocornut/imgui.git
#    GIT_TAG        master # v1.81
#    GIT_SHALLOW    TRUE
#    GIT_PROGRESS   TRUE
#)

# Not yet in use
#FetchContent_Declare(
#    libigl
#    GIT_REPOSITORY https://github.com/libigl/libigl.git
#    GIT_TAG        v2.3.0
#    GIT_SHALLOW    TRUE
#    GIT_PROGRESS   TRUE
#)

FetchContent_Declare(
    JoltPhysics
    GIT_REPOSITORY  https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG         origin/master
    #GIT_REPOSITORY  https://github.com/tksuoran/JoltPhysics.git
    #GIT_TAG         point-constraint-edit
    GIT_SHALLOW     TRUE
    GIT_PROGRESS    TRUE
)

FetchContent_Declare(
    lunasvg
    GIT_REPOSITORY  https://github.com/sammycage/lunasvg.git
    GIT_TAG         origin/master
    GIT_SHALLOW     TRUE
    GIT_PROGRESS    TRUE
)

FetchContent_Declare(
    MathGeoLib
    #GIT_REPOSITORY https://github.com/juj/MathGeoLib.git
    GIT_REPOSITORY  https://github.com/tksuoran/MathGeoLib.git
    GIT_TAG        origin/fetchcontent
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.2
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

# Subset now in src/mango
#
#FetchContent_Declare(
#    mango
#    #GIT_REPOSITORY https://github.com/t0rakka/mango.git
#    GIT_REPOSITORY https://github.com/tksuoran/mango.git
#    #GIT_TAG        master
#    GIT_TAG        origin/subset
#    GIT_SHALLOW    TRUE
#    GIT_PROGRESS   TRUE
#)

FetchContent_Declare(
    OpenXR-SDK
    GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
    GIT_TAG        origin/main
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.10.0
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    tracy
    GIT_REPOSITORY https://github.com/tksuoran/tracy.git
    GIT_TAG        origin/fix-operator-precedence-v0.8.1
    #GIT_REPOSITORY https://github.com/tksuoran/tracy.git
    #GIT_TAG        v0.8.1
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
