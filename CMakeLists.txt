cmake_minimum_required(VERSION 3.16)
project(RSCtrlApp_Commons LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt5 COMPONENTS Core REQUIRED)

add_library(RSCtrlApp_Commons STATIC
    FrameRecv/FrameDispatcher.cpp
    GimbalDev/FocalLengthHandler.cpp
)

target_include_directories(RSCtrlApp_Commons PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(RSCtrlApp_Commons PUBLIC
    Qt5::Core
)

add_library(RSCtrlApp::Commons ALIAS RSCtrlApp_Commons)
