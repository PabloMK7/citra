function(copy_citra_FFmpeg_deps target_dir)
    include(WindowsCopyFiles)
    set(DLL_DEST "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/")
    windows_copy_files(${target_dir} ${FFMPEG_DIR}/bin ${DLL_DEST}
        avcodec*.dll
        avformat*.dll
        avutil*.dll
        swresample*.dll
        swscale*.dll
    )
endfunction(copy_citra_FFmpeg_deps)
