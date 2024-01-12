
set(CURRENT_MODULE_DIR ${CMAKE_CURRENT_LIST_DIR})

# This function downloads Qt using aqt. The path of the downloaded content will be added to the CMAKE_PREFIX_PATH.
# Params:
#   target: Qt dependency to install. Specify a version number to download Qt, or "tools_(name)" for a specific build tool.
function(download_qt target)
    if (target MATCHES "tools_.*")
        set(DOWNLOAD_QT_TOOL ON)
    else()
        set(DOWNLOAD_QT_TOOL OFF)
    endif()

    # Determine installation parameters for OS, architecture, and compiler
    if (WIN32)
        set(host "windows")
        set(type "desktop")
        if (NOT DOWNLOAD_QT_TOOL)
            if (MINGW)
                set(arch "win64_mingw")
                set(arch_path "mingw_64")
            elseif (MSVC)
                if ("arm64" IN_LIST ARCHITECTURE)
                    set(arch_path "msvc2019_arm64")
                elseif ("x86_64" IN_LIST ARCHITECTURE)
                    set(arch_path "msvc2019_64")
                else()
                    message(FATAL_ERROR "Unsupported bundled Qt architecture. Enable USE_SYSTEM_QT and provide your own.")
                endif()
                set(arch "win64_${arch_path}")
            else()
                message(FATAL_ERROR "Unsupported bundled Qt toolchain. Enable USE_SYSTEM_QT and provide your own.")
            endif()
        endif()
    elseif (APPLE)
        set(host "mac")
        if (IOS AND NOT DOWNLOAD_QT_TOOL)
            set(type "ios")
            set(arch "ios")
            set(arch_path "ios")
            set(host_arch_path "macos")
        else()
            set(type "desktop")
            set(arch "clang_64")
            set(arch_path "macos")
        endif()
    else()
        set(host "linux")
        set(type "desktop")
        set(arch "gcc_64")
        set(arch_path "linux")
    endif()

    get_external_prefix(qt base_path)
    file(MAKE_DIRECTORY "${base_path}")

    set(install_args -c "${CURRENT_MODULE_DIR}/aqt_config.ini")
    if (DOWNLOAD_QT_TOOL)
        set(prefix "${base_path}/Tools")
        set(install_args ${install_args} install-tool --outputdir ${base_path} ${host} desktop ${target})
    else()
        set(prefix "${base_path}/${target}/${arch_path}")
        if (host_arch_path)
            set(host_flag "--autodesktop")
            set(host_prefix "${base_path}/${target}/${host_arch_path}")
        endif()
        set(install_args ${install_args} install-qt --outputdir ${base_path} ${host} ${type} ${target} ${arch} ${host_flag}
                                    -m qtmultimedia --archives qttranslations qttools qtsvg qtbase)
    endif()

    if (NOT EXISTS "${prefix}")
        message(STATUS "Downloading binaries for Qt...")
        set(AQT_PREBUILD_BASE_URL "https://github.com/miurahr/aqtinstall/releases/download/v3.1.9")
        if (WIN32)
            set(aqt_path "${base_path}/aqt.exe")
            file(DOWNLOAD
                ${AQT_PREBUILD_BASE_URL}/aqt.exe
                ${aqt_path} SHOW_PROGRESS)
            execute_process(COMMAND ${aqt_path} ${install_args}
                    WORKING_DIRECTORY ${base_path})
        elseif (APPLE)
            set(aqt_path "${base_path}/aqt-macos")
            file(DOWNLOAD
                ${AQT_PREBUILD_BASE_URL}/aqt-macos
                ${aqt_path} SHOW_PROGRESS)
            execute_process(COMMAND chmod +x ${aqt_path})
            execute_process(COMMAND ${aqt_path} ${install_args}
                    WORKING_DIRECTORY ${base_path})
        else()
            # aqt does not offer binary releases for other platforms, so download and run from pip.
            set(aqt_install_path "${base_path}/aqt")
            file(MAKE_DIRECTORY "${aqt_install_path}")

            execute_process(COMMAND python3 -m pip install --target=${aqt_install_path} aqtinstall
                    WORKING_DIRECTORY ${base_path})
            execute_process(COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${aqt_install_path} python3 -m aqt ${install_args}
                    WORKING_DIRECTORY ${base_path})
        endif()
    endif()

    message(STATUS "Using downloaded Qt binaries at ${prefix}")

    # Add the Qt prefix path so CMake can locate it.
    list(APPEND CMAKE_PREFIX_PATH "${prefix}")
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} PARENT_SCOPE)

    if (DEFINED host_prefix)
        message(STATUS "Using downloaded host Qt binaries at ${host_prefix}")
        set(QT_HOST_PATH "${host_prefix}" CACHE STRING "")
    endif()
endfunction()

function(download_moltenvk)
    if (IOS)
        set(MOLTENVK_PLATFORM "iOS")
    else()
        set(MOLTENVK_PLATFORM "macOS")
    endif()

    set(MOLTENVK_DIR "${CMAKE_BINARY_DIR}/externals/MoltenVK")
    set(MOLTENVK_TAR "${CMAKE_BINARY_DIR}/externals/MoltenVK.tar")
    if (NOT EXISTS ${MOLTENVK_DIR})
        if (NOT EXISTS ${MOLTENVK_TAR})
            file(DOWNLOAD https://github.com/KhronosGroup/MoltenVK/releases/download/v1.2.7-rc2/MoltenVK-all.tar
                ${MOLTENVK_TAR} SHOW_PROGRESS)
        endif()

        execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf "${MOLTENVK_TAR}"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/externals")
    endif()

    # Add the MoltenVK library path to the prefix so find_library can locate it.
    list(APPEND CMAKE_PREFIX_PATH "${MOLTENVK_DIR}/MoltenVK/dylib/${MOLTENVK_PLATFORM}")
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} PARENT_SCOPE)
endfunction()

function(get_external_prefix lib_name prefix_var)
    set(${prefix_var} "${CMAKE_BINARY_DIR}/externals/${lib_name}" PARENT_SCOPE)
endfunction()
