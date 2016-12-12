function(copy_citra_Qt5_deps target_dir)
    include(WindowsCopyFiles)
    set(DLL_DEST "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/")
    set(Qt5_DLL_DIR "${Qt5_DIR}/../../../bin")
    set(Qt5_PLATFORMS_DIR "${Qt5_DIR}/../../../plugins/platforms/")
    set(PLATFORMS ${DLL_DEST}platforms/)
    windows_copy_files(${target_dir} ${Qt5_DLL_DIR} ${DLL_DEST}
        icudt*.dll
        icuin*.dll
        icuuc*.dll
        Qt5Core$<$<CONFIG:Debug>:d>.*
        Qt5Gui$<$<CONFIG:Debug>:d>.*
        Qt5OpenGL$<$<CONFIG:Debug>:d>.*
        Qt5Widgets$<$<CONFIG:Debug>:d>.*
    )
    windows_copy_files(citra-qt ${Qt5_PLATFORMS_DIR} ${PLATFORMS} qwindows$<$<CONFIG:Debug>:d>.*)
endfunction(copy_citra_Qt5_deps)
