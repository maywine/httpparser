function(UnitTest test_file depends)
    get_filename_component(target_name ${test_file} NAME_WE)
    add_executable(${target_name} ${test_file})

    # google test
	target_include_directories(${target_name} PRIVATE ${PROJECT_SOURCE_DIR}/include)
    foreach(dep ${depends})
        target_link_libraries(${target_name} ${dep})
    endforeach()

    add_test(${target_name} ${target_name})
endfunction(UnitTest)

UnitTest(keepalive.cpp "${Boost_LIBRARIES}")
UnitTest(post.cpp "${Boost_LIBRARIES}")
UnitTest(response.cpp "${Boost_LIBRARIES}")
UnitTest(request.cpp "${Boost_LIBRARIES}")
UnitTest(simple.cpp "${Boost_LIBRARIES}")
