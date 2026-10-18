set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(OrbiterSDKHeaders INTERFACE)
target_include_directories(OrbiterSDKHeaders INTERFACE "${PROJECT_SOURCE_DIR}/Orbitersdk/include")
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(OrbiterSDKHeaders INTERFACE
    -ffp-contract=off -Werror=pointer-to-int-cast -Werror=int-to-pointer-cast -Werror=format)
endif()

# Existing simulation core plus a native scenario host (see NativeWorld.cmake).
# SDL input and the graphics client are still under development.
add_library(OrbiterCore SHARED
  "${PROJECT_SOURCE_DIR}/Src/Orbiter/Astro.cpp"
  "${PROJECT_SOURCE_DIR}/Src/Orbiter/Vecmat.cpp"
  "${PROJECT_SOURCE_DIR}/Src/Orbiter/TimeData.cpp"
  "${PROJECT_SOURCE_DIR}/Src/Orbiter/CoreState.cpp"
  "${PROJECT_SOURCE_DIR}/Src/Orbiter/Config.cpp"
  "${PROJECT_SOURCE_DIR}/Src/Orbiter/State.cpp"
  "${PROJECT_SOURCE_DIR}/Src/Orbiter/Color.cpp"
  "${PROJECT_SOURCE_DIR}/Src/Orbiter/Element.cpp"
  "${PROJECT_SOURCE_DIR}/Src/Orbiter/Log.cpp"
  "${PROJECT_SOURCE_DIR}/Src/Orbiter/Platform/Joystick.cpp"
  "${PROJECT_SOURCE_DIR}/Src/Orbiter/Platform/Vfs.cpp"
  "${PROJECT_SOURCE_DIR}/Src/Orbiter/Platform/DynamicLibrary.cpp"
  "${PROJECT_SOURCE_DIR}/Src/Orbiter/Platform/ModuleLibrary.cpp")
set_target_properties(OrbiterCore PROPERTIES OUTPUT_NAME orbiter WINDOWS_EXPORT_ALL_SYMBOLS ON)
target_link_libraries(OrbiterCore PUBLIC OrbiterSDKHeaders)
target_include_directories(OrbiterCore PUBLIC "${PROJECT_SOURCE_DIR}/Src/Orbiter")
target_compile_definitions(OrbiterCore PRIVATE OAPI_IMPLEMENTATION)
target_link_libraries(OrbiterCore PRIVATE ${CMAKE_DL_LIBS})
if(WIN32)
  target_link_libraries(OrbiterCore PRIVATE psapi version)
endif()

include("${PROJECT_SOURCE_DIR}/cmake/NativeImage.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/NativeImGui.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/NativeWorld.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/NativeCelbody.cmake")

option(ORBITER_BUILD_NASSP "Compile NASSP PanelSDK in the portability checkpoint" ON)
if(ORBITER_BUILD_NASSP)
  set(NASSP_ADDON_DIR "${PROJECT_SOURCE_DIR}/Addons/NASSP" CACHE PATH "Adapted NASSP source root (data selected separately)")
  set(NASSP_SOURCE_ROOT "${NASSP_ADDON_DIR}/Orbitersdk/samples/ProjectApollo")
  set(NASSP_DATA_DIR "${NASSP_ADDON_DIR}" CACHE PATH "External original NASSP data, separate from adapted source")
  set(NASSP_DATA_PROFILE "auto" CACHE STRING "Pinned NASSP data profile: auto, baseline, candidate, v9")
  set_property(CACHE NASSP_DATA_PROFILE PROPERTY STRINGS auto baseline candidate v9)
  set(ORBITER_TEXTURE_DIR "" CACHE PATH "Optional external Orbiter Textures directory")
  set(_native_preflight_args)
  if(ORBITER_TEXTURE_DIR)
    list(APPEND _native_preflight_args --orbiter-textures "${ORBITER_TEXTURE_DIR}")
  endif()
  execute_process(COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/nassp_demo.py"
    check-data --nassp-data-root "${NASSP_DATA_DIR}" --profile "${NASSP_DATA_PROFILE}"
    --source-addon "${NASSP_ADDON_DIR}" ${_native_preflight_args}
    RESULT_VARIABLE _data_result OUTPUT_VARIABLE _data_report ERROR_VARIABLE _data_error)
  if(NOT _data_result EQUAL 0)
    message(FATAL_ERROR "${_data_error}\nSet -DNASSP_DATA_DIR=<matching-external-data>. See docs/clean-macos-demos.md.")
  endif()
  string(JSON _native_data_profile GET "${_data_report}" profile)
  set(_native_data_args --addon "${NASSP_DATA_DIR}" --profile "${NASSP_DATA_PROFILE}")
  set(_native_mission_data_args ${_native_data_args})
  if(ORBITER_TEXTURE_DIR)
    list(APPEND _native_mission_data_args --textures "${ORBITER_TEXTURE_DIR}")
  endif()
  get_filename_component(ORBITERSDK_DIR "${PROJECT_SOURCE_DIR}/Orbitersdk" ABSOLUTE)
  include("${PROJECT_SOURCE_DIR}/cmake/NativeNassp.cmake")
  if(_native_data_profile STREQUAL "v9")
    # Read upstream source lists while keeping native linkage and platform
    # selection outside NASSP's Windows-only installation/detection policy.
    orbiter_add_native_panelsdk()
  elseif(EXISTS "${NASSP_SOURCE_ROOT}/Project/PanelSDK/CMakeLists.txt")
    add_subdirectory("${NASSP_SOURCE_ROOT}/Project/PanelSDK" "Addons/NASSP/PanelSDK")
  else()
    orbiter_add_native_panelsdk()
  endif()
endif()

enable_testing()
if(ORBITER_BUILD_NASSP)
  add_test(NAME native_data_preflight COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/Tests/NativeDataPreflight.py" "${PROJECT_BINARY_DIR}"
    "${NASSP_DATA_DIR}" "${_native_data_profile}" "${NASSP_ADDON_DIR}" ${_native_preflight_args})
  if(_native_data_profile STREQUAL "v9")
    add_test(NAME native_manual_step_alignment COMMAND "${Python3_EXECUTABLE}"
      "${PROJECT_SOURCE_DIR}/Tests/NativeManualStepAlignment.py" "$<TARGET_FILE:native_parity_probe>"
      "${PROJECT_BINARY_DIR}" "${NASSP_DATA_DIR}" "${PROJECT_BINARY_DIR}/manual-step-alignment-evidence")
    set_tests_properties(native_manual_step_alignment PROPERTIES
      ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1" LABELS "simulation" TIMEOUT 180)
  endif()
endif()
include("${PROJECT_SOURCE_DIR}/cmake/NativeAudio.cmake")
add_executable(portable_sdk_test "${PROJECT_SOURCE_DIR}/Tests/PortableSDK.cpp")
target_link_libraries(portable_sdk_test PRIVATE OrbiterSDKHeaders)
add_test(NAME portable_sdk COMMAND portable_sdk_test)

add_library(Orbitersdk OBJECT "${PROJECT_SOURCE_DIR}/Src/Orbitersdk/Orbitersdk.cpp")
target_link_libraries(Orbitersdk PUBLIC OrbiterSDKHeaders)
set_target_properties(Orbitersdk PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(core_module_fixture MODULE "${PROJECT_SOURCE_DIR}/Tests/CoreModuleFixture.cpp")
target_link_libraries(core_module_fixture PRIVATE Orbitersdk)
target_link_libraries(core_module_fixture PRIVATE OrbiterCore)
set_target_properties(core_module_fixture PROPERTIES PREFIX "" SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
add_executable(native_core_test "${PROJECT_SOURCE_DIR}/Tests/NativeCore.cpp")
target_link_libraries(native_core_test PRIVATE OrbiterCore)
add_dependencies(native_core_test core_module_fixture)
add_test(NAME native_core COMMAND native_core_test "$<TARGET_FILE:core_module_fixture>")

add_library(legacy_module_fixture MODULE "${PROJECT_SOURCE_DIR}/Tests/CoreModuleFixture.cpp")
target_compile_definitions(legacy_module_fixture PRIVATE LEGACY_MODULE_CALLBACKS)
target_link_libraries(legacy_module_fixture PRIVATE OrbiterCore Orbitersdk)
set_target_properties(legacy_module_fixture PROPERTIES PREFIX "" SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
add_executable(module_lifecycle_test "${PROJECT_SOURCE_DIR}/Tests/ModuleLifecycle.cpp")
target_link_libraries(module_lifecycle_test PRIVATE OrbiterCore)
add_dependencies(module_lifecycle_test core_module_fixture legacy_module_fixture)
add_test(NAME module_lifecycle COMMAND module_lifecycle_test
  "$<TARGET_FILE:core_module_fixture>" "$<TARGET_FILE:legacy_module_fixture>")

if(ORBITER_BUILD_NASSP)
  include("${PROJECT_SOURCE_DIR}/cmake/NasspResources.cmake")
  add_subdirectory("${PROJECT_SOURCE_DIR}/Extern/Lua" "Extern/Lua")
  add_executable(native_lua_test "${PROJECT_SOURCE_DIR}/Tests/NativeLua.cpp")
  target_link_libraries(native_lua_test PRIVATE OrbiterSDKHeaders lua::lib)
  add_test(NAME native_lua COMMAND native_lua_test)
  add_test(NAME nassp_mesh COMMAND native_world_test
    "${PROJECT_SOURCE_DIR}/Tests/Fixtures/NativeWorld"
    "${NASSP_DATA_DIR}/Meshes/ProjectApollo/CM-HatchF.msh")
  if(_native_data_profile STREQUAL "v9")
    orbiter_add_native_nassp_modules()
  elseif(EXISTS "${NASSP_SOURCE_ROOT}/CMakeLists.txt")
    add_subdirectory("${NASSP_SOURCE_ROOT}" "Addons/NASSP/Modules" EXCLUDE_FROM_ALL)
  else()
    orbiter_add_native_nassp_modules()
  endif()
  include("${PROJECT_SOURCE_DIR}/cmake/NativeMissionModules.cmake")
  if(TARGET Saturn5)
    add_executable(nassp_assets_test "${PROJECT_SOURCE_DIR}/Tests/NasspAssets.cpp")
    target_include_directories(nassp_assets_test PRIVATE
      "${NASSP_SOURCE_ROOT}/src_sys/yaAGC" "${NASSP_SOURCE_ROOT}/src_aux")
    target_link_libraries(nassp_assets_test PRIVATE Saturn5 OrbiterCore)
    set(_nassp_checklist "${NASSP_DATA_DIR}/Doc/Project Apollo - NASSP/Checklists/Apollo 11 Checklists.xls")
    if(NOT EXISTS "${_nassp_checklist}")
      set(_nassp_checklist "${PROJECT_SOURCE_DIR}/Addons/NASSP/Doc/Project Apollo - NASSP/Checklists/Apollo 11 Checklists.xls")
    endif()
    add_test(NAME nassp_assets COMMAND nassp_assets_test
      "${NASSP_DATA_DIR}/Config/ProjectApollo/Comanche055.bin" "${_nassp_checklist}")
  endif()
  add_executable(scenario_state_test "${PROJECT_SOURCE_DIR}/Tests/ScenarioState.cpp")
  target_link_libraries(scenario_state_test PRIVATE OrbiterCore)
  add_test(NAME nassp_scenario_state COMMAND scenario_state_test
    "${NASSP_DATA_DIR}/Scenarios/Project Apollo - NASSP/Apollo 11 - Launch.scn"
    "${PROJECT_BINARY_DIR}/scenario-roundtrip.scn")
  add_executable(panelsdk_math_test "${PROJECT_SOURCE_DIR}/Tests/PanelSDKMath.cpp")
  target_include_directories(panelsdk_math_test PRIVATE "${NASSP_SOURCE_ROOT}/src_sys/PanelSDK")
  target_link_libraries(panelsdk_math_test PRIVATE PanelSDK)
  add_test(NAME panelsdk_math COMMAND panelsdk_math_test)
endif()

include("${PROJECT_SOURCE_DIR}/cmake/NativeSDL.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/NativeWgpu.cmake")

if(_native_data_profile STREQUAL "v9" AND TARGET Saturn5 AND TARGET LEM AND TARGET MCC)
  add_dependencies(native_parity_probe Saturn5 LEM MCC)
  add_test(NAME native_scenario_compatibility COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/Tests/NativeScenarioCompatibility.py"
    "$<TARGET_FILE:native_parity_probe>" "${PROJECT_BINARY_DIR}" "${NASSP_DATA_DIR}"
    "${ORBITER_TEXTURE_DIR}" "${PROJECT_BINARY_DIR}/scenario-compatibility-evidence")
  set_tests_properties(native_scenario_compatibility PROPERTIES LABELS "simulation"
    ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1" TIMEOUT 120)
endif()

target_sources(OrbiterCore PRIVATE Src/Orbiter/NativeGraphicsClient.cpp Src/Orbiter/GraphicsClientCommon.cpp)

if(ORBITER_BUILD_NASSP)
  add_executable(fdai_rasterizer_test "${PROJECT_SOURCE_DIR}/Tests/FdaiRasterizer.cpp"
    "${NASSP_SOURCE_ROOT}/src_native/FdaiRasterizer.cpp")
  target_include_directories(fdai_rasterizer_test PRIVATE "${NASSP_SOURCE_ROOT}/src_native")
  target_link_libraries(fdai_rasterizer_test PRIVATE OrbiterCore)
  add_test(NAME fdai_rasterizer COMMAND fdai_rasterizer_test
    "${NASSP_DATA_DIR}/Textures/ProjectApollo/FDAI_Ball.dds"
    "${NASSP_DATA_DIR}/Textures/ProjectApollo/FDAI_Ball_LM.dds")
endif()
