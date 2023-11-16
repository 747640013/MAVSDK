cmake_minimum_required(VERSION 3.10.2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(formation)

add_executable(test
    test.cpp
    communicator.cpp
)

find_package(MAVSDK REQUIRED)
find_package(Threads REQUIRED)


target_link_libraries(test
    MAVSDK::mavsdk
    Threads::Threads
)

if(NOT MSVC)
    add_compile_options(test PRIVATE -Wall -Wextra)
else()
    add_compile_options(test PRIVATE -WX -W2)
endif()