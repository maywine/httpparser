function(BuildExample example_file)
    get_filename_component(target_name ${example_file} NAME_WE)
    add_executable(${target_name} ${example_file})
	target_include_directories(${target_name} PRIVATE ${PROJECT_SOURCE_DIR}/include)
    target_compile_options(${target_name}
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror)
endfunction(BuildExample)

BuildExample(request_example.cpp)
BuildExample(response_example.cpp)
BuildExample(urlparser_example.cpp)