function(copy_citra_Qt6_deps target_dir)
    include(WindowsCopyFiles)
    set(DLL_DEST "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/")
    set(Qt6_DLL_DIR "${Qt6_DIR}/../../../bin")
    set(Qt6_PLATFORMS_DIR "${Qt6_DIR}/../../../plugins/platforms/")
    set(Qt6_MULTIMEDIA_DIR "${Qt6_DIR}/../../../plugins/multimedia/")
    set(Qt6_STYLES_DIR "${Qt6_DIR}/../../../plugins/styles/")
    set(Qt6_IMAGEFORMATS_DIR "${Qt6_DIR}/../../../plugins/imageformats/")
    set(PLATFORMS ${DLL_DEST}plugins/platforms/)
    set(MULTIMEDIA ${DLL_DEST}plugins/multimedia/)
    set(STYLES ${DLL_DEST}plugins/styles/)
    set(IMAGEFORMATS ${DLL_DEST}plugins/imageformats/)
    windows_copy_files(${target_dir} ${Qt6_DLL_DIR} ${DLL_DEST}
        icudt*.dll
        icuin*.dll
        icuuc*.dll
        Qt6Core$<$<CONFIG:Debug>:d>.*
        Qt6Gui$<$<CONFIG:Debug>:d>.*
        Qt6Widgets$<$<CONFIG:Debug>:d>.*
        Qt6Concurrent$<$<CONFIG:Debug>:d>.*
        Qt6Multimedia$<$<CONFIG:Debug>:d>.*
        Qt6Network$<$<CONFIG:Debug>:d>.*
    )
    windows_copy_files(citra-qt ${Qt6_PLATFORMS_DIR} ${PLATFORMS} qwindows$<$<CONFIG:Debug>:d>.*)
    windows_copy_files(citra-qt ${Qt6_MULTIMEDIA_DIR} ${MULTIMEDIA}
        windowsmediaplugin$<$<CONFIG:Debug>:d>.*
    )
    windows_copy_files(citra-qt ${Qt6_STYLES_DIR} ${STYLES} qwindowsvistastyle$<$<CONFIG:Debug>:d>.*)
    windows_copy_files(${target_dir} ${Qt6_IMAGEFORMATS_DIR} ${IMAGEFORMATS}
        qgif$<$<CONFIG:Debug>:d>.dll
        qicns$<$<CONFIG:Debug>:d>.dll
        qico$<$<CONFIG:Debug>:d>.dll
        qjpeg$<$<CONFIG:Debug>:d>.dll
        qsvg$<$<CONFIG:Debug>:d>.dll
        qtga$<$<CONFIG:Debug>:d>.dll
        qtiff$<$<CONFIG:Debug>:d>.dll
        qwbmp$<$<CONFIG:Debug>:d>.dll
        qwebp$<$<CONFIG:Debug>:d>.dll
    )

    # Create an empty qt.conf file. Qt will detect that this file exists, and use the folder that its in as the root folder.
    # This way it'll look for plugins in the root/plugins/ folder
    add_custom_command(TARGET citra-qt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E touch ${DLL_DEST}qt.conf
    )
endfunction(copy_citra_Qt6_deps)
