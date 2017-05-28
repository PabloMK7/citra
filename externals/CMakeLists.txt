# Catch
add_library(catch-single-include INTERFACE)
target_include_directories(catch-single-include INTERFACE catch/single_include)

# Xbyak
if (ARCHITECTURE_x86_64)
    add_library(xbyak INTERFACE)
    target_include_directories(xbyak INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/xbyak/xbyak)
    if (NOT MSVC)
        target_compile_options(xbyak INTERFACE -fno-operator-names)
    endif()
endif()

add_subdirectory(cryptopp)

add_subdirectory(fmt)
