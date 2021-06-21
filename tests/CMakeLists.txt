function(UnitTest test_file depends)
    get_filename_component(target_name ${test_file} NAME_WE)
    add_executable(${target_name} ${test_file})

	target_include_directories(${target_name} PRIVATE ${PROJECT_SOURCE_DIR}/include)
    foreach(dep ${depends})
        target_link_libraries(${target_name} ${dep})
    endforeach()

    add_test(${target_name} ${target_name})
endfunction(UnitTest)

UnitTest(keepalive_test.cpp "${Boost_LIBRARIES}")
UnitTest(post_test.cpp "${Boost_LIBRARIES}")
UnitTest(response_test.cpp "${Boost_LIBRARIES}")
UnitTest(request_test.cpp "${Boost_LIBRARIES}")
UnitTest(urlparser_test.cpp "${Boost_LIBRARIES}")
