include(FetchContent)
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG 3912b3d9a9c1b3f17431aebafd86d2f40ee6e59c) # v1.92.5-docking
FetchContent_MakeAvailable(imgui)
add_library(NativeImGuiHeaders INTERFACE)
target_include_directories(NativeImGuiHeaders INTERFACE "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends" "${PROJECT_SOURCE_DIR}/Src/Orbiter")
target_compile_definitions(NativeImGuiHeaders INTERFACE IMGUI_USER_CONFIG="NativeImGuiConfig.h")
# Compile ImGui once into liborbiter, so the host, SDL adapter and graphics plugin
# share its context, allocators and ABI instead of carrying independent globals.
target_sources(OrbiterCore PRIVATE
  "${imgui_SOURCE_DIR}/imgui.cpp" "${imgui_SOURCE_DIR}/imgui_draw.cpp"
  "${imgui_SOURCE_DIR}/imgui_tables.cpp" "${imgui_SOURCE_DIR}/imgui_widgets.cpp")
target_link_libraries(OrbiterCore PUBLIC NativeImGuiHeaders)
target_compile_definitions(OrbiterCore PRIVATE IMGUI_BUILD_CORE)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
file(GLOB native_font_sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/Extern/NativeFonts/*.ttf")
set(native_font_header "${PROJECT_BINARY_DIR}/generated/NativeFontData.h")
add_custom_command(OUTPUT "${native_font_header}"
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/embed_native_fonts.py"
    "${native_font_header}" "${PROJECT_SOURCE_DIR}/Extern/NativeFonts"
  DEPENDS ${native_font_sources} "${PROJECT_SOURCE_DIR}/tools/embed_native_fonts.py"
  VERBATIM)
target_sources(OrbiterCore PRIVATE "${PROJECT_SOURCE_DIR}/Src/Orbiter/NativeImGui.cpp" "${native_font_header}")
target_include_directories(OrbiterCore PRIVATE "${PROJECT_BINARY_DIR}/generated")
