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

set(GCO_CRIME_SOURCES
    src/crime/CrimeDomain.cpp
    src/crime/CrimeDomain.h
    src/crime/CrimeRegistry.cpp
    src/crime/CrimeRegistry.h
    src/crime/CrimeDirector.cpp
    src/crime/CrimeDirector.h
    src/crime/CrimePersistence.cpp
    src/crime/CrimePersistence.h
    src/crime/CrimeDebugInspector.cpp
    src/crime/CrimeDebugInspector.h
)

set(GCO_ROBBERY_SOURCES
    src/robbery/StoreDomain.cpp
    src/robbery/StoreDomain.h
    src/robbery/StoreTarget.cpp
    src/robbery/StoreTarget.h
    src/robbery/StorePersistence.cpp
    src/robbery/StorePersistence.h
    src/robbery/StoreRuntime.cpp
    src/robbery/StoreRuntime.h
)

set(GCO_WITNESS_SOURCES
    src/witness/WitnessDomain.cpp
    src/witness/WitnessDomain.h
    src/witness/WitnessDirector.cpp
    src/witness/WitnessDirector.h
)

set(GCO_IDENTITY_SOURCES
    src/identity/IdentitySystem.cpp
    src/identity/IdentitySystem.h
    src/identity/ClerkRecognitionDirector.cpp
    src/identity/ClerkRecognitionDirector.h
)

set(GCO_VEHICLE_SOURCES
    src/vehicle/VehicleDomain.cpp
    src/vehicle/VehicleDomain.h
    src/vehicle/VehicleIdentitySystem.cpp
    src/vehicle/VehicleIdentitySystem.h
    src/vehicle/VehiclePersistence.cpp
    src/vehicle/VehiclePersistence.h
    src/vehicle/GarageVehicleService.cpp
    src/vehicle/GarageVehicleService.h
    src/vehicle/VehicleIdentityDirector.cpp
    src/vehicle/VehicleIdentityDirector.h
)

set(GCO_INVESTIGATION_SOURCES
    src/investigation/InvestigationDomain.cpp
    src/investigation/InvestigationDomain.h
    src/investigation/DispatchDirector.cpp
    src/investigation/DispatchDirector.h
    src/investigation/InvestigationDirector.cpp
    src/investigation/InvestigationDirector.h
)

add_library(GTA_Crime_Overhaul SHARED
    src/main.cpp
    src/Runtime.cpp
    src/Runtime.h
    src/CoreServices.cpp
    src/CoreServices.h
    src/Foundation.cpp
    src/Foundation.h
    src/IdStatePersistence.cpp
    ${GCO_CRIME_SOURCES}
    ${GCO_ROBBERY_SOURCES}
    ${GCO_WITNESS_SOURCES}
    ${GCO_IDENTITY_SOURCES}
    ${GCO_VEHICLE_SOURCES}
    ${GCO_INVESTIGATION_SOURCES}
    src/dialogue/InvestigationDialogue.cpp
    src/dialogue/InvestigationDialogue.h
    src/dialogue/SpeakerPresentation.cpp
    src/dialogue/SpeakerPresentation.h
    src/platform/PlatformTypes.cpp
    src/platform/PlatformTypes.h
    src/platform/PlatformAdapters.cpp
    src/platform/PlatformAdapters.h
    src/platform/AdapterDiagnostics.cpp
    src/platform/AdapterDiagnostics.h
    src/platform/FacialAnimationAdapter.cpp
    src/platform/FacialAnimationAdapter.h
    src/platform/PedPresentationAdapter.cpp
    src/platform/PedPresentationAdapter.h
    src/platform/RobberyPedAdapter.cpp
    src/platform/RobberyPedAdapter.h
    src/platform/WitnessPerceptionAdapter.cpp
    src/platform/WitnessPerceptionAdapter.h
    src/platform/VehicleIdentityAdapter.cpp
    src/platform/VehicleIdentityAdapter.h
    src/platform/PoliceInvestigationAdapter.cpp
    src/platform/PoliceInvestigationAdapter.h
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
    target_include_directories(GCO_PlatformTypesTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

    add_executable(GCO_CoreServicesTests
        tests/CoreServicesTests.cpp
        src/CoreServices.cpp
        src/CoreServices.h
    )
    target_include_directories(GCO_CoreServicesTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
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
    target_include_directories(GCO_PersistenceTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(GCO_PersistenceTests PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
        GCO_VERSION="${PROJECT_VERSION}"
        GCO_BUILD_CONFIG="$<CONFIG>"
        GCO_GIT_SHA="${GCO_GIT_SHA}"
    )

    add_executable(GCO_InvestigationDialogueTests
        tests/InvestigationDialogueTests.cpp
        src/dialogue/InvestigationDialogue.cpp
        src/dialogue/InvestigationDialogue.h
        src/dialogue/SpeakerPresentation.h
    )
    target_include_directories(GCO_InvestigationDialogueTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

    add_executable(GCO_SpeakerPresentationTests
        tests/SpeakerPresentationTests.cpp
        src/dialogue/SpeakerPresentation.cpp
        src/dialogue/SpeakerPresentation.h
    )
    target_include_directories(GCO_SpeakerPresentationTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

    add_executable(GCO_CrimeDomainTests
        tests/CrimeDomainTests.cpp
        src/crime/CrimeDomain.cpp
        src/crime/CrimeRegistry.cpp
        src/crime/CrimeDirector.cpp
        src/crime/CrimeDebugInspector.cpp
        src/CoreServices.cpp
    )
    target_include_directories(GCO_CrimeDomainTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(GCO_CrimeDomainTests PRIVATE
        GCO_VERSION="${PROJECT_VERSION}"
        GCO_BUILD_CONFIG="$<CONFIG>"
        GCO_GIT_SHA="${GCO_GIT_SHA}"
    )

    add_executable(GCO_CrimePersistenceTests
        tests/CrimePersistenceTests.cpp
        src/crime/CrimeDomain.cpp
        src/crime/CrimeRegistry.cpp
        src/crime/CrimeDirector.cpp
        src/crime/CrimePersistence.cpp
        src/Foundation.cpp
        src/IdStatePersistence.cpp
        src/CoreServices.cpp
    )
    target_include_directories(GCO_CrimePersistenceTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(GCO_CrimePersistenceTests PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
        GCO_VERSION="${PROJECT_VERSION}"
        GCO_BUILD_CONFIG="$<CONFIG>"
        GCO_GIT_SHA="${GCO_GIT_SHA}"
    )

    add_executable(GCO_StoreDomainTests
        tests/StoreDomainTests.cpp
        src/robbery/StoreDomain.cpp
        src/CoreServices.cpp
    )
    target_include_directories(GCO_StoreDomainTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(GCO_StoreDomainTests PRIVATE
        GCO_VERSION="${PROJECT_VERSION}"
        GCO_BUILD_CONFIG="$<CONFIG>"
        GCO_GIT_SHA="${GCO_GIT_SHA}"
    )

    add_executable(GCO_StoreTargetTests
        tests/StoreTargetTests.cpp
        src/robbery/StoreTarget.cpp
        src/robbery/StoreDomain.cpp
        src/CoreServices.cpp
    )
    target_include_directories(GCO_StoreTargetTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(GCO_StoreTargetTests PRIVATE
        GCO_VERSION="${PROJECT_VERSION}"
        GCO_BUILD_CONFIG="$<CONFIG>"
        GCO_GIT_SHA="${GCO_GIT_SHA}"
    )

    add_executable(GCO_StorePersistenceTests
        tests/StorePersistenceTests.cpp
        src/robbery/StorePersistence.cpp
        src/robbery/StoreDomain.cpp
        src/identity/IdentitySystem.cpp
        src/Foundation.cpp
        src/IdStatePersistence.cpp
        src/CoreServices.cpp
        src/platform/PlatformTypes.cpp
    )
    target_include_directories(GCO_StorePersistenceTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(GCO_StorePersistenceTests PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
        GCO_VERSION="${PROJECT_VERSION}"
        GCO_BUILD_CONFIG="$<CONFIG>"
        GCO_GIT_SHA="${GCO_GIT_SHA}"
    )

    add_executable(GCO_WitnessDomainTests
        tests/WitnessDomainTests.cpp
        src/witness/WitnessDomain.cpp
        src/identity/IdentitySystem.cpp
        src/platform/PlatformTypes.cpp
    )
    target_include_directories(GCO_WitnessDomainTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

    add_executable(GCO_IdentitySystemTests
        tests/IdentitySystemTests.cpp
        src/identity/IdentitySystem.cpp
        src/platform/PlatformTypes.cpp
    )
    target_include_directories(GCO_IdentitySystemTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

    add_executable(GCO_VehicleIdentityTests
        tests/VehicleIdentityTests.cpp
        src/vehicle/VehicleDomain.cpp
        src/vehicle/VehicleIdentitySystem.cpp
        src/vehicle/VehiclePersistence.cpp
        src/vehicle/GarageVehicleService.cpp
        src/crime/CrimeDomain.cpp
        src/Foundation.cpp
        src/IdStatePersistence.cpp
        src/CoreServices.cpp
        src/platform/PlatformTypes.cpp
    )
    target_include_directories(GCO_VehicleIdentityTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(GCO_VehicleIdentityTests PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
        GCO_VERSION="${PROJECT_VERSION}"
        GCO_BUILD_CONFIG="$<CONFIG>"
        GCO_GIT_SHA="${GCO_GIT_SHA}"
    )

    add_executable(GCO_InvestigationDomainTests
        tests/InvestigationDomainTests.cpp
        src/investigation/InvestigationDomain.cpp
        src/crime/CrimeDomain.cpp
    )
    target_include_directories(GCO_InvestigationDomainTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

    add_executable(GCO_DispatchDirectorTests
        tests/DispatchDirectorTests.cpp
        src/investigation/InvestigationDomain.cpp
        src/investigation/DispatchDirector.cpp
        src/crime/CrimeDomain.cpp
        src/crime/CrimeRegistry.cpp
        src/crime/CrimeDirector.cpp
        src/CoreServices.cpp
    )
    target_include_directories(GCO_DispatchDirectorTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(GCO_DispatchDirectorTests PRIVATE
        GCO_VERSION="${PROJECT_VERSION}"
        GCO_BUILD_CONFIG="$<CONFIG>"
        GCO_GIT_SHA="${GCO_GIT_SHA}"
    )

    foreach(test_target IN ITEMS
        GCO_PlatformTypesTests
        GCO_CoreServicesTests
        GCO_PersistenceTests
        GCO_InvestigationDialogueTests
        GCO_SpeakerPresentationTests
        GCO_CrimeDomainTests
        GCO_CrimePersistenceTests
        GCO_StoreDomainTests
        GCO_StoreTargetTests
        GCO_StorePersistenceTests
        GCO_WitnessDomainTests
        GCO_IdentitySystemTests
        GCO_VehicleIdentityTests
        GCO_InvestigationDomainTests
        GCO_DispatchDirectorTests)
        if(MSVC)
            target_compile_options(${test_target} PRIVATE /W4 /permissive- /EHsc /utf-8)
        endif()
    endforeach()

    add_test(NAME PlatformTypes COMMAND GCO_PlatformTypesTests)
    add_test(NAME CoreServices COMMAND GCO_CoreServicesTests)
    add_test(NAME Persistence COMMAND GCO_PersistenceTests)
    add_test(NAME InvestigationDialogue COMMAND GCO_InvestigationDialogueTests)
    add_test(NAME SpeakerPresentation COMMAND GCO_SpeakerPresentationTests)
    add_test(NAME CrimeDomain COMMAND GCO_CrimeDomainTests)
    add_test(NAME CrimePersistence COMMAND GCO_CrimePersistenceTests)
    add_test(NAME StoreDomain COMMAND GCO_StoreDomainTests)
    add_test(NAME StoreTarget COMMAND GCO_StoreTargetTests)
    add_test(NAME StorePersistence COMMAND GCO_StorePersistenceTests)
    add_test(NAME WitnessDomain COMMAND GCO_WitnessDomainTests)
    add_test(NAME IdentitySystem COMMAND GCO_IdentitySystemTests)
    add_test(NAME VehicleIdentity COMMAND GCO_VehicleIdentityTests)
    add_test(NAME InvestigationDomain COMMAND GCO_InvestigationDomainTests)
    add_test(NAME DispatchDirector COMMAND GCO_DispatchDirectorTests)
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
