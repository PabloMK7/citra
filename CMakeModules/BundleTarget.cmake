
if (BUNDLE_TARGET_EXECUTE)
    # --- Bundling method logic ---

    function(bundle_qt executable_path)
        if (WIN32)
            get_filename_component(executable_parent_dir "${executable_path}" DIRECTORY)
            find_program(windeployqt_executable windeployqt6)

            # Create a qt.conf file pointing to the app directory.
            # This ensures Qt can find its plugins.
            file(WRITE "${executable_parent_dir}/qt.conf" "[Paths]\nprefix = .")

            message(STATUS "Executing windeployqt for executable ${executable_path}")
            execute_process(COMMAND "${windeployqt_executable}" "${executable_path}"
                --no-compiler-runtime --no-system-d3d-compiler --no-opengl-sw --no-translations
                --plugindir "${executable_parent_dir}/plugins")

            # Remove the FFmpeg multimedia plugin as we don't include FFmpeg.
            # We want to use the Windows media plugin instead, which is also included.
            file(REMOVE "${executable_parent_dir}/plugins/multimedia/ffmpegmediaplugin.dll")
        elseif (APPLE)
            get_filename_component(executable_name "${executable_path}" NAME_WE)
            find_program(MACDEPLOYQT_EXECUTABLE macdeployqt6)

            message(STATUS "Executing macdeployqt for executable ${executable_path}")
            execute_process(
                COMMAND "${MACDEPLOYQT_EXECUTABLE}"
                "${executable_path}"
                "-executable=${executable_path}/Contents/MacOS/${executable_name}"
                -always-overwrite)

            # Bundling libraries can rewrite path information and break code signatures of system libraries.
            # Perform an ad-hoc re-signing on the whole app bundle to fix this.
            execute_process(COMMAND codesign --deep -fs - "${executable_path}")
        else()
            message(FATAL_ERROR "Unsupported OS for Qt bundling.")
        endif()
    endfunction()

    function(bundle_appimage bundle_dir executable_path source_path binary_path linuxdeploy_executable enable_qt)
        get_filename_component(executable_name "${executable_path}" NAME_WE)
        set(appdir_path "${binary_path}/AppDir-${executable_name}")

        if (enable_qt)
            # Find qmake to make sure the plugin uses the right version of Qt.
            find_program(QMAKE_EXECUTABLE qmake6)

            set(extra_linuxdeploy_env "QMAKE=${QMAKE_EXECUTABLE}")
            set(extra_linuxdeploy_args --plugin qt)
        endif()

        message(STATUS "Creating AppDir for executable ${executable_path}")
        execute_process(COMMAND ${CMAKE_COMMAND} -E env
            ${extra_linuxdeploy_env}
            "${linuxdeploy_executable}"
            ${extra_linuxdeploy_args}
            --plugin checkrt
            --executable "${executable_path}"
            --icon-file "${source_path}/dist/citra.svg"
            --desktop-file "${source_path}/dist/${executable_name}.desktop"
            --appdir "${appdir_path}")

        if (enable_qt)
            set(qt_hook_file "${appdir_path}/apprun-hooks/linuxdeploy-plugin-qt-hook.sh")
            file(READ "${qt_hook_file}" qt_hook_contents)
            # Add Cinnamon to list of DEs for GTK3 theming.
            string(REPLACE
                "*XFCE*"
                "*X-Cinnamon*|*XFCE*"
                qt_hook_contents "${qt_hook_contents}")
            # Wayland backend crashes due to changed schemas in Gnome 40.
            string(REPLACE
                "export QT_QPA_PLATFORMTHEME=gtk3"
                "export QT_QPA_PLATFORMTHEME=gtk3; export GDK_BACKEND=x11"
                qt_hook_contents "${qt_hook_contents}")
            file(WRITE "${qt_hook_file}" "${qt_hook_contents}")
        endif()

        message(STATUS "Creating AppImage for executable ${executable_path}")
        execute_process(COMMAND ${CMAKE_COMMAND} -E env
            "OUTPUT=${bundle_dir}/${executable_name}.AppImage"
            "${linuxdeploy_executable}"
            --output appimage
            --appdir "${appdir_path}")
    endfunction()

    function(bundle_standalone executable_path original_executable_path bundle_library_paths)
        get_filename_component(executable_parent_dir "${executable_path}" DIRECTORY)

        # Resolve dependent library files if they were not passed in.
        message(STATUS "Determining runtime dependencies of ${executable_path} using library paths ${bundle_library_paths}")
        file(GET_RUNTIME_DEPENDENCIES
                EXECUTABLES ${original_executable_path}
                RESOLVED_DEPENDENCIES_VAR resolved_deps
                UNRESOLVED_DEPENDENCIES_VAR unresolved_deps
                DIRECTORIES ${bundle_library_paths}
                POST_EXCLUDE_REGEXES ".*system32.*")

        if (WIN32)
            # Same directory since we don't have rpath.
            set(lib_dir "${executable_parent_dir}")
        else()
            set(lib_dir "${executable_parent_dir}/libs")
        endif()

        # Copy files to bundled output.
        if (resolved_deps)
            file(MAKE_DIRECTORY ${lib_dir})
            foreach (lib_file IN LISTS resolved_deps)
                message(STATUS "Bundling library ${lib_file}")
                # Use native copy to turn symlinks into normal files.
                execute_process(COMMAND cp -L "${lib_file}" "${lib_dir}")
            endforeach()
        endif()

        # Add libs directory to executable rpath where applicable.
        if (APPLE)
            execute_process(COMMAND install_name_tool -add_rpath "@loader_path/libs" "${executable_path}")
        elseif (UNIX)
            execute_process(COMMAND patchelf --set-rpath '$ORIGIN/../libs' "${executable_path}")
        endif()
    endfunction()

    # --- Root bundling logic ---

    set(bundle_dir ${BINARY_PATH}/bundle)

    # On Linux, always bundle an AppImage.
    if (DEFINED LINUXDEPLOY)
        if (IN_PLACE)
            message(FATAL_ERROR "Cannot bundle for Linux in-place.")
        endif()

        bundle_appimage("${bundle_dir}" "${EXECUTABLE_PATH}" "${SOURCE_PATH}" "${BINARY_PATH}" "${LINUXDEPLOY}" ${BUNDLE_QT})
    else()
        if (IN_PLACE)
            message(STATUS "Bundling dependencies in-place")
            set(bundled_executable_path "${EXECUTABLE_PATH}")
        else()
            message(STATUS "Copying base executable ${EXECUTABLE_PATH} to output directory ${bundle_dir}")
            file(COPY ${EXECUTABLE_PATH} DESTINATION ${bundle_dir})
            get_filename_component(bundled_executable_name "${EXECUTABLE_PATH}" NAME)
            set(bundled_executable_path "${bundle_dir}/${bundled_executable_name}")
        endif()

        if (BUNDLE_QT)
            bundle_qt("${bundled_executable_path}")
        endif()

        if (WIN32 OR NOT BUNDLE_QT)
            bundle_standalone("${bundled_executable_path}" "${EXECUTABLE_PATH}" "${BUNDLE_LIBRARY_PATHS}")
        endif()
    endif()
else()
    # --- Bundling target creation logic ---

    # Downloads and extracts a linuxdeploy component.
    function(download_linuxdeploy_component base_dir name executable_name)
        set(executable_file "${base_dir}/${executable_name}")
        if (NOT EXISTS "${executable_file}")
            message(STATUS "Downloading ${executable_name}")
            file(DOWNLOAD
                "https://github.com/linuxdeploy/${name}/releases/download/continuous/${executable_name}"
                "${executable_file}" SHOW_PROGRESS)
            file(CHMOD "${executable_file}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

            get_filename_component(executable_ext "${executable_file}" LAST_EXT)
            if (executable_ext STREQUAL ".AppImage")
                message(STATUS "Extracting ${executable_name}")
                execute_process(
                    COMMAND "${executable_file}" --appimage-extract
                    WORKING_DIRECTORY "${base_dir}")
            else()
                message(STATUS "Copying ${executable_name}")
                file(COPY "${executable_file}" DESTINATION "${base_dir}/squashfs-root/usr/bin/")
            endif()
        endif()
    endfunction()

    # Adds a target to the bundle target, packing in required libraries.
    # If in_place is true, the bundling will be done in-place as part of the specified target.
    function(bundle_target_internal target_name in_place)
        # Create base bundle target if it does not exist.
        if (NOT in_place AND NOT TARGET bundle)
            message(STATUS "Creating base bundle target")

            add_custom_target(bundle)
            add_custom_command(
                TARGET bundle
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bundle/")
            add_custom_command(
                TARGET bundle
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bundle/dist/")
            add_custom_command(
                TARGET bundle
                COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/dist/icon.png" "${CMAKE_BINARY_DIR}/bundle/dist/citra.png")
            add_custom_command(
                TARGET bundle
                COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/license.txt" "${CMAKE_BINARY_DIR}/bundle/")
            add_custom_command(
                TARGET bundle
                COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/README.md" "${CMAKE_BINARY_DIR}/bundle/")
            add_custom_command(
                TARGET bundle
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/dist/scripting" "${CMAKE_BINARY_DIR}/bundle/scripting")
        endif()

        set(BUNDLE_EXECUTABLE_PATH "$<TARGET_FILE:${target_name}>")
        if (target_name MATCHES ".*qt")
            set(BUNDLE_QT ON)
            if (APPLE)
                # For Qt targets on Apple, expect an app bundle.
                set(BUNDLE_EXECUTABLE_PATH "$<TARGET_BUNDLE_DIR:${target_name}>")
            endif()
        else()
            set(BUNDLE_QT OFF)
        endif()

        # Build a list of library search paths from prefix paths.
        foreach(prefix_path IN LISTS CMAKE_PREFIX_PATH CMAKE_SYSTEM_PREFIX_PATH)
            if (WIN32)
                list(APPEND BUNDLE_LIBRARY_PATHS "${prefix_path}/bin")
            endif()
            list(APPEND BUNDLE_LIBRARY_PATHS "${prefix_path}/lib")
        endforeach()
        foreach(library_path IN LISTS CMAKE_SYSTEM_LIBRARY_PATH)
            list(APPEND BUNDLE_LIBRARY_PATHS "${library_path}")
        endforeach()

        # On Linux, prepare linuxdeploy and any required plugins.
        if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
            set(LINUXDEPLOY_BASE "${CMAKE_BINARY_DIR}/externals/linuxdeploy")

            # Download plugins first so they don't overwrite linuxdeploy's AppRun file.
            download_linuxdeploy_component("${LINUXDEPLOY_BASE}" "linuxdeploy-plugin-qt" "linuxdeploy-plugin-qt-x86_64.AppImage")
            download_linuxdeploy_component("${LINUXDEPLOY_BASE}" "linuxdeploy-plugin-checkrt" "linuxdeploy-plugin-checkrt-x86_64.sh")
            download_linuxdeploy_component("${LINUXDEPLOY_BASE}" "linuxdeploy" "linuxdeploy-x86_64.AppImage")

            set(EXTRA_BUNDLE_ARGS "-DLINUXDEPLOY=${LINUXDEPLOY_BASE}/squashfs-root/AppRun")
        endif()

        if (in_place)
            message(STATUS "Adding in-place bundling to ${target_name}")
            set(DEST_TARGET ${target_name})
        else()
            message(STATUS "Adding ${target_name} to bundle target")
            set(DEST_TARGET bundle)
            add_dependencies(bundle ${target_name})
        endif()

        add_custom_command(TARGET ${DEST_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND}
            "-DCMAKE_PREFIX_PATH=\"${CMAKE_PREFIX_PATH}\""
            "-DBUNDLE_TARGET_EXECUTE=1"
            "-DTARGET=${target_name}"
            "-DSOURCE_PATH=${CMAKE_SOURCE_DIR}"
            "-DBINARY_PATH=${CMAKE_BINARY_DIR}"
            "-DEXECUTABLE_PATH=${BUNDLE_EXECUTABLE_PATH}"
            "-DBUNDLE_LIBRARY_PATHS=\"${BUNDLE_LIBRARY_PATHS}\""
            "-DBUNDLE_QT=${BUNDLE_QT}"
            "-DIN_PLACE=${in_place}"
            ${EXTRA_BUNDLE_ARGS}
            -P "${CMAKE_SOURCE_DIR}/CMakeModules/BundleTarget.cmake"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
    endfunction()

    # Adds a target to the bundle target, packing in required libraries.
    function(bundle_target target_name)
        bundle_target_internal("${target_name}" OFF)
    endfunction()

    # Bundles the target in-place, packing in required libraries.
    function(bundle_target_in_place target_name)
        bundle_target_internal("${target_name}" ON)
    endfunction()
endif()
