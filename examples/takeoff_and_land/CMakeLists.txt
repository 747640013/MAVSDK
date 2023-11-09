# 运行所需的cmake最低版本
cmake_minimum_required(VERSION 3.10.2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(takeoff_and_land)

add_executable(takeoff_and_land
    takeoff_and_land.cpp
)

find_package(MAVSDK REQUIRED)

target_link_libraries(takeoff_and_land
    MAVSDK::mavsdk
)

# 根据编译器设置警告 -Wall 表示启用所有警告 -Wextra 启用额外的警告
if(NOT MSVC)
    add_compile_options(takeoff_and_land PRIVATE -Wall -Wextra)
else()
    add_compile_options(takeoff_and_land PRIVATE -WX -W2)
endif()
