# To use this as a script, make sure you pass in the variables SRC_DIR BUILD_DIR and TARGET_FILE

if(WIN32)
    set(PLATFORM "windows")
elseif(APPLE)
    set(PLATFORM "mac")
elseif(UNIX)
    set(PLATFORM "linux")
else()
    message(FATAL_ERROR "Cannot build installer for this unsupported platform")
endif()

set(DIST_DIR "${BUILD_DIR}/dist")
set(ARCHIVE "${PLATFORM}.7z")

file(MAKE_DIRECTORY ${BUILD_DIR})
file(MAKE_DIRECTORY ${DIST_DIR})
file(DOWNLOAD https://github.com/citra-emu/ext-windows-bin/raw/master/qtifw/${ARCHIVE}
    "${BUILD_DIR}/${ARCHIVE}" SHOW_PROGRESS)
execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf "${BUILD_DIR}/${ARCHIVE}"
    WORKING_DIRECTORY "${BUILD_DIR}/")

set(TARGET_NAME "citra-setup-${PLATFORM}")
set(CONFIG_FILE "${SRC_DIR}/config/config_${PLATFORM}.xml")
set(INSTALLER_BASE "${BUILD_DIR}/installerbase_${PLATFORM}")
set(BINARY_CREATOR "${BUILD_DIR}/binarycreator_${PLATFORM}")
set(PACKAGES_DIR "${BUILD_DIR}/packages")
file(MAKE_DIRECTORY ${PACKAGES_DIR})

if (UNIX OR APPLE)
    execute_process(COMMAND chmod 744 ${BINARY_CREATOR})
endif()

execute_process(COMMAND ${BINARY_CREATOR} -t ${INSTALLER_BASE} -n -c ${CONFIG_FILE} -p ${PACKAGES_DIR} ${TARGET_FILE})

if (APPLE)
    execute_process(COMMAND chmod 744 ${TARGET_FILE}.app/Contents/MacOS/${TARGET_NAME})
endif()
