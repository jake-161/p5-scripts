cmake_minimum_required(VERSION 3.20)

project(P5Cli LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CAMERA_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/SDK/p1camerasdk-linux/dest"
    CACHE PATH "Phase One Camera SDK root")
set(CameraSDK_DIR "${CAMERA_SDK_ROOT}/lib/cmake/CameraSDK"
    CACHE PATH "CameraSDK CMake package directory")

find_package(CameraSDK CONFIG REQUIRED)

add_executable(p5-cli
    src/p5-cli.cpp
)

target_link_libraries(p5-cli PRIVATE CameraSDK::CameraSdkCpp)

# Keep the executable beside its separately supplied SDK libraries.
install(TARGETS p5-cli RUNTIME DESTINATION .)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME cli-smoke
        COMMAND "${CMAKE_COMMAND}" "-DP5_CLI=$<TARGET_FILE:p5-cli>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli-smoke.cmake")
endif()

if(UNIX AND NOT APPLE)
    set_target_properties(p5-cli PROPERTIES
        BUILD_RPATH "${CAMERA_SDK_ROOT}/lib"
        INSTALL_RPATH "$ORIGIN/lib"
    )
endif()
