cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(analyze_contour_attr CXX)

set(CMAKE_BUILD_TYPE Release)

add_executable(${PROJECT_NAME}
main.cpp
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../pri/boost.cmake)

target_link_libraries(${PROJECT_NAME}
${BOOST_ROOT}/lib64-msvc-14.0/libboost_program_options-vc140-mt-1_59.lib)
