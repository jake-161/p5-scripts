cmake_minimum_required(VERSION 3.20)

project(P5Cli LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CAMERA_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/SDK/p1camerasdk-linux/dest")
set(CameraSDK_DIR "${CAMERA_SDK_ROOT}/lib/cmake/CameraSDK")

find_package(CameraSDK CONFIG REQUIRED)

add_executable(p5-cli
    src/p5-cli.cpp
)

target_link_libraries(p5-cli PRIVATE CameraSDK::CameraSdkCpp)

if(UNIX AND NOT APPLE)
    set_target_properties(p5-cli PROPERTIES
        BUILD_RPATH "${CAMERA_SDK_ROOT}/lib"
    )
endif()
