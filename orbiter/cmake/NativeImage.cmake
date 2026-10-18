include(FetchContent)
FetchContent_Declare(stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20)
FetchContent_MakeAvailable(stb)
target_sources(OrbiterCore PRIVATE "${PROJECT_SOURCE_DIR}/Src/Orbiter/Platform/Image.cpp")
target_include_directories(OrbiterCore PRIVATE "${stb_SOURCE_DIR}")
