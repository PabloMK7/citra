SET(MINGW_PREFIX   /usr/x86_64-w64-mingw32/)
SET(CMAKE_SYSTEM_NAME               Windows)
SET(CMAKE_SYSTEM_PROCESSOR           x86_64)


SET(CMAKE_FIND_ROOT_PATH            ${MINGW_PREFIX})
SET(SDL2_PATH                       ${MINGW_PREFIX})
SET(MINGW_TOOL_PREFIX               ${CMAKE_SYSTEM_PROCESSOR}-w64-mingw32-)

# Specify the cross compiler
SET(CMAKE_C_COMPILER            ${MINGW_TOOL_PREFIX}gcc)
SET(CMAKE_CXX_COMPILER          ${MINGW_TOOL_PREFIX}g++)
SET(CMAKE_RC_COMPILER           ${MINGW_TOOL_PREFIX}windres)

# Mingw tools
SET(STRIP                       ${MINGW_TOOL_PREFIX}strip)
SET(WINDRES                     ${MINGW_TOOL_PREFIX}windres)
SET(ENV{PKG_CONFIG}             ${MINGW_TOOL_PREFIX}pkg-config)

# ccache wrapper
OPTION(CITRA_USE_CCACHE "Use ccache for compilation" OFF)
IF(CITRA_USE_CCACHE)
    FIND_PROGRAM(CCACHE ccache)
    IF (CCACHE)
        MESSAGE(STATUS "Using ccache found in PATH")
        SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE})
        SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE})
    ELSE(CCACHE)
        MESSAGE(WARNING "CITRA_USE_CCACHE enabled, but no ccache found")
    ENDIF(CCACHE)
ENDIF(CITRA_USE_CCACHE)

# Search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)


# Echo modified cmake vars to screen for debugging purposes
IF(NOT DEFINED ENV{MINGW_DEBUG_INFO})
	MESSAGE("")
	MESSAGE("Custom cmake vars: (blank = system default)")
	MESSAGE("-----------------------------------------")
	MESSAGE("* CMAKE_C_COMPILER                     : ${CMAKE_C_COMPILER}")
	MESSAGE("* CMAKE_CXX_COMPILER                   : ${CMAKE_CXX_COMPILER}")
	MESSAGE("* CMAKE_RC_COMPILER                    : ${CMAKE_RC_COMPILER}")
	MESSAGE("* WINDRES                              : ${WINDRES}")
	MESSAGE("* ENV{PKG_CONFIG}                      : $ENV{PKG_CONFIG}")
	MESSAGE("* STRIP                                : ${STRIP}")
	MESSAGE("* CITRA_USE_CCACHE                     : ${CITRA_USE_CCACHE}")
	MESSAGE("")
	# So that the debug info only appears once
	SET(ENV{MINGW_DEBUG_INFO} SHOWN)
ENDIF()
