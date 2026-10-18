# Rebuild the existing ephemeris implementations for the native shared core.
set(native_celbody_root "${PROJECT_SOURCE_DIR}/Src/Celbody")
add_library(NativeVsop STATIC "${native_celbody_root}/Vsop87/Vsop87.cpp")
set_target_properties(NativeVsop PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(NativeVsop PUBLIC OrbiterCore)
target_include_directories(NativeVsop PUBLIC "${native_celbody_root}/Vsop87" PRIVATE "${PROJECT_SOURCE_DIR}/Src/Orbiter")
foreach(body Sun Earth Moon)
  if(body STREQUAL "Moon")
    set(sources "${native_celbody_root}/Moon/Moon.cpp" "${native_celbody_root}/Moon/ELP82.cpp")
  else()
    set(sources "${native_celbody_root}/Vsop87/${body}/${body}.cpp")
  endif()
  add_library(Native${body} MODULE ${sources})
  target_link_libraries(Native${body} PRIVATE NativeVsop)
  target_include_directories(Native${body} PRIVATE "${PROJECT_SOURCE_DIR}/Src/Orbiter")
  set_target_properties(Native${body} PROPERTIES PREFIX "" SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}" OUTPUT_NAME "${body}"
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/runtime/Modules/Celbody")
endforeach()

add_library(NativeEarthAtmosphere MODULE "${native_celbody_root}/Vsop87/Earth/Atmosphere/EarthAtmJ71G/EarthAtmJ71G.cpp")
target_link_libraries(NativeEarthAtmosphere PRIVATE OrbiterCore)
set_target_properties(NativeEarthAtmosphere PROPERTIES PREFIX "" SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}"
  OUTPUT_NAME "EarthAtmJ71G" LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/runtime/Modules/Celbody/Earth/Atmosphere")

include("${PROJECT_SOURCE_DIR}/cmake/NativeMissionCelbody.cmake")
