# Bundles libraries with an output executable.
# Parameters:
# - TYPE: "qt" or "standalone". The type of app to bundle.
#   - "qt" uses windeployqt/macdeployqt to bundle Qt and other libraries.
#   - "standalone" copies dependent libraries to a "libs" folder alongside the executable file.
# - EXECUTABLE_PATH: Path to the executable binary.

# TODO: This does not really work fully for Windows yet, some libraries are missing from the output.
# TODO: Leaving a basic framework of Windows support here to be iterated on in the future.
if (WIN32)
    message(FATAL_ERROR "Advanced library bundling is not yet supported for Windows.")
endif()

if ("${TYPE}" STREQUAL "qt")
    get_filename_component(executable_dir ${EXECUTABLE_PATH} DIRECTORY)

    # Bundle dependencies using appropriate Qt tool.
    if (WIN32)
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt)
        execute_process(COMMAND ${WINDEPLOYQT_EXECUTABLE} ${EXECUTABLE_PATH}
            --no-compiler-runtime --no-system-d3d-compiler --no-opengl-sw --no-translations
            --plugindir "${executable_dir}/plugins")
    elseif (APPLE)
        get_filename_component(contents_dir ${executable_dir} DIRECTORY)
        get_filename_component(bundle_dir ${contents_dir} DIRECTORY)

        find_program(MACDEPLOYQT_EXECUTABLE macdeployqt)
        execute_process(COMMAND ${MACDEPLOYQT_EXECUTABLE} ${bundle_dir} -executable=${EXECUTABLE_PATH} -always-overwrite)
    else()
        message(FATAL_ERROR "Unsupported OS for Qt-based library bundling.")
    endif()
else()
    # Resolve dependent library files.
    file(GET_RUNTIME_DEPENDENCIES
        EXECUTABLES ${EXECUTABLE_PATH}
        RESOLVED_DEPENDENCIES_VAR resolved_deps
        UNRESOLVED_DEPENDENCIES_VAR unresolved_deps
        POST_EXCLUDE_REGEXES ".*system32/.*\\.dll")

    # Determine libraries directory.
    get_filename_component(executable_dir ${EXECUTABLE_PATH} DIRECTORY)
    if (WIN32)
        # Same directory since we don't have rpath.
        set(lib_dir ${executable_dir})
    else()
        set(lib_dir ${executable_dir}/libs)
    endif()

    # Copy library files to bundled output.
    file(MAKE_DIRECTORY ${lib_dir})
    foreach (lib_file ${resolved_deps})
        # Use native copy to turn symlinks into normal files.
        execute_process(COMMAND cp -L ${lib_file} ${lib_dir})
    endforeach()

    # Add libs directory to executable rpath where applicable.
    if (APPLE)
        execute_process(COMMAND install_name_tool -add_rpath "@loader_path/libs" ${EXECUTABLE_PATH})
    elseif (UNIX)
        execute_process(COMMAND patchelf --set-rpath '$ORIGIN/../libs' ${EXECUTABLE_PATH})
    endif()
endif()
