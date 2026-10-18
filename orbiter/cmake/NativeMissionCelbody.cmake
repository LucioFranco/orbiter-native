# Original NASSP Sol_VirtualAGC needs the full configured planetary system.
foreach(family Galsat Satsat)
  if(family STREQUAL "Galsat")
    set(theory Lieske)
    set(moons Io Europa Ganymede Callisto)
  else()
    set(theory Tass17)
    set(moons Mimas Enceladus Tethys Dione Rhea Titan)
  endif()
  add_library(Native${family} SHARED "${native_celbody_root}/${family}/${family}.cpp" "${native_celbody_root}/${family}/${theory}.cpp")
  target_link_libraries(Native${family} PUBLIC OrbiterCore)
  target_include_directories(Native${family} PUBLIC "${native_celbody_root}/${family}")
  target_compile_definitions(Native${family} PRIVATE "InitModule=orbiter_native_init_${family}")
  set_target_properties(Native${family} PROPERTIES OUTPUT_NAME "${family}" LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/runtime/Modules")
  foreach(body IN LISTS moons)
    add_library(Native${body} MODULE "${native_celbody_root}/${family}/${body}/${body}.cpp")
    target_link_libraries(Native${body} PRIVATE Native${family})
    set_target_properties(Native${body} PROPERTIES PREFIX "" SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}" OUTPUT_NAME "${body}"
      LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/runtime/Modules/Celbody")
  endforeach()
endforeach()

foreach(body Mercury Venus Mars Jupiter Saturn Uranus Neptune)
  add_library(Native${body} MODULE "${native_celbody_root}/Vsop87/${body}/${body}.cpp")
  target_link_libraries(Native${body} PRIVATE NativeVsop)
  if(body STREQUAL "Jupiter" OR body STREQUAL "Saturn")
    if(body STREQUAL "Jupiter")
      set(family Galsat)
    else()
      set(family Satsat)
    endif()
    target_link_libraries(Native${body} PRIVATE Native${family})
    # The shared theory's InitModule was formerly supplied by Win32 DllMain.
    # Initialise it explicitly before the parent planet/its moons are created.
    set_source_files_properties("${native_celbody_root}/Vsop87/${body}/${body}.cpp" PROPERTIES COMPILE_DEFINITIONS "InitModule=orbiter_unused_init_${body}")
    configure_file("${native_celbody_root}/NativeDependency.cpp.in"
      "${PROJECT_BINARY_DIR}/generated/${body}Dependency.cpp" @ONLY)
    target_sources(Native${body} PRIVATE "${PROJECT_BINARY_DIR}/generated/${body}Dependency.cpp")
  endif()
  set_target_properties(Native${body} PROPERTIES PREFIX "" SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}" OUTPUT_NAME "${body}"
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/runtime/Modules/Celbody")
endforeach()
add_library(NativeMarsAtmosphere MODULE "${native_celbody_root}/Vsop87/Mars/Atmosphere/MarsAtm2006/MarsAtm2006.cpp")
target_link_libraries(NativeMarsAtmosphere PRIVATE OrbiterCore)
set_target_properties(NativeMarsAtmosphere PROPERTIES PREFIX "" SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}"
  OUTPUT_NAME "MarsAtm2006" LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/runtime/Modules/Celbody/Mars/Atmosphere")

add_library(NativeVenusAtmosphere MODULE "${native_celbody_root}/Vsop87/Venus/Atmosphere/VenusAtm2006/VenusAtm2006.cpp")
target_link_libraries(NativeVenusAtmosphere PRIVATE OrbiterCore)
set_target_properties(NativeVenusAtmosphere PROPERTIES PREFIX "" SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}"
  OUTPUT_NAME "VenusAtm2006" LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/runtime/Modules/Celbody/Venus/Atmosphere")
