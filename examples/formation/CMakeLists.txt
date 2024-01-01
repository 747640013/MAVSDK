cmake_minimum_required(VERSION 3.10.2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(formation)

find_package(MAVSDK REQUIRED)
find_package(Threads REQUIRED)
find_package(GeographicLib REQUIRED)  


add_executable(test
    test.cpp
    communicator.cpp
)

target_include_directories(test PRIVATE ${GeographicLib_INCLUDE_DIRS})

target_link_libraries(test
    MAVSDK::mavsdk
    Threads::Threads
    GeographicLib::GeographicLib
)

if(NOT MSVC)
    add_compile_options(test PRIVATE -Wall -Wextra)
else()
    add_compile_options(test PRIVATE -WX -W2)
endif()
