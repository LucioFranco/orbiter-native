option(ORBITER_BUILD_WGPU "Build the native Rust/wgpu renderer" ON)
if(NOT ORBITER_BUILD_WGPU)
  return()
endif()

set(ORBITER_CARGO_HOME "$ENV{CARGO_HOME}" CACHE PATH "Optional project-local Cargo cache")
if(ORBITER_CARGO_HOME)
  set(ENV{CARGO_HOME} "${ORBITER_CARGO_HOME}")
endif()
include(FetchContent)
FetchContent_Declare(Corrosion
  GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
  GIT_TAG a1a1aaa057a5da656c06c3d8505b767a4e941709) # v0.5.2
FetchContent_MakeAvailable(Corrosion)
corrosion_import_crate(MANIFEST_PATH "${PROJECT_SOURCE_DIR}/Src/WgpuClient/rust/Cargo.toml" LOCKED)
if(ORBITER_CARGO_HOME)
  corrosion_set_env_vars(orbiter_wgpu "CARGO_HOME=${ORBITER_CARGO_HOME}")
endif()
if(APPLE)
  # Rust static archives do not carry transitive framework link directives.
  target_link_libraries(orbiter_wgpu INTERFACE
    "-framework Metal" "-framework QuartzCore" "-framework Foundation" objc)
elseif(WIN32)
  # The Rust static archive's Windows API references need explicit import libs
  # in every C++ target that consumes it.
  target_link_libraries(orbiter_wgpu INTERFACE
    opengl32 propsys d3dcompiler runtimeobject)
endif()
add_executable(native_wgpu_test "${PROJECT_SOURCE_DIR}/Tests/NativeWgpu.cpp")
target_include_directories(native_wgpu_test PRIVATE "${PROJECT_SOURCE_DIR}/Src/WgpuClient" "${PROJECT_SOURCE_DIR}/Orbitersdk/include")
target_link_libraries(native_wgpu_test PRIVATE orbiter_wgpu)
add_test(NAME native_wgpu COMMAND native_wgpu_test)
set_tests_properties(native_wgpu PROPERTIES LABELS "gpu")
add_library(WgpuClient MODULE "${PROJECT_SOURCE_DIR}/Src/WgpuClient/WgpuClient.cpp"
  "${PROJECT_SOURCE_DIR}/Src/WgpuClient/NativeFont.cpp" "${native_font_header}")
target_include_directories(WgpuClient PRIVATE "${PROJECT_BINARY_DIR}/generated")
target_include_directories(WgpuClient PRIVATE "${stb_SOURCE_DIR}")
target_link_libraries(WgpuClient PRIVATE OrbiterCore orbiter_wgpu)
set_target_properties(WgpuClient PROPERTIES PREFIX "" SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}"
  LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/runtime/Modules/Plugin"
  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/runtime/Modules/Plugin")
add_executable(wgpu_text_test "${PROJECT_SOURCE_DIR}/Tests/WgpuText.cpp")
target_link_libraries(wgpu_text_test PRIVATE OrbiterCore)
add_dependencies(wgpu_text_test WgpuClient)
add_test(NAME wgpu_text COMMAND wgpu_text_test "$<TARGET_FILE:WgpuClient>")
set_tests_properties(wgpu_text PROPERTIES LABELS "gpu")
if(ORBITER_BUILD_NASSP)
  add_executable(wgpu_client_test "${PROJECT_SOURCE_DIR}/Tests/WgpuClient.cpp")
  target_link_libraries(wgpu_client_test PRIVATE OrbiterCore SDL3::SDL3)
  target_compile_definitions(wgpu_client_test PRIVATE SDL_DISABLE_OLD_NAMES)
  add_dependencies(wgpu_client_test WgpuClient nassp_bitmap_resources)
  add_test(NAME wgpu_client COMMAND wgpu_client_test "$<TARGET_FILE:WgpuClient>"
    "${PROJECT_BINARY_DIR}/runtime/Resources/NASSP/masteralarmbright.bmp"
    "${nassp_bitmap_source_root}/Bitmaps/masteralarmbright.bmp")
  set_tests_properties(wgpu_client PROPERTIES LABELS "gpu")
endif()

if(ORBITER_BUILD_NASSP)
  add_dependencies(native_world_test WgpuClient)
  add_test(NAME wgpu_world COMMAND native_world_test
    "${PROJECT_SOURCE_DIR}/Tests/Fixtures/NativeWorld"
    "${NASSP_DATA_DIR}/Meshes/ProjectApollo/CM-HatchF.msh"
    "$<TARGET_FILE:WgpuClient>")
  set_tests_properties(wgpu_world PROPERTIES LABELS "gpu")
endif()
if(ORBITER_BUILD_NASSP)
  add_executable(wgpu_present_test "${PROJECT_SOURCE_DIR}/Tests/WgpuPresent.cpp")
  target_link_libraries(wgpu_present_test PRIVATE OrbiterSDL)
  add_dependencies(wgpu_present_test WgpuClient nassp_bitmap_resources)
  add_test(NAME wgpu_present COMMAND wgpu_present_test "$<TARGET_FILE:WgpuClient>"
    "${PROJECT_BINARY_DIR}/runtime/Resources/NASSP/csm_main_panel.bmp")
  set_tests_properties(wgpu_present PROPERTIES LABELS "gpu;desktop")
endif()
if(APPLE AND TARGET wgpu_present_test)
  set_target_properties(wgpu_present_test PROPERTIES MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "local.orbiter.wgpu-preview"
    MACOSX_BUNDLE_BUNDLE_NAME "Orbiter GPU Preview")
endif()
if(ORBITER_BUILD_NASSP)
  add_executable(wgpu_cockpit_test "${PROJECT_SOURCE_DIR}/Tests/WgpuCockpit.cpp")
  target_link_libraries(wgpu_cockpit_test PRIVATE OrbiterSDL)
  add_dependencies(wgpu_cockpit_test WgpuClient)
  if(APPLE)
    set_target_properties(wgpu_cockpit_test PROPERTIES MACOSX_BUNDLE TRUE
      MACOSX_BUNDLE_GUI_IDENTIFIER "local.orbiter.cockpit-preview"
      MACOSX_BUNDLE_BUNDLE_NAME "Orbiter 3D Cockpit")
  endif()
endif()
add_executable(wgpu_mesh_test "${PROJECT_SOURCE_DIR}/Tests/WgpuMesh.cpp")
target_include_directories(wgpu_mesh_test PRIVATE "${PROJECT_SOURCE_DIR}/Src/WgpuClient" "${PROJECT_SOURCE_DIR}/Orbitersdk/include")
target_link_libraries(wgpu_mesh_test PRIVATE orbiter_wgpu)
add_test(NAME wgpu_mesh COMMAND wgpu_mesh_test)
set_tests_properties(wgpu_mesh PROPERTIES LABELS "gpu")
add_executable(wgpu_dds_test "${PROJECT_SOURCE_DIR}/Tests/WgpuDDS.cpp")
target_include_directories(wgpu_dds_test PRIVATE "${PROJECT_SOURCE_DIR}/Src/WgpuClient" "${PROJECT_SOURCE_DIR}/Orbitersdk/include")
target_link_libraries(wgpu_dds_test PRIVATE orbiter_wgpu)
add_test(NAME wgpu_dds COMMAND wgpu_dds_test)
set_tests_properties(wgpu_dds PROPERTIES LABELS "gpu")

if(ORBITER_BUILD_NASSP)
  add_test(NAME nassp_stage_cockpit COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tools/stage_nassp_smoke.py" "${PROJECT_BINARY_DIR}" ${_native_data_args})
  set_tests_properties(nassp_stage_cockpit PROPERTIES FIXTURES_SETUP nassp_cockpit_assets)
  add_test(NAME wgpu_live_cockpit COMMAND wgpu_cockpit_test "$<TARGET_FILE:WgpuClient>"
    "${PROJECT_BINARY_DIR}/runtime" 8 --live)
  set_tests_properties(wgpu_live_cockpit PROPERTIES LABELS "gpu;desktop"
    FIXTURES_REQUIRED nassp_cockpit_assets TIMEOUT 120)
  add_test(NAME nassp_stage_solar COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tools/stage_nassp_smoke.py" "${PROJECT_BINARY_DIR}" ${_native_data_args} --insertion --solar)
  set_tests_properties(nassp_stage_solar PROPERTIES FIXTURES_SETUP nassp_solar_assets)
  add_executable(native_solar_test "${PROJECT_SOURCE_DIR}/Tests/NativeSolar.cpp")
  target_link_libraries(native_solar_test PRIVATE OrbiterCore)
  add_test(NAME native_solar COMMAND native_solar_test "${PROJECT_BINARY_DIR}/runtime-solar")
  set_tests_properties(native_solar PROPERTIES FIXTURES_REQUIRED nassp_solar_assets TIMEOUT 120)
  add_test(NAME nassp_stage_csm COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tools/stage_nassp_smoke.py" "${PROJECT_BINARY_DIR}" ${_native_data_args} --csm)
  set_tests_properties(nassp_stage_csm PROPERTIES FIXTURES_SETUP nassp_csm_assets)
  add_test(NAME wgpu_csm_cockpit COMMAND wgpu_cockpit_test "$<TARGET_FILE:WgpuClient>"
    "${PROJECT_BINARY_DIR}/runtime-csm" 8 --csm)
  set_tests_properties(wgpu_csm_cockpit PROPERTIES LABELS "gpu;desktop"
    FIXTURES_REQUIRED nassp_csm_assets TIMEOUT 180)
  add_executable(native_rcs_test "${PROJECT_SOURCE_DIR}/Tests/NativeRCS.cpp")
  target_link_libraries(native_rcs_test PRIVATE OrbiterSDL)
  add_test(NAME native_rcs COMMAND native_rcs_test "${PROJECT_BINARY_DIR}/runtime-csm")
  set_tests_properties(native_rcs PROPERTIES FIXTURES_REQUIRED nassp_csm_assets TIMEOUT 120)
  add_executable(native_pause_test "${PROJECT_SOURCE_DIR}/Tests/NativePause.cpp")
  target_link_libraries(native_pause_test PRIVATE OrbiterSDL)
  add_dependencies(native_pause_test core_module_fixture)
  add_test(NAME native_pause COMMAND native_pause_test "${PROJECT_BINARY_DIR}/runtime-csm" "$<TARGET_FILE:core_module_fixture>")
  set_tests_properties(native_pause PROPERTIES FIXTURES_REQUIRED nassp_csm_assets TIMEOUT 120)
  add_executable(native_startup_pause_test "${PROJECT_SOURCE_DIR}/Tests/NativeStartupPause.cpp")
  target_link_libraries(native_startup_pause_test PRIVATE OrbiterSDL)
  add_dependencies(native_startup_pause_test core_module_fixture)
  add_test(NAME native_startup_pause COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/Tests/NativeStartupPause.py" "$<TARGET_FILE:native_startup_pause_test>"
    "${PROJECT_BINARY_DIR}/runtime-csm" "$<TARGET_FILE:core_module_fixture>"
    "${PROJECT_BINARY_DIR}/startup-pause-evidence")
  set_tests_properties(native_startup_pause PROPERTIES FIXTURES_REQUIRED nassp_csm_assets
    ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1" TIMEOUT 180)
  add_test(NAME native_startup_pause_graphics COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/Tests/NativeStartupPause.py" "$<TARGET_FILE:native_startup_pause_test>"
    "${PROJECT_BINARY_DIR}/runtime-csm" "$<TARGET_FILE:core_module_fixture>"
    "${PROJECT_BINARY_DIR}/startup-pause-graphics-evidence" "$<TARGET_FILE:WgpuClient>")
  set_tests_properties(native_startup_pause_graphics PROPERTIES FIXTURES_REQUIRED nassp_csm_assets
    ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1" LABELS "gpu;desktop" TIMEOUT 240)
  add_test(NAME native_nassp_resume COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/Tests/NativeResume.py" "$<TARGET_FILE:orbiter>" "${PROJECT_BINARY_DIR}/runtime-solar")
  set_tests_properties(native_nassp_resume PROPERTIES FIXTURES_REQUIRED nassp_solar_assets TIMEOUT 120)
  add_test(NAME wgpu_solar_cockpit COMMAND wgpu_cockpit_test "$<TARGET_FILE:WgpuClient>"
    "${PROJECT_BINARY_DIR}/runtime-solar" 8 --insertion)
  set_tests_properties(wgpu_solar_cockpit PROPERTIES LABELS "gpu;desktop"
    FIXTURES_REQUIRED nassp_solar_assets TIMEOUT 180)
  add_test(NAME nassp_stage_insertion COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tools/stage_nassp_smoke.py" "${PROJECT_BINARY_DIR}" ${_native_data_args} --insertion)
  set_tests_properties(nassp_stage_insertion PROPERTIES FIXTURES_SETUP nassp_insertion_assets)
  add_test(NAME wgpu_insertion_cockpit COMMAND wgpu_cockpit_test "$<TARGET_FILE:WgpuClient>"
    "${PROJECT_BINARY_DIR}/runtime" 8 --insertion)
  set_tests_properties(wgpu_insertion_cockpit PROPERTIES LABELS "gpu;desktop"
    FIXTURES_REQUIRED nassp_insertion_assets TIMEOUT 120)
endif()
add_executable(animation_pose_test "${PROJECT_SOURCE_DIR}/Tests/AnimationPose.cpp")
target_include_directories(animation_pose_test PRIVATE "${PROJECT_SOURCE_DIR}/Src/WgpuClient")
target_link_libraries(animation_pose_test PRIVATE OrbiterSDKHeaders)
add_test(NAME animation_pose COMMAND animation_pose_test)

if(ORBITER_BUILD_NASSP AND TARGET Saturn5)
  # Same thin host and renderer; only startup defaults differ from the CLI.
  add_executable(nassp_poc "${PROJECT_SOURCE_DIR}/Src/Orbiter/NativeMain.cpp")
  target_link_libraries(nassp_poc PRIVATE OrbiterSDL)
  target_compile_definitions(nassp_poc PRIVATE
    ORBITER_POC_ASSET_ROOT="${PROJECT_BINARY_DIR}/runtime-csm")
  set_target_properties(nassp_poc PROPERTIES OUTPUT_NAME "NASSP PoC")
  if(APPLE)
    set_target_properties(nassp_poc PROPERTIES MACOSX_BUNDLE TRUE
      MACOSX_BUNDLE_GUI_IDENTIFIER "local.orbiter.nassp-poc"
      MACOSX_BUNDLE_BUNDLE_NAME "NASSP PoC"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
      MACOSX_BUNDLE_BUNDLE_VERSION "1")
  endif()
  add_custom_target(stage_nassp_poc
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/stage_nassp_smoke.py"
      "${PROJECT_BINARY_DIR}" ${_native_mission_data_args} --csm
    DEPENDS nassp_poc WgpuClient Saturn5 NativeSun NativeEarth NativeMoon NativeEarthAtmosphere nassp_bitmap_resources
    COMMENT "Stage the local NASSP CSM cockpit app assets" VERBATIM)
  add_test(NAME nassp_poc_launch COMMAND nassp_poc --steps 3)
  set_tests_properties(nassp_poc_launch PROPERTIES LABELS "gpu;desktop"
    FIXTURES_REQUIRED nassp_csm_assets TIMEOUT 120)
endif()
add_executable(star_catalog_test "${PROJECT_SOURCE_DIR}/Tests/StarCatalog.cpp")
target_include_directories(star_catalog_test PRIVATE "${PROJECT_SOURCE_DIR}/Src/WgpuClient")
target_link_libraries(star_catalog_test PRIVATE OrbiterSDKHeaders)
add_test(NAME star_catalog COMMAND star_catalog_test "${PROJECT_SOURCE_DIR}/Config/CSphere/Data/star.bin")
add_executable(wgpu_stars_test "${PROJECT_SOURCE_DIR}/Tests/WgpuStars.cpp")
target_include_directories(wgpu_stars_test PRIVATE "${PROJECT_SOURCE_DIR}/Src/WgpuClient" "${PROJECT_SOURCE_DIR}/Orbitersdk/include")
target_link_libraries(wgpu_stars_test PRIVATE orbiter_wgpu)
add_test(NAME wgpu_stars COMMAND wgpu_stars_test)
set_tests_properties(wgpu_stars PROPERTIES LABELS "gpu")
add_executable(local_lights_test "${PROJECT_SOURCE_DIR}/Tests/LocalLights.cpp")
target_include_directories(local_lights_test PRIVATE "${PROJECT_SOURCE_DIR}/Src/WgpuClient")
target_link_libraries(local_lights_test PRIVATE OrbiterCore)
add_test(NAME local_lights COMMAND local_lights_test)
add_executable(exhaust_geometry_test "${PROJECT_SOURCE_DIR}/Tests/ExhaustGeometry.cpp")
target_include_directories(exhaust_geometry_test PRIVATE "${PROJECT_SOURCE_DIR}/Src/WgpuClient")
target_link_libraries(exhaust_geometry_test PRIVATE OrbiterSDKHeaders)
add_test(NAME exhaust_geometry COMMAND exhaust_geometry_test)
add_executable(terrain_texture_border_test "${PROJECT_SOURCE_DIR}/Tests/TerrainTextureBorder.cpp")
target_include_directories(terrain_texture_border_test PRIVATE "${PROJECT_SOURCE_DIR}/Src/WgpuClient")
add_test(NAME terrain_texture_border COMMAND terrain_texture_border_test)
if(ORBITER_BUILD_NASSP)
  add_executable(native_views_test "${PROJECT_SOURCE_DIR}/Tests/NativeViews.cpp")
  target_link_libraries(native_views_test PRIVATE OrbiterCore)
  add_dependencies(native_views_test WgpuClient)
  add_test(NAME native_views COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tests/NativeViews.py"
    "$<TARGET_FILE:native_views_test>" "$<TARGET_FILE:WgpuClient>" "${PROJECT_BINARY_DIR}/runtime-csm")
  set_tests_properties(native_views PROPERTIES FIXTURES_REQUIRED nassp_csm_assets LABELS "gpu" TIMEOUT 360)
endif()

if(TARGET LEM)
  add_test(NAME nassp_stage_lm COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/stage_nassp_smoke.py" "${PROJECT_BINARY_DIR}" ${_native_data_args} --lm)
  set_tests_properties(nassp_stage_lm PROPERTIES FIXTURES_SETUP nassp_lm_assets)
  add_executable(native_lm_test "${PROJECT_SOURCE_DIR}/Tests/NativeLM.cpp")
  target_link_libraries(native_lm_test PRIVATE OrbiterSDL)
  add_dependencies(native_lm_test LEM WgpuClient)
  if(APPLE)
    set_target_properties(native_lm_test PROPERTIES MACOSX_BUNDLE TRUE)
  endif()
  add_test(NAME native_lm COMMAND native_lm_test "$<TARGET_FILE:WgpuClient>" "${PROJECT_BINARY_DIR}/runtime-lm")
  set_tests_properties(native_lm PROPERTIES FIXTURES_REQUIRED nassp_lm_assets FIXTURES_SETUP nassp_lm_saved LABELS "gpu;desktop" TIMEOUT 180)
  add_test(NAME native_lm_resume COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tests/NativeLMResume.py" "$<TARGET_FILE:orbiter>" "${PROJECT_BINARY_DIR}/runtime-lm")
  set_tests_properties(native_lm_resume PROPERTIES FIXTURES_REQUIRED nassp_lm_saved TIMEOUT 180)
endif()

if(TARGET LEM AND TARGET Saturn5)
  add_test(NAME nassp_stage_pair COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/stage_nassp_smoke.py" "${PROJECT_BINARY_DIR}" ${_native_data_args} --pair)
  set_tests_properties(nassp_stage_pair PROPERTIES FIXTURES_SETUP nassp_pair_assets)
  add_executable(native_pair_test "${PROJECT_SOURCE_DIR}/Tests/NativePair.cpp")
  target_link_libraries(native_pair_test PRIVATE OrbiterSDL)
  add_dependencies(native_pair_test LEM Saturn5 WgpuClient)
  if(APPLE)
    set_target_properties(native_pair_test PROPERTIES MACOSX_BUNDLE TRUE)
  endif()
  add_test(NAME native_pair COMMAND native_pair_test "$<TARGET_FILE:WgpuClient>" "${PROJECT_BINARY_DIR}/runtime-pair")
  add_test(NAME native_exterior COMMAND native_pair_test "$<TARGET_FILE:WgpuClient>" "${PROJECT_BINARY_DIR}/runtime-pair" --exterior)
  set_tests_properties(native_exterior PROPERTIES FIXTURES_REQUIRED nassp_pair_assets LABELS "gpu;desktop" TIMEOUT 180)
  set_tests_properties(native_pair PROPERTIES FIXTURES_REQUIRED nassp_pair_assets FIXTURES_SETUP nassp_pair_saved LABELS "gpu;desktop" TIMEOUT 180)
  add_test(NAME native_pair_resume COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tests/NativePairResume.py" "$<TARGET_FILE:orbiter>" "${PROJECT_BINARY_DIR}/runtime-pair")
  set_tests_properties(native_pair_resume PROPERTIES FIXTURES_REQUIRED nassp_pair_saved TIMEOUT 180)
endif()

if(TARGET LEM)
  add_test(NAME nassp_stage_liftoff COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/stage_nassp_smoke.py" "${PROJECT_BINARY_DIR}" ${_native_data_args} --liftoff)
  set_tests_properties(nassp_stage_liftoff PROPERTIES FIXTURES_SETUP nassp_liftoff_assets)
  add_executable(native_liftoff_test "${PROJECT_SOURCE_DIR}/Tests/NativeLiftoff.cpp")
  target_link_libraries(native_liftoff_test PRIVATE OrbiterSDL)
  if(APPLE)
    set_target_properties(native_liftoff_test PROPERTIES MACOSX_BUNDLE TRUE)
  endif()
  add_dependencies(native_liftoff_test LEM WgpuClient)
  add_test(NAME native_liftoff COMMAND native_liftoff_test "$<TARGET_FILE:WgpuClient>" "${PROJECT_BINARY_DIR}/runtime-liftoff")
  set_tests_properties(native_liftoff PROPERTIES FIXTURES_REQUIRED nassp_liftoff_assets FIXTURES_SETUP nassp_liftoff_saved LABELS "gpu;desktop" TIMEOUT 180)
  add_test(NAME native_liftoff_resume COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tests/NativeLiftoffResume.py" "$<TARGET_FILE:orbiter>" "${PROJECT_BINARY_DIR}/runtime-liftoff")
  set_tests_properties(native_liftoff_resume PROPERTIES FIXTURES_REQUIRED nassp_liftoff_saved TIMEOUT 180)
endif()

add_executable(wgpu_imgui_test "${PROJECT_SOURCE_DIR}/Tests/WgpuImGui.cpp")
target_link_libraries(wgpu_imgui_test PRIVATE OrbiterCore)
add_dependencies(wgpu_imgui_test WgpuClient)
add_test(NAME wgpu_imgui COMMAND wgpu_imgui_test "$<TARGET_FILE:WgpuClient>")
set_tests_properties(wgpu_imgui PROPERTIES LABELS "gpu")
if(TARGET Saturn5)
  add_executable(native_ui_test "${PROJECT_SOURCE_DIR}/Tests/NativeUI.cpp")
  target_link_libraries(native_ui_test PRIVATE OrbiterSDL)
  add_dependencies(native_ui_test WgpuClient Saturn5)
  if(APPLE)
    set_target_properties(native_ui_test PROPERTIES MACOSX_BUNDLE TRUE)
  endif()
  add_test(NAME native_ui COMMAND native_ui_test "$<TARGET_FILE:WgpuClient>" "${PROJECT_BINARY_DIR}/runtime-csm")
  set_tests_properties(native_ui PROPERTIES FIXTURES_REQUIRED nassp_csm_assets LABELS "gpu;desktop" TIMEOUT 180)
endif()
if(TARGET Saturn5 AND TARGET LEM)
  add_executable(native_mfd_test "${PROJECT_SOURCE_DIR}/Tests/NativeMFD.cpp")
  target_link_libraries(native_mfd_test PRIVATE OrbiterSDL)
  add_dependencies(native_mfd_test WgpuClient Saturn5 LEM)
  if(APPLE)
    set_target_properties(native_mfd_test PROPERTIES MACOSX_BUNDLE TRUE)
  endif()
  add_test(NAME native_mfd COMMAND native_mfd_test "$<TARGET_FILE:WgpuClient>" "${PROJECT_BINARY_DIR}/runtime-pair")
  set_tests_properties(native_mfd PROPERTIES FIXTURES_REQUIRED nassp_pair_assets FIXTURES_SETUP nassp_mfd_saved LABELS "gpu;desktop" TIMEOUT 180)
endif()

if(TARGET native_mfd_test)
  add_test(NAME native_mfd_resume COMMAND native_mfd_test "$<TARGET_FILE:WgpuClient>" "${PROJECT_BINARY_DIR}/runtime-pair" --resume)
  set_tests_properties(native_mfd_resume PROPERTIES FIXTURES_REQUIRED nassp_mfd_saved LABELS "gpu;desktop" TIMEOUT 180)
endif()

if(TARGET ProjectApolloMFD AND TARGET nassp_poc)
  target_compile_definitions(nassp_poc PRIVATE ORBITER_POC_MFD)
  add_dependencies(nassp_poc ProjectApolloMFD)
endif()

if(TARGET ProjectApolloMFD)
  add_executable(nassp_mfd_test "${PROJECT_SOURCE_DIR}/Tests/NasspMFD.cpp")
  target_link_libraries(nassp_mfd_test PRIVATE OrbiterSDL)
  add_dependencies(nassp_mfd_test WgpuClient ProjectApolloMFD)
  if(APPLE)
    set_target_properties(nassp_mfd_test PROPERTIES MACOSX_BUNDLE TRUE)
  endif()
  add_test(NAME nassp_mfd COMMAND nassp_mfd_test "$<TARGET_FILE:WgpuClient>" "$<TARGET_FILE:ProjectApolloMFD>" "${PROJECT_BINARY_DIR}/runtime-pair")
  set_tests_properties(nassp_mfd PROPERTIES FIXTURES_REQUIRED nassp_pair_assets FIXTURES_SETUP nassp_addon_mfd_saved LABELS "gpu;desktop" TIMEOUT 180)
  add_test(NAME nassp_mfd_resume COMMAND nassp_mfd_test "$<TARGET_FILE:WgpuClient>" "$<TARGET_FILE:ProjectApolloMFD>" "${PROJECT_BINARY_DIR}/runtime-pair" --resume)
  set_tests_properties(nassp_mfd_resume PROPERTIES FIXTURES_REQUIRED nassp_addon_mfd_saved LABELS "gpu;desktop" TIMEOUT 180)
endif()

if(TARGET PanelSDK AND TARGET NASSPBitmapResources)
  add_executable(native_fdai_test "${PROJECT_SOURCE_DIR}/Tests/NativeFdai.cpp"
    "${NASSP_SOURCE_ROOT}/src_sys/FDAI.cpp" "${NASSP_SOURCE_ROOT}/src_native/FdaiRasterizer.cpp")
  target_compile_definitions(native_fdai_test PRIVATE NASSP_NATIVE)
  target_include_directories(native_fdai_test PRIVATE "${NASSP_SOURCE_ROOT}/src_sys" "${NASSP_SOURCE_ROOT}/src_native")
  target_link_libraries(native_fdai_test PRIVATE OrbiterCore PanelSDK NASSPBitmapResources)
  add_dependencies(native_fdai_test WgpuClient)
  add_test(NAME native_fdai COMMAND native_fdai_test "$<TARGET_FILE:WgpuClient>" "${PROJECT_BINARY_DIR}/runtime-pair")
  set_tests_properties(native_fdai PROPERTIES FIXTURES_REQUIRED nassp_pair_assets LABELS "gpu" TIMEOUT 60)
endif()

if(TARGET LEM AND TARGET Saturn5)
  add_test(NAME nassp_stage_joystick COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/stage_nassp_smoke.py" "${PROJECT_BINARY_DIR}" ${_native_data_args} --pair --output "${PROJECT_BINARY_DIR}/runtime-joystick")
  set_tests_properties(nassp_stage_joystick PROPERTIES FIXTURES_SETUP nassp_joystick_assets)
  add_executable(nassp_joystick_test "${PROJECT_SOURCE_DIR}/Tests/NasspJoystick.cpp")
  target_link_libraries(nassp_joystick_test PRIVATE OrbiterSDL)
  add_dependencies(nassp_joystick_test LEM Saturn5)
  add_test(NAME nassp_joystick COMMAND nassp_joystick_test "${PROJECT_BINARY_DIR}/runtime-joystick")
  set_tests_properties(nassp_joystick PROPERTIES FIXTURES_REQUIRED nassp_joystick_assets TIMEOUT 120)
endif()

if(TARGET Saturn5)
  add_test(NAME nassp_stage_joystick_flight COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/stage_nassp_smoke.py" "${PROJECT_BINARY_DIR}" ${_native_data_args} --csm --output "${PROJECT_BINARY_DIR}/runtime-joystick-flight")
  set_tests_properties(nassp_stage_joystick_flight PROPERTIES FIXTURES_SETUP nassp_joystick_flight_assets)
  add_executable(nassp_joystick_flight_test "${PROJECT_SOURCE_DIR}/Tests/NasspJoystickFlight.cpp")
  target_link_libraries(nassp_joystick_flight_test PRIVATE OrbiterSDL)
  add_dependencies(nassp_joystick_flight_test Saturn5)
  add_test(NAME nassp_joystick_flight COMMAND nassp_joystick_flight_test "${PROJECT_BINARY_DIR}/runtime-joystick-flight")
  set_tests_properties(nassp_joystick_flight PROPERTIES FIXTURES_REQUIRED nassp_joystick_flight_assets TIMEOUT 120)
endif()

if(TARGET nassp_joystick_test)
  add_test(NAME nassp_stage_joystick_throttle COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/stage_nassp_smoke.py" "${PROJECT_BINARY_DIR}" ${_native_data_args} --pair --output "${PROJECT_BINARY_DIR}/runtime-joystick-throttle")
  set_tests_properties(nassp_stage_joystick_throttle PROPERTIES FIXTURES_SETUP nassp_joystick_throttle_assets)
  add_test(NAME nassp_joystick_throttle COMMAND nassp_joystick_test "${PROJECT_BINARY_DIR}/runtime-joystick-throttle" --throttle)
  set_tests_properties(nassp_joystick_throttle PROPERTIES FIXTURES_REQUIRED nassp_joystick_throttle_assets TIMEOUT 120)
endif()

if(TARGET LEM AND TARGET Saturn5)
  add_executable(native_sound_test "${PROJECT_SOURCE_DIR}/Tests/NativeSound.cpp")
  target_link_libraries(native_sound_test PRIVATE OrbiterCore)
  add_dependencies(native_sound_test LEM Saturn5)
  add_test(NAME native_sound COMMAND native_sound_test "${CMAKE_BINARY_DIR}/runtime-pair")
  set_tests_properties(native_sound PROPERTIES FIXTURES_REQUIRED nassp_pair_assets TIMEOUT 120)
endif()

if(TARGET native_pair_test)
  add_test(NAME terrain_textures COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/Tests/TerrainTextures.py" "${PROJECT_BINARY_DIR}"
    "${PROJECT_SOURCE_DIR}/tools/stage_nassp_smoke.py"
    "$<TARGET_FILE:native_pair_test>" "$<TARGET_FILE:WgpuClient>" "${NASSP_DATA_DIR}")
  set_tests_properties(terrain_textures PROPERTIES LABELS "gpu;desktop" TIMEOUT 240)
endif()

if(TARGET native_liftoff_test)
  add_test(NAME terrain_contact COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/Tests/TerrainContact.py" "${PROJECT_BINARY_DIR}"
    "${PROJECT_SOURCE_DIR}/tools/stage_nassp_smoke.py"
    "$<TARGET_FILE:native_liftoff_test>" "$<TARGET_FILE:WgpuClient>" "${NASSP_DATA_DIR}")
  set_tests_properties(terrain_contact PROPERTIES LABELS "gpu;desktop" TIMEOUT 180)
endif()

add_executable(particle_simulation_test "${PROJECT_SOURCE_DIR}/Tests/ParticleSimulation.cpp")
target_include_directories(particle_simulation_test PRIVATE "${PROJECT_SOURCE_DIR}/Src/WgpuClient")
target_link_libraries(particle_simulation_test PRIVATE OrbiterCore)
add_test(NAME particle_simulation COMMAND particle_simulation_test)

add_executable(sun_occlusion_test "${PROJECT_SOURCE_DIR}/Tests/SunOcclusion.cpp")
target_include_directories(sun_occlusion_test PRIVATE "${PROJECT_SOURCE_DIR}/Src/WgpuClient")
target_link_libraries(sun_occlusion_test PRIVATE OrbiterSDKHeaders)
add_test(NAME sun_occlusion COMMAND sun_occlusion_test)

add_executable(landing_sounds_test "${PROJECT_SOURCE_DIR}/Tests/LandingSounds.cpp")
target_include_directories(landing_sounds_test PRIVATE "${NASSP_SOURCE_ROOT}/src_native")
add_test(NAME landing_sounds COMMAND landing_sounds_test "${NASSP_DATA_DIR}/Sound/ProjectApollo")

if(TARGET LEM)
  add_executable(landing_sound_playback_test "${PROJECT_SOURCE_DIR}/Tests/LandingSoundPlayback.cpp")
  target_include_directories(landing_sound_playback_test PRIVATE
    "${NASSP_SOURCE_ROOT}/src_sys"
    "${NASSP_SOURCE_ROOT}/src_native")
  target_compile_definitions(landing_sound_playback_test PRIVATE NASSP_NATIVE)
  target_link_libraries(landing_sound_playback_test PRIVATE LEM OrbiterCore)
  add_test(NAME landing_sound_playback COMMAND landing_sound_playback_test "${CMAKE_BINARY_DIR}/runtime-pair")
  set_tests_properties(landing_sound_playback PROPERTIES FIXTURES_REQUIRED nassp_pair_assets TIMEOUT 120)
endif()

if(TARGET LEM)
  add_executable(native_descent_probe "${PROJECT_SOURCE_DIR}/Tests/NativeDescent.cpp")
  target_link_libraries(native_descent_probe PRIVATE OrbiterCore LEM)
  target_include_directories(native_descent_probe PRIVATE $<TARGET_PROPERTY:LEM,INCLUDE_DIRECTORIES>)
  target_compile_definitions(native_descent_probe PRIVATE NASSP_NATIVE OPENORBITER)
  add_dependencies(native_descent_probe LEM)
endif()

add_test(NAME pdi_protocol_contract COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/Tests/NativePDIProtocolContract.py")
set_tests_properties(pdi_protocol_contract PROPERTIES ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1" TIMEOUT 30)
add_test(NAME pdi_pilot_contract COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/Tests/NativePDIPilotContract.py")
set_tests_properties(pdi_pilot_contract PROPERTIES ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1" TIMEOUT 30)
add_test(NAME pdi_aca_contract COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/Tests/NativePDIACAContract.py")
set_tests_properties(pdi_aca_contract PROPERTIES ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1" TIMEOUT 30)

if(TARGET LEM)
  add_executable(native_pdi_protocol_probe "${PROJECT_SOURCE_DIR}/Tests/NativePDIProtocol.cpp")
  target_link_libraries(native_pdi_protocol_probe PRIVATE OrbiterSDL LEM)
  target_include_directories(native_pdi_protocol_probe PRIVATE $<TARGET_PROPERTY:LEM,INCLUDE_DIRECTORIES>)
  target_compile_definitions(native_pdi_protocol_probe PRIVATE NASSP_NATIVE OPENORBITER)
  add_dependencies(native_pdi_protocol_probe LEM)
  add_executable(native_lm_persistence_test "${PROJECT_SOURCE_DIR}/Tests/NativeLMPersistence.cpp")
  target_link_libraries(native_lm_persistence_test PRIVATE OrbiterCore LEM)
  target_include_directories(native_lm_persistence_test PRIVATE $<TARGET_PROPERTY:LEM,INCLUDE_DIRECTORIES>)
  target_compile_definitions(native_lm_persistence_test PRIVATE NASSP_NATIVE OPENORBITER)
  add_test(NAME native_lm_persistence COMMAND native_lm_persistence_test "${PROJECT_BINARY_DIR}/runtime-lm-persistence")
  set_tests_properties(native_lm_persistence PROPERTIES TIMEOUT 30)

endif()

if(TARGET EasepPSEP AND TARGET native_liftoff_test)
  set(_original_liftoff "Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 11 - Lunar Liftoff T-1m.scn")
  add_test(NAME nassp_stage_original_liftoff COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tools/stage_nassp_mission.py" "${PROJECT_BINARY_DIR}"
    ${_native_mission_data_args} --scenario "${_original_liftoff}" --output "${PROJECT_BINARY_DIR}/runtime-original-liftoff")
  set_tests_properties(nassp_stage_original_liftoff PROPERTIES FIXTURES_SETUP nassp_original_liftoff)
  add_test(NAME native_original_liftoff COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/Tests/NativeOriginalLiftoff.py" "$<TARGET_FILE:orbiter>"
    "$<TARGET_FILE:native_liftoff_test>" "$<TARGET_FILE:WgpuClient>"
    "${PROJECT_BINARY_DIR}/runtime-original-liftoff"
    "${NASSP_DATA_DIR}/Scenarios/Project Apollo - NASSP/${_original_liftoff}")
  set_tests_properties(native_original_liftoff PROPERTIES FIXTURES_REQUIRED nassp_original_liftoff LABELS "gpu;desktop" TIMEOUT 300)
endif()

if(TARGET MCC)
  add_executable(native_mcc_test "${PROJECT_SOURCE_DIR}/Tests/NativeMCC.cpp")
  target_link_libraries(native_mcc_test PRIVATE OrbiterSDL)
  add_dependencies(native_mcc_test MCC WgpuClient)
  set_target_properties(native_mcc_test PROPERTIES MACOSX_BUNDLE TRUE)
  add_test(NAME native_mcc COMMAND native_mcc_test "$<TARGET_FILE:WgpuClient>" "${PROJECT_BINARY_DIR}/runtime-original-insertion")
  add_test(NAME native_mcc_resume COMMAND native_mcc_test "$<TARGET_FILE:WgpuClient>" "${PROJECT_BINARY_DIR}/runtime-original-insertion" --resume)
  set_tests_properties(native_mcc_resume PROPERTIES DEPENDS native_mcc FIXTURES_REQUIRED nassp_mcc_save LABELS "gpu;desktop" TIMEOUT 120)
  set_tests_properties(native_mcc PROPERTIES FIXTURES_SETUP nassp_mcc_save FIXTURES_REQUIRED nassp_original_insertion LABELS "gpu;desktop" TIMEOUT 120)
endif()

if(TARGET native_descent_probe AND TARGET MCC)
  set(_original_descent "Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 10 - PDI T-15m.scn")
  add_test(NAME nassp_stage_original_descent COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tools/stage_nassp_mission.py" "${PROJECT_BINARY_DIR}"
    ${_native_mission_data_args} --scenario "${_original_descent}" --output "${PROJECT_BINARY_DIR}/runtime-original-descent")
  set_tests_properties(nassp_stage_original_descent PROPERTIES FIXTURES_SETUP nassp_original_descent)
  add_test(NAME native_original_descent COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tests/NativeOriginalDescent.py"
    "$<TARGET_FILE:native_descent_probe>" "$<TARGET_FILE:orbiter>" "${PROJECT_BINARY_DIR}/runtime-original-descent"
    "${NASSP_DATA_DIR}/Scenarios/Project Apollo - NASSP/${_original_descent}")
  set_tests_properties(native_original_descent PROPERTIES FIXTURES_REQUIRED nassp_original_descent FIXTURES_SETUP nassp_descent_save TIMEOUT 600)
endif()

if(TARGET native_descent_probe AND TARGET MCC)
  add_executable(native_descent_visual_test "${PROJECT_SOURCE_DIR}/Tests/NativeDescentVisual.cpp")
  target_link_libraries(native_descent_visual_test PRIVATE OrbiterSDL)
  add_dependencies(native_descent_visual_test WgpuClient LEM)
  set_target_properties(native_descent_visual_test PROPERTIES MACOSX_BUNDLE TRUE)
  add_test(NAME native_descent_visual COMMAND native_descent_visual_test "$<TARGET_FILE:WgpuClient>" "${PROJECT_BINARY_DIR}/runtime-original-descent")
  set_tests_properties(native_descent_visual PROPERTIES FIXTURES_REQUIRED nassp_descent_save LABELS "gpu;desktop" TIMEOUT 120)
endif()

# Separate observational checkpoint; preserves the original 2000-ft diagnostic.
if(TARGET native_pdi_protocol_probe AND TARGET MCC)
  set(_pdi_protocol_scenario "Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 10 - PDI T-15m.scn")
  add_test(NAME nassp_stage_pdi_protocol COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tools/stage_nassp_mission.py" "${PROJECT_BINARY_DIR}"
    ${_native_mission_data_args} --scenario "${_pdi_protocol_scenario}" --output "${PROJECT_BINARY_DIR}/runtime-pdi-protocol")
  set_tests_properties(nassp_stage_pdi_protocol PROPERTIES FIXTURES_SETUP nassp_pdi_protocol)
  add_test(NAME native_pdi_protocol COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/Tests/NativePDIProtocol.py"
    "$<TARGET_FILE:native_pdi_protocol_probe>" "$<TARGET_FILE:orbiter>" "${PROJECT_BINARY_DIR}/runtime-pdi-protocol"
    "${NASSP_DATA_DIR}/Scenarios/Project Apollo - NASSP/${_pdi_protocol_scenario}")
  set_tests_properties(native_pdi_protocol PROPERTIES FIXTURES_REQUIRED nassp_pdi_protocol
    RUN_SERIAL TRUE ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1" LABELS "simulation" TIMEOUT 600)
  # Separate original proportional pilot inputs and the published radar gate.
  add_test(NAME nassp_stage_pdi_pilot COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tools/stage_nassp_mission.py" "${PROJECT_BINARY_DIR}"
    ${_native_mission_data_args} --scenario "${_pdi_protocol_scenario}" --output "${PROJECT_BINARY_DIR}/runtime-pdi-pilot")
  set_tests_properties(nassp_stage_pdi_pilot PROPERTIES FIXTURES_SETUP nassp_pdi_pilot)
  add_test(NAME native_pdi_pilot COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/Tests/NativePDIPilot.py"
    "$<TARGET_FILE:native_pdi_protocol_probe>" "$<TARGET_FILE:orbiter>" "${PROJECT_BINARY_DIR}/runtime-pdi-pilot"
    "${NASSP_DATA_DIR}/Scenarios/Project Apollo - NASSP/${_pdi_protocol_scenario}")
  set_tests_properties(native_pdi_pilot PROPERTIES FIXTURES_REQUIRED nassp_pdi_pilot
    RUN_SERIAL TRUE ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1" LABELS "simulation" TIMEOUT 600)
  add_test(NAME nassp_stage_pdi_uninterrupted_aca COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tools/stage_nassp_mission.py" "${PROJECT_BINARY_DIR}"
    ${_native_mission_data_args} --scenario "${_pdi_protocol_scenario}" --output "${PROJECT_BINARY_DIR}/runtime-pdi-uninterrupted-aca")
  set_tests_properties(nassp_stage_pdi_uninterrupted_aca PROPERTIES FIXTURES_SETUP nassp_pdi_uninterrupted_aca)
  add_test(NAME native_pdi_uninterrupted_aca COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/Tests/NativePDIUninterruptedACA.py"
    "$<TARGET_FILE:native_pdi_protocol_probe>" "${PROJECT_BINARY_DIR}/runtime-pdi-uninterrupted-aca"
    "${NASSP_DATA_DIR}/Scenarios/Project Apollo - NASSP/${_pdi_protocol_scenario}")
  set_tests_properties(native_pdi_uninterrupted_aca PROPERTIES FIXTURES_REQUIRED nassp_pdi_uninterrupted_aca
    RUN_SERIAL TRUE ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1" LABELS "simulation" TIMEOUT 600)
  if(TARGET WgpuClient AND TARGET ProjectApolloMFD)
    add_dependencies(native_pdi_protocol_probe WgpuClient ProjectApolloMFD)
    add_test(NAME native_pdi_window COMMAND "${Python3_EXECUTABLE}"
      "${PROJECT_SOURCE_DIR}/tools/nassp_pdi_window.py" --build "${PROJECT_BINARY_DIR}" --self-test --repeat 2)
    set_tests_properties(native_pdi_window PROPERTIES RUN_SERIAL TRUE
      ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1" LABELS "gpu;desktop;simulation" TIMEOUT 2400)
  endif()
endif()
