include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG 535d80badefc83c5c527ec5748f2a20d6a9310fe) # release-3.2.0
FetchContent_MakeAvailable(SDL3)
add_library(OrbiterSDL STATIC "${PROJECT_SOURCE_DIR}/Src/Orbiter/Platform/SDLJoystick.cpp" "${PROJECT_SOURCE_DIR}/Src/Orbiter/Platform/SDLWindow.cpp" "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp")
target_link_libraries(OrbiterSDL PUBLIC OrbiterCore SDL3::SDL3)
target_compile_definitions(OrbiterSDL PUBLIC SDL_DISABLE_OLD_NAMES)
target_link_libraries(orbiter PRIVATE OrbiterSDL)
add_executable(native_sdl_test "${PROJECT_SOURCE_DIR}/Tests/NativeSDL.cpp")
target_link_libraries(native_sdl_test PRIVATE OrbiterSDL)
add_test(NAME native_sdl COMMAND native_sdl_test)
add_test(NAME native_cli COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/Tests/NativeCLI.py" "$<TARGET_FILE:orbiter>"
  "${PROJECT_SOURCE_DIR}/Tests/Fixtures/NativeWorld")
add_executable(realtime_steps_test "${PROJECT_SOURCE_DIR}/Tests/RealtimeSteps.cpp")
target_link_libraries(realtime_steps_test PRIVATE OrbiterCore)
add_test(NAME realtime_steps COMMAND realtime_steps_test)

add_executable(native_joystick_test "${PROJECT_SOURCE_DIR}/Tests/NativeJoystick.cpp")
target_link_libraries(native_joystick_test PRIVATE OrbiterSDL)
add_test(NAME native_joystick COMMAND native_joystick_test)

if(ORBITER_BUILD_NASSP)
  add_executable(native_vesim_test "${PROJECT_SOURCE_DIR}/Tests/NativeVesim.cpp"
    "${NASSP_SOURCE_ROOT}/src_native/NativeVesim.cpp")
  target_include_directories(native_vesim_test PRIVATE
    "${NASSP_SOURCE_ROOT}/src_aux" "${NASSP_SOURCE_ROOT}/src_native")
  target_compile_definitions(native_vesim_test PRIVATE NASSP_NATIVE)
  target_link_libraries(native_vesim_test PRIVATE OrbiterCore)
  add_test(NAME native_vesim COMMAND native_vesim_test "${CMAKE_BINARY_DIR}/vesim-test.cfg")
endif()
