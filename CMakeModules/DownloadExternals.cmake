
# This function downloads a binary library package from our external repo.
# Params:
#   remote_path: path to the file to download, relative to the remote repository root
#   prefix_var: name of a variable which will be set with the path to the extracted contents
function(download_bundled_external remote_path lib_name prefix_var)
    get_external_prefix(${lib_name} prefix)
    if (NOT EXISTS "${prefix}")
        message(STATUS "Downloading binaries for ${lib_name}...")

        if (WIN32)
            set(repo_base "ext-windows-bin/raw/master")
        elseif (APPLE)
            set(repo_base "ext-macos-bin/raw/main")
        else()
            message(FATAL_ERROR "Bundled libraries are unsupported for this OS.")
        endif()

        file(DOWNLOAD
            https://github.com/citra-emu/${repo_base}/${remote_path}${lib_name}.7z
            "${CMAKE_BINARY_DIR}/externals/${lib_name}.7z" SHOW_PROGRESS)
        execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf "${CMAKE_BINARY_DIR}/externals/${lib_name}.7z"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/externals")
    endif()

    # For packages that include the /usr/local prefix, include it in the prefix path.
    if (EXISTS "${prefix}/usr/local")
        set(prefix "${prefix}/usr/local")
    endif()

    message(STATUS "Using bundled binaries at ${prefix}")
    set(${prefix_var} "${prefix}" PARENT_SCOPE)
endfunction()

# This function downloads Qt using aqt.
# Params:
#   target: Qt dependency to install. Specify a version number to download Qt, or "tools_(name)" for a specific build tool.
#   prefix_var: Name of a variable which will be set with the path to the extracted contents.
function(download_qt_external target)
    # Determine installation parameters for OS, architecture, and compiler
    if (WIN32)
        set(host "windows")
        set(type "desktop")
        if (MINGW)
            set(arch_path "mingw81")
        elseif ((MSVC_VERSION GREATER_EQUAL 1920 AND MSVC_VERSION LESS 1940) AND "x86_64" IN_LIST ARCHITECTURE)
            if ("arm64" IN_LIST ARCHITECTURE)
                set(arch_path "msvc2019_arm64")
            elseif ("x86_64" IN_LIST ARCHITECTURE)
                set(arch_path "msvc2019_64")
            else()
                message(FATAL_ERROR "Unsupported bundled Qt architecture. Disable CITRA_USE_BUNDLED_QT and provide your own.")
            endif()
        else()
            message(FATAL_ERROR "Unsupported bundled Qt toolchain. Disable CITRA_USE_BUNDLED_QT and provide your own.")
        endif()
        set(arch "win64_${arch_path}")
    elseif (APPLE)
        set(host "mac")
        if (IOS)
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

    if (target MATCHES "tools_.*")
        set(prefix "${base_path}")
        set(install_args install-tool --outputdir ${base_path} ${host} desktop ${target})
    else()
        set(prefix "${base_path}/${target}/${arch_path}")
        if (host_arch_path)
            set(host_flag "--autodesktop")
            set(host_prefix "${base_path}/${target}/${host_arch_path}")
        endif()
        set(install_args install-qt --outputdir ${base_path} ${host} ${type} ${target} ${arch} ${host_flag} -m qtmultimedia)
    endif()

    if (NOT EXISTS "${prefix}")
        message(STATUS "Downloading binaries for Qt...")
        if (WIN32)
            set(aqt_path "${base_path}/aqt.exe")
            file(DOWNLOAD
                https://github.com/miurahr/aqtinstall/releases/download/v3.1.4/aqt.exe
                ${aqt_path} SHOW_PROGRESS)
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
            file(DOWNLOAD https://github.com/KhronosGroup/MoltenVK/releases/latest/download/MoltenVK-all.tar
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
