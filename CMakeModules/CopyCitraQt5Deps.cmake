function(copy_citra_Qt5_deps target_dir)
    include(WindowsCopyFiles)
    set(DLL_DEST "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/")
    set(Qt5_DLL_DIR "${Qt5_DIR}/../../../bin")
    set(Qt5_PLATFORMS_DIR "${Qt5_DIR}/../../../plugins/platforms/")
    set(Qt5_MEDIASERVICE_DIR "${Qt5_DIR}/../../../plugins/mediaservice/")
    set(Qt5_STYLES_DIR "${Qt5_DIR}/../../../plugins/styles/")
    set(Qt5_IMAGEFORMATS_DIR "${Qt5_DIR}/../../../plugins/imageformats/")
    set(PLATFORMS ${DLL_DEST}plugins/platforms/)
    set(MEDIASERVICE ${DLL_DEST}plugins/mediaservice/)
    set(STYLES ${DLL_DEST}plugins/styles/)
    set(IMAGEFORMATS ${DLL_DEST}plugins/imageformats/)
    windows_copy_files(${target_dir} ${Qt5_DLL_DIR} ${DLL_DEST}
        icudt*.dll
        icuin*.dll
        icuuc*.dll
        Qt5Core$<$<CONFIG:Debug>:d>.*
        Qt5Gui$<$<CONFIG:Debug>:d>.*
        Qt5OpenGL$<$<CONFIG:Debug>:d>.*
        Qt5Widgets$<$<CONFIG:Debug>:d>.*
        Qt5Multimedia$<$<CONFIG:Debug>:d>.*
        Qt5Network$<$<CONFIG:Debug>:d>.*
    )
    windows_copy_files(citra-qt ${Qt5_PLATFORMS_DIR} ${PLATFORMS} qwindows$<$<CONFIG:Debug>:d>.*)
    windows_copy_files(citra-qt ${Qt5_MEDIASERVICE_DIR} ${MEDIASERVICE}
        dsengine$<$<CONFIG:Debug>:d>.*
        wmfengine$<$<CONFIG:Debug>:d>.*
    )
    windows_copy_files(citra-qt ${Qt5_STYLES_DIR} ${STYLES} qwindowsvistastyle$<$<CONFIG:Debug>:d>.*)
    windows_copy_files(${target_dir} ${Qt5_IMAGEFORMATS_DIR} ${IMAGEFORMATS}
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
endfunction(copy_citra_Qt5_deps)
