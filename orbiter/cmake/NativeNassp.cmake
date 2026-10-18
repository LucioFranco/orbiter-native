# Native targets for upstream revisions that provide only Visual Studio manifests.
include("${PROJECT_SOURCE_DIR}/cmake/NativeNasspManifest.cmake")
function(orbiter_add_native_panelsdk)
    orbiter_read_nassp_manifest(PanelSDK)
    add_library(PanelSDK STATIC ${SOURCES})
    set_target_properties(PanelSDK PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_include_directories(PanelSDK PRIVATE "${PROJECT_SOURCE_DIR}/Orbitersdk/include" ${INCLUDE_DIRS})
    target_link_libraries(PanelSDK PUBLIC OrbiterSDKHeaders OrbiterCore)
endfunction()

# Shared native module configuration; the original per-module source lists are
# authoritative. Windows import libraries and RC objects are not used here.
function(nassp_add_native_module name)
    set(sources ${ARGN})
    set(native_sources)
    foreach(source IN LISTS sources)
        if(source MATCHES "/vesim\\.cpp$")
            list(APPEND native_sources "${NASSP_SOURCE_ROOT}/src_native/NativeVesim.cpp")
        elseif(source MATCHES "/soundevents\\.cpp$")
            list(APPEND native_sources "${NASSP_SOURCE_ROOT}/src_native/SoundEvents.cpp")
        elseif(source MATCHES "/FDAI\\.cpp$")
            list(APPEND native_sources "${NASSP_SOURCE_ROOT}/src_native/FdaiRasterizer.cpp")
        endif()
    endforeach()
    list(FILTER sources EXCLUDE REGEX "/vesim\\.cpp$")
    list(FILTER sources EXCLUDE REGEX "/soundevents\\.cpp$")
    # KillableWorker is only used by RTCC MFD, not by Saturn's AGC worker.
    if(name STREQUAL "Saturn5" OR name STREQUAL "LEM")
        list(FILTER sources EXCLUDE REGEX "/thread\\.cpp$")
    endif()
    add_library(${name} SHARED ${sources} ${native_sources})
    target_include_directories(${name} PRIVATE "${PROJECT_SOURCE_DIR}/Orbitersdk/include" ${INCLUDE_DIRS} "${NASSP_SOURCE_ROOT}/src_native")
    target_link_libraries(${name} PRIVATE OrbiterCore Orbitersdk PanelSDK NASSPBitmapResources lua::lib)
    target_compile_definitions(${name} PRIVATE NASSP_NATIVE OPENORBITER)
    if(NOT WIN32)
        target_compile_options(${name} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-include>"
      "$<$<COMPILE_LANGUAGE:CXX>:${PROJECT_SOURCE_DIR}/Src/Orbiter/Platform/NativeMissionCompat.h>")
    endif()
    set_target_properties(${name} PROPERTIES
        # Native addon plugins link vessel C++ interfaces directly.
        WINDOWS_EXPORT_ALL_SYMBOLS ON
        PREFIX ""
        SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime/Modules/ProjectApollo"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime/Modules/ProjectApollo")
endfunction()

function(orbiter_add_native_nassp_modules)
    set(NASSP_NATIVE_MODULES "Saturn5;LEM;Sat5LMDSC" CACHE STRING "NASSP modules to build for the native port")
    set(modules ${NASSP_NATIVE_MODULES})
    if("LEM" IN_LIST modules)
        list(APPEND modules Sat5LMDSC)
    endif()
    if("Saturn5" IN_LIST modules)
        list(APPEND modules ML Floodlight)
    endif()
    list(REMOVE_DUPLICATES modules)
    foreach(module IN LISTS modules)
        orbiter_read_nassp_manifest(${module})
        nassp_add_native_module(${module} ${SOURCES})
    endforeach()
    if(TARGET LEM AND TARGET Sat5LMDSC)
        add_dependencies(LEM Sat5LMDSC)
    endif()
    if(TARGET Saturn5 AND TARGET LEM AND NOT TARGET ProjectApolloMFD)
        orbiter_read_nassp_manifest(ProjectApolloMFD)
        nassp_add_native_module(ProjectApolloMFD ${SOURCES})
        target_link_libraries(ProjectApolloMFD PRIVATE Saturn5 LEM)
        set_target_properties(ProjectApolloMFD PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/runtime/Modules/Plugin"
            RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/runtime/Modules/Plugin")
    endif()
endfunction()
