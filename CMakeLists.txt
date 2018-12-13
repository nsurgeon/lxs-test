
cmake_minimum_required( VERSION "3.5.0" FATAL_ERROR )

project( "tets-m" )

file( GLOB_RECURSE ${PROJECT_NAME}_CXX_SOURCES PATH "${PROJECT_SOURCE_DIR}/src/*.cpp" )

message( STATUS  "CXX sources: ${${PROJECT_NAME}_CXX_SOURCES}" )
message( STATUS "Tool-chain: ${CMAKE_TOOLCHAIN_FILE}" )

set( CMAKE_CXX_STANDARD 17 )

include_directories(
    ${CMAKE_INSTALL_PREFIX}/include
    ${PROJECT_SOURCE_DIR}/include
)

link_directories( ${CMAKE_INSTALL_PREFIX}/lib )

add_library( ${PROJECT_NAME}-l STATIC ${${PROJECT_NAME}_CXX_SOURCES} )


add_executable( ${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/main.cpp )
target_link_libraries( 
    ${PROJECT_NAME} 
    ${PROJECT_NAME}-l
    boost_system
    boost_program_options
    boost_filesystem
)
    