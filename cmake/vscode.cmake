function (vscode_support)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    option(FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." TRUE)
    if (${FORCE_COLORED_OUTPUT})
        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
            add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fdiagnostics-color=always>)
        elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
            add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fcolor-diagnostics>)
        endif ()
    endif ()
endfunction ()
