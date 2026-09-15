cmake_minimum_required(VERSION 3.24)

project(GTA_Crime_Overhaul VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "GTA Crime Overhaul is a Windows x64 GTA V ASI plugin.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Only x64 builds are supported.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SCRIPTHOOKV_SDK_DIR "" CACHE PATH "Path to the official ScriptHookV SDK root containing inc/ and lib/")

if(NOT SCRIPTHOOKV_SDK_DIR AND DEFINED ENV{SCRIPTHOOKV_SDK_DIR})
    file(TO_CMAKE_PATH "$ENV{SCRIPTHOOKV_SDK_DIR}" SCRIPTHOOKV_SDK_DIR)
endif()

if(NOT EXISTS "${SCRIPTHOOKV_SDK_DIR}/inc/main.h")
    message(FATAL_ERROR
        "ScriptHookV SDK not found. Set -DSCRIPTHOOKV_SDK_DIR=<path> or the "
        "SCRIPTHOOKV_SDK_DIR environment variable. Expected inc/main.h.")
endif()

find_library(SCRIPTHOOKV_LIBRARY
    NAMES ScriptHookV
    PATHS "${SCRIPTHOOKV_SDK_DIR}/lib"
    NO_DEFAULT_PATH
)

if(NOT SCRIPTHOOKV_LIBRARY)
    message(FATAL_ERROR "ScriptHookV.lib not found under ${SCRIPTHOOKV_SDK_DIR}/lib")
endif()

add_library(GTA_Crime_Overhaul SHARED
    src/main.cpp
    src/Runtime.cpp
    src/Runtime.h
    src/Foundation.cpp
    src/Foundation.h
    src/platform/PlatformTypes.cpp
    src/platform/PlatformTypes.h
    src/platform/PlatformAdapters.cpp
    src/platform/PlatformAdapters.h
)

target_include_directories(GTA_Crime_Overhaul PRIVATE
    "${SCRIPTHOOKV_SDK_DIR}/inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(GTA_Crime_Overhaul PRIVATE
    "${SCRIPTHOOKV_LIBRARY}"
)

target_compile_definitions(GTA_Crime_Overhaul PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    GCO_VERSION="${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_options(GTA_Crime_Overhaul PRIVATE
        /W4
        /permissive-
        /EHsc
        /utf-8
        $<$<CONFIG:Release>:/O2>
    )
endif()

set_target_properties(GTA_Crime_Overhaul PROPERTIES
    PREFIX ""
    OUTPUT_NAME "GTA_Crime_Overhaul"
    SUFFIX ".asi"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

include(CTest)
if(BUILD_TESTING)
    add_executable(GCO_PlatformTypesTests
        tests/PlatformTypesTests.cpp
        src/platform/PlatformTypes.cpp
        src/platform/PlatformTypes.h
    )
    target_include_directories(GCO_PlatformTypesTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    if(MSVC)
        target_compile_options(GCO_PlatformTypesTests PRIVATE
            /W4
            /permissive-
            /EHsc
            /utf-8
        )
    endif()
    add_test(NAME PlatformTypes COMMAND GCO_PlatformTypesTests)
endif()

install(TARGETS GTA_Crime_Overhaul
    RUNTIME DESTINATION .
    LIBRARY DESTINATION .
)

install(FILES "config/GTA_Crime_Overhaul.ini"
    DESTINATION "GTA_Crime_Overhaul"
)

install(DIRECTORY "data/"
    DESTINATION "GTA_Crime_Overhaul/data"
)
