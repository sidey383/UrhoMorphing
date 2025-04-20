cmake_minimum_required(VERSION 3.10)
project(MyFBXViewer)

set(CMAKE_CXX_STANDARD 17)

# Установите путь до Urho3D
set(URHO3D_HOME "$ENV{URHO3D_HOME}" CACHE PATH "Path to Urho3D build tree")

# Добавляем путь к FindUrho3D.cmake
list(APPEND CMAKE_MODULE_PATH "${URHO3D_HOME}/share/CMake/Modules")

# Найти Urho3D
find_package(Urho3D REQUIRED)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(URHO3D_LIB_NAME Urho3D_d)
else()
    set(URHO3D_LIB_NAME Urho3D)
endif()


# Добавить include директории Urho3D вручную
include_directories("${URHO3D_HOME}/include")
include_directories("${URHO3D_HOME}/include/Urho3D/ThirdParty")
link_directories("${URHO3D_HOME}/lib")

# FBX SDK
find_path(FBX_INCLUDE_DIR fbxsdk.h PATHS "$ENV{FBX_SDK}/include")

# Попробуем найти libfbxsdk вручную в типичных путях
find_library(FBX_LIB 
    NAMES libfbxsdk 
    PATHS 
        "$ENV{FBX_SDK}/lib/vs2015/x64/release" 
        "$ENV{FBX_SDK}/lib/vs2017/x64/release" 
        "$ENV{FBX_SDK}/lib/vs2022/x64/release"
)

if(NOT FBX_INCLUDE_DIR OR NOT FBX_LIB)
    message(FATAL_ERROR "FBX SDK not found. Set FBX_SDK environment variable correctly.")
endif()

if (MSVC)
    set(URHO3D_LIB_NAME Urho3D$<$<CONFIG:Debug>:_d>)
endif()

include_directories(${FBX_INCLUDE_DIR})

file(GLOB_RECURSE SRC_FILES src/*.cpp src/*.h)

add_executable(MyFBXViewer WIN32 ${SRC_FILES})

target_link_libraries(MyFBXViewer
	Urho3D
	${FBX_LIB}
    d3d11
    dxgi
    d3dcompiler
	uuid
    setupapi
    winmm
    imm32
    version
)

# Путь до папки ресурсов в проекте
set(RESOURCE_DIR "${CMAKE_SOURCE_DIR}/Resources")
set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/$<CONFIG>")

# Копирование директорий
add_custom_command(TARGET MyFBXViewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${RESOURCE_DIR}/CoreData"
        "${OUTPUT_DIR}/CoreData"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${RESOURCE_DIR}/Data"
        "${OUTPUT_DIR}/Data"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${RESOURCE_DIR}/FBXData"
        "${OUTPUT_DIR}/FBXData"
)

# FBX SDK DLL
set(FBX_DLL_PATH "$ENV{FBX_SDK}/lib/vs2015/x64/release/libfbxsdk.dll")

# Копирование libfbxsdk.dll (если он есть)
if(EXISTS "${FBX_DLL_PATH}")
    add_custom_command(TARGET MyFBXViewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${FBX_DLL_PATH}"
            "${OUTPUT_DIR}/libfbxsdk.dll"
    )
else()
    message(WARNING "FBX DLL not found at ${FBX_DLL_PATH}. Make sure it's installed.")
endif()
