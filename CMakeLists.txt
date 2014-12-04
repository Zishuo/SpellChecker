cmake_minimum_required(VERSION 2.8) 
project(SpellCheck)

set(CMAKE_CXX_COMPILER /usr/bin/g++)
set(CMAKE_CXX_FLAGS "-std=c++11 -Wall")

find_package(Boost 1.55.0 REQUIRED COMPONENTS thread log system)
if(Boost_FOUND)
	message(STATUS "Boost found")
	message(STATUS "Boost libraries: " ${Boost_LIBRARIES})
    add_definitions(-DBOOST_ALL_DYN_LINK)
endif()

message(STATUS "c++ flag : " ${CMAKE_CXX_FLAGS}) 
message(STATUS "this is source dir " ${PROJECT_SOURCE_DIR})
message(STATUS "this is binary dir " ${PROJECT_BINARY_DIR})

include_directories(./)
include_directories(./gen-cpp)
include_directories(Boost_INCLUDE_DIRS)
add_subdirectory(gen-cpp)

add_executable(SpellClient SpellClient.cpp)
add_executable(SpellServer SpellServer.cpp)
if(UNIX)
	target_link_libraries(
        SpellClient 
        ${Boost_LIBRARIES}
		pthread
        SpellService
        thrift
        crypto 
		)
	target_link_libraries(
        SpellServer
        pthread
        ${Boost_LIBRARIES}
        SpellService
        thrift
        )
elseif(WIN32)
elseif(APPLE)
endif(UNIX)

