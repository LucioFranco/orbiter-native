include("${PROJECT_SOURCE_DIR}/cmake/NativeNasspManifest.cmake")
# Build missing mission vessels from the upstream per-module source manifest.
# Keep platform build policy outside NASSP. Do not replace unavailable vessels.
function(orbiter_add_native_mission_module module)
  orbiter_read_nassp_manifest(${module})
  # This source implements the unused RTCC MFD KillableWorker. MCC uses Runnable
  # from thread.h; no worker implementation or cancellation policy is substituted.
  list(FILTER SOURCES EXCLUDE REGEX "/thread\\.cpp$")
  add_library(${module} SHARED ${SOURCES})
  target_include_directories(${module} PRIVATE ${INCLUDE_DIRS} "${NASSP_SOURCE_ROOT}/src_native")
  target_link_libraries(${module} PRIVATE OrbiterCore Orbitersdk PanelSDK lua::lib)
  target_compile_definitions(${module} PRIVATE NASSP_NATIVE OPENORBITER)
  if(NOT WIN32)
    target_compile_options(${module} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-include>"
      "$<$<COMPILE_LANGUAGE:CXX>:${PROJECT_SOURCE_DIR}/Src/Orbiter/Platform/NativeMissionCompat.h>")
  endif()
  if(module STREQUAL "MCC" AND NOT WIN32)
    # Preserve the upstream MSVC goto semantics rather than rewrite RTCC algorithms.
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      target_compile_options(${module} PRIVATE -fms-extensions -ferror-limit=0)
    endif()
  endif()
  set_target_properties(${module} PROPERTIES PREFIX "" SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}"
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/runtime/Modules/ProjectApollo"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/runtime/Modules/ProjectApollo")
endfunction()

if(TARGET Saturn5)
  get_target_property(_mission_sdk OrbiterSDKHeaders INTERFACE_INCLUDE_DIRECTORIES)
  list(GET _mission_sdk 0 _mission_sdk)
  get_filename_component(ORBITERSDK_DIR "${_mission_sdk}" DIRECTORY)
  orbiter_add_native_mission_module(MCC)
  target_link_libraries(MCC PRIVATE Saturn5)
  orbiter_add_native_mission_module(sii)
  set_target_properties(sii PROPERTIES OUTPUT_NAME "SII")
  foreach(module SIVB Flag EasepPSEP EasepLR3 EasepSWC)
    orbiter_add_native_mission_module(${module})
  endforeach()
endif()

# These upstream targets are excluded from ALL by the baseline addon. Original
# missions load them at runtime, so a fresh engine build must produce them.
foreach(module Floodlight ML)
  if(TARGET ${module})
    add_dependencies(orbiter ${module})
  endif()
endforeach()

if(TARGET MCC AND TARGET sii)
  set(_original_scenario "${NASSP_DATA_DIR}/Scenarios/Project Apollo - NASSP/Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 02 - Insertion.scn")
  add_test(NAME nassp_stage_original_insertion COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tools/stage_nassp_mission.py" "${PROJECT_BINARY_DIR}" ${_native_mission_data_args})
  set_tests_properties(nassp_stage_original_insertion PROPERTIES FIXTURES_SETUP nassp_original_insertion)
  add_test(NAME native_original_insertion COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/Tests/NativeOriginalInsertion.py" "$<TARGET_FILE:orbiter>"
    "${PROJECT_BINARY_DIR}/runtime-original-insertion" "${_original_scenario}")
  set_tests_properties(native_original_insertion PROPERTIES FIXTURES_REQUIRED nassp_original_insertion LABELS "gpu;desktop" TIMEOUT 300)
endif()
