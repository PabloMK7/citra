set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)

add_executable(citra-room
    precompiled_headers.h
    citra-room.cpp
    citra-room.rc
)

create_target_directory_groups(citra-room)

target_link_libraries(citra-room PRIVATE citra_common network)
if (ENABLE_WEB_SERVICE)
    target_link_libraries(citra-room PRIVATE web_service)
endif()

target_link_libraries(citra-room PRIVATE cryptopp)
if (MSVC)
    target_link_libraries(citra-room PRIVATE getopt)
endif()
target_link_libraries(citra-room PRIVATE ${PLATFORM_LIBRARIES} Threads::Threads)

if(UNIX AND NOT APPLE)
    install(TARGETS citra-room RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()

if (CITRA_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(citra-room PRIVATE precompiled_headers.h)
endif()

# Bundle in-place on MSVC so dependencies can be resolved by builds.
if (MSVC)
    include(BundleTarget)
    bundle_target_in_place(citra-room)
endif()
