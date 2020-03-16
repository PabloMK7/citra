function(GetShaderHeaderFile shader_file_name)
    set(shader_header_file ${CMAKE_CURRENT_BINARY_DIR}/shaders/${shader_file_name} PARENT_SCOPE)
endfunction()

foreach(shader_file ${SHADER_FILES})
    file(READ ${shader_file} shader)
    get_filename_component(shader_file_name ${shader_file} NAME)
    string(REPLACE . _ shader_name ${shader_file_name})
    GetShaderHeaderFile(${shader_file_name})
    file(WRITE ${shader_header_file}
        "#pragma once\n"
        "constexpr std::string_view ${shader_name} = R\"(\n"
        "${shader}"
        ")\";\n"
    )
endforeach()
