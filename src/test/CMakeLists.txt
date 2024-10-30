cmake_minimum_required(VERSION 3.4.1)
project(mdp_demo LANGUAGES C CXX VERSION 1.0.0.0)

#set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -fexceptions -frtti -D__STDC_FORMAT_MACROS")
if(PLATFORM MATCHES ".*OS" OR PLATFORM MATCHES ".*MAC" OR PLATFORM MATCHES ".*SIMULATOR") 
    include(${CMAKE_CURRENT_LIST_DIR}/../CMakeLists.txt)
    INSTALL(FILES ${libffkit_include_files} DESTINATION ${TMFPREFIX}/include) 
endif()


function(executable_func ename )
    set(executable_name ename)
    message("------wyq-------")
    if(${CMAKE_BUILD_TYPE} STREQUAL "Debug" )
        set(executable_name ${ename}_g)
    endif()
    add_executable(${executable_name} ${ARGN})
    target_link_libraries(${executable_name} PRIVATE mdp bz2 z lzma iconv "-framework CoreFoundation -framework AudioToolbox -framework VideoToolbox -framework CoreMedia -framework CoreVideo -framework Security")
endfunction()

#这里只对MAC或者Linux生成可执行文件，后续可以考虑iOS生成测试工程
if(PLATFORM MATCHES ".*MAC")
    executable_func("mdp_mov_parser_test" ${CMAKE_CURRENT_LIST_DIR}/mdp_mov_parser_test.c)
    # executable_func("demuxing_decoding" ${CMAKE_CURRENT_LIST_DIR}/examples/demuxing_decoding.c)
endif()
