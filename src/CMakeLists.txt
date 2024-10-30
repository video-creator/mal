# The Flutter tooling requires that developers have CMake 3.10 or later
# installed. You should not increase this version, as doing so will cause
# the plugin to fail to compile for some customers of the plugin.
cmake_minimum_required(VERSION 3.10)

project(mdp_library VERSION 0.0.1 LANGUAGES C)

########FFmpeg######################
if(PLATFORM MATCHES "MAC")
# set(ffmpeg_dir ${CMAKE_CURRENT_LIST_DIR}/third_party/mac/ffmpeg)
# set(ffmpeg_lib_dir ${ffmpeg_dir}/lib)
# endif()
# set_target_properties(FFmpeg
#                         PROPERTIES IMPORTED_LOCATION ${ffmpeg_lib_dir}/libavdevice.a)
# set_property(TARGET FFmpeg APPEND PROPERTY
#                         INTERFACE_LINK_LIBRARIES ${ffmpeg_lib_dir}/libavcodec.a
#                       )
# set_property(TARGET FFmpeg APPEND PROPERTY
#                         INTERFACE_LINK_LIBRARIES ${ffmpeg_lib_dir}/libavutil.a
#                       )
# set_property(TARGET FFmpeg APPEND PROPERTY
#                       INTERFACE_LINK_LIBRARIES ${ffmpeg_lib_dir}/libavfilter.a
#                     )
# set_property(TARGET FFmpeg APPEND PROPERTY
#                         INTERFACE_LINK_LIBRARIES ${ffmpeg_lib_dir}/libavformat.a
#                       )
# set_property(TARGET FFmpeg APPEND PROPERTY
#                         INTERFACE_LINK_LIBRARIES ${ffmpeg_lib_dir}/libswresample.a
#                       )
# set_property(TARGET FFmpeg APPEND PROPERTY
#                       INTERFACE_LINK_LIBRARIES ${ffmpeg_lib_dir}/libswscale.a
#                     )


set(ffmpeg_dir ${CMAKE_CURRENT_LIST_DIR}/third_party/mac/ffmpeg_arm64)
set(ffmpeg_header_dir ${CMAKE_CURRENT_LIST_DIR}/third_party/mac/ffmpeg_arm64/Libs/FFmpeg/include)
endif()
file(GLOB_RECURSE FF_LIBS "${ffmpeg_dir}/*.a")
message("FF_LIBS:  ${FF_LIBS}")
include_directories( ${ffmpeg_header_dir} )
#########FFmpeg######################

#########libyuv######################
add_library( libyuv STATIC IMPORTED GLOBAL)
if(PLATFORM MATCHES "MAC")
set(libyuv_dir ${CMAKE_CURRENT_LIST_DIR}/third_party/mac/libyuv_arm64)
set(libyuv_lib_dir ${libyuv_dir}/lib)
endif()
set_target_properties(libyuv
                        PROPERTIES IMPORTED_LOCATION ${libyuv_lib_dir}/libyuv.a)
include_directories( ${libyuv_dir}/include )
#########libyuv######################

message("CMAKE_CURRENT_LIST_DIR: ${CMAKE_CURRENT_LIST_DIR}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
set(libmdp_name "mdp")
set(libmdp_source_dir ${CMAKE_CURRENT_LIST_DIR}/sources)

file(GLOB_RECURSE libmdp_source_files ${libmdp_source_dir}/**/**.c ${libmdp_source_dir}/**/**.cpp)

message("libmdp_source_files: ${libmdp_source_files}")

add_library(${libmdp_name} SHARED ${libmdp_source_files})
target_include_directories(${libmdp_name} PUBLIC ${libmdp_source_dir})
target_link_libraries( ${libmdp_name} ${FF_LIBS} libyuv  bz2 z lzma iconv "-framework CoreFoundation -framework AudioToolbox -framework VideoToolbox -framework CoreMedia -framework CoreVideo -framework Security" stdc++)

# add_library(media_eye SHARED
#   "media_eye.c"
#   "parser/Parser.cpp"
# )

# set_target_properties(media_eye PROPERTIES
#   PUBLIC_HEADER media_eye.h
#   OUTPUT_NAME "media_eye"
# )
file(GLOB libmdp_parsers_include_files ${libmdp_source_dir}/parsers/**.h)
file(GLOB libmdp_datasource_include_files ${libmdp_source_dir}/datasource/**.h)
file(GLOB libmdp_utils_include_files ${libmdp_source_dir}/utils/**.h)

target_compile_definitions(mdp PUBLIC DART_SHARED_LIB)
