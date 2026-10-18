include(FetchContent)
FetchContent_Declare(miniaudio
  GIT_REPOSITORY https://github.com/mackron/miniaudio.git
  GIT_TAG 9634bedb5b5a2ca38c1ee7108a9358a4e233f14d # 0.11.25
  SOURCE_SUBDIR orbiter-header-only)
FetchContent_MakeAvailable(miniaudio)
include("${PROJECT_SOURCE_DIR}/cmake/PatchMiniaudio.cmake")
target_sources(OrbiterCore PRIVATE
  "${PROJECT_SOURCE_DIR}/Src/Orbiter/NativeSound.cpp"
  "${PROJECT_SOURCE_DIR}/Src/Orbiter/Platform/Audio.cpp"
  "${PROJECT_SOURCE_DIR}/Src/Orbiter/Platform/Miniaudio.cpp")
target_include_directories(OrbiterCore PRIVATE "${miniaudio_private_include}")
find_package(Threads REQUIRED)
target_link_libraries(OrbiterCore PRIVATE Threads::Threads)
if(APPLE)
  target_link_libraries(OrbiterCore PRIVATE "-framework CoreAudio" "-framework AudioToolbox" "-framework CoreFoundation")
endif()
add_executable(native_audio_test "${PROJECT_SOURCE_DIR}/Tests/NativeAudio.cpp")
target_link_libraries(native_audio_test PRIVATE OrbiterCore)
add_test(NAME native_audio COMMAND native_audio_test "${CMAKE_BINARY_DIR}/audio-test" "${NASSP_DATA_DIR}/Sound/ProjectApollo")

# Manual hardware probe; not part of CTest because CI may have no output device.
add_executable(native_audio_device_test "${PROJECT_SOURCE_DIR}/Tests/NativeAudioDevice.cpp")
target_link_libraries(native_audio_device_test PRIVATE OrbiterCore Threads::Threads)
