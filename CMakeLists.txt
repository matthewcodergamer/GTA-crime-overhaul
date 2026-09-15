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

if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Supported build configurations" FORCE)
endif()

find_package(Git QUIET)
set(GCO_GIT_SHA "unknown")
if(GIT_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE GCO_GIT_SHA
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

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
    src/CoreServices.cpp
    src/CoreServices.h
    src/Foundation.cpp
    src/Foundation.h
    src/IdStatePersistence.cpp
    src/dialogue/InvestigationDialogue.cpp
    src/dialogue/InvestigationDialogue.h
    src/platform/PlatformTypes.cpp
    src/platform/PlatformTypes.h
    src/platform/PlatformAdapters.cpp
    src/platform/PlatformAdapters.h
    src/platform/AdapterDiagnostics.cpp
    src/platform/AdapterDiagnostics.h
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
    GCO_BUILD_CONFIG="$<CONFIG>"
    GCO_GIT_SHA="${GCO_GIT_SHA}"
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

    add_executable(GCO_CoreServicesTests
        tests/CoreServicesTests.cpp
        src/CoreServices.cpp
        src/CoreServices.h
    )
    target_include_directories(GCO_CoreServicesTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_compile_definitions(GCO_CoreServicesTests PRIVATE
        GCO_VERSION="${PROJECT_VERSION}"
        GCO_BUILD_CONFIG="$<CONFIG>"
        GCO_GIT_SHA="${GCO_GIT_SHA}"
    )

    add_executable(GCO_PersistenceTests
        tests/PersistenceTests.cpp
        src/Foundation.cpp
        src/Foundation.h
        src/IdStatePersistence.cpp
        src/CoreServices.cpp
        src/CoreServices.h
    )
    target_include_directories(GCO_PersistenceTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_compile_definitions(GCO_PersistenceTests PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
        GCO_VERSION="${PROJECT_VERSION}"
        GCO_BUILD_CONFIG="$<CONFIG>"
        GCO_GIT_SHA="${GCO_GIT_SHA}"
    )

    add_executable(GCO_InvestigationDialogueTests
        tests/InvestigationDialogueTests.cpp
        src/dialogue/InvestigationDialogue.cpp
        src/dialogue/InvestigationDialogue.h
    )
    target_include_directories(GCO_InvestigationDialogueTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )

    foreach(test_target IN ITEMS GCO_PlatformTypesTests GCO_CoreServicesTests GCO_PersistenceTests GCO_InvestigationDialogueTests)
        if(MSVC)
            target_compile_options(${test_target} PRIVATE
                /W4
                /permissive-
                /EHsc
                /utf-8
            )
        endif()
    endforeach()

    add_test(NAME PlatformTypes COMMAND GCO_PlatformTypesTests)
    add_test(NAME CoreServices COMMAND GCO_CoreServicesTests)
    add_test(NAME Persistence COMMAND GCO_PersistenceTests)
    add_test(NAME InvestigationDialogue COMMAND GCO_InvestigationDialogueTests)
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
