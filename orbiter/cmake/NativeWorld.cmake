# The original world/vessel implementation, linked to the native application host.
set(native_world_sources
  Terrain/ElevationTile.cpp Body.cpp Rigidbody.cpp BodyIntegrator.cpp PinesGrav.cpp Celbody.cpp
  Planet.cpp Star.cpp Psys.cpp Vesselbase.cpp Vessel.cpp Vesselstatus.cpp
  SuperVessel.cpp Mesh.cpp D3dmath.cpp Nav.cpp Base.cpp Baseobj.cpp
  FlightRecorder.cpp LightEmitter.cpp elevmgr.cpp ZTreeMgr.cpp Util.cpp
  Keymap.cpp ModuleAPI.cpp Shadow.cpp CamAPI.cpp WorldAPI.cpp AnnotationAPI.cpp SurfaceAPI.cpp MeshAPI.cpp VesselLifecycleAPI.cpp CockpitAPI.cpp DrawAPI.cpp NativeCockpit.cpp NativeMFD.cpp NativeDialogs.cpp Mfd.cpp MfdOrbit.cpp MfdSurface.cpp MfdUser.cpp Select.cpp Camera.cpp FlightInput.cpp FileAPI.cpp ParticleStream.cpp NativeHost.cpp)
list(TRANSFORM native_world_sources PREPEND "${PROJECT_SOURCE_DIR}/Src/Orbiter/")
add_library(OrbiterWorld OBJECT ${native_world_sources})
target_link_libraries(OrbiterWorld PRIVATE OrbiterSDKHeaders NativeImGuiHeaders)
target_include_directories(OrbiterWorld PRIVATE "${PROJECT_SOURCE_DIR}/Src/Orbiter")
target_compile_definitions(OrbiterWorld PRIVATE ORBITER_NATIVE_CORE NOGRAPHICS OAPI_IMPLEMENTATION IMGUI_BUILD_CORE)
set_target_properties(OrbiterWorld PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_sources(OrbiterCore PRIVATE $<TARGET_OBJECTS:OrbiterWorld>)
target_compile_definitions(OrbiterCore PUBLIC ORBITER_NATIVE_CORE NOGRAPHICS)
if(WIN32)
  # The standard Windows runner has no zlib development package. Use the
  # stock v1.2.11 source pin and build only the static library: upstream's
  # CMakeLists renames its tracked zconf.h in the source checkout.
  include(FetchContent)
  FetchContent_Declare(zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG cacf7f1d4e3d44d871b605da3b647f07d718623f
    SOURCE_SUBDIR orbiter-no-upstream-cmake)
  FetchContent_MakeAvailable(zlib)
  set(native_zlib_sources
    adler32.c compress.c crc32.c deflate.c gzclose.c gzlib.c gzread.c
    gzwrite.c inflate.c infback.c inftrees.c inffast.c trees.c uncompr.c zutil.c)
  list(TRANSFORM native_zlib_sources PREPEND "${zlib_SOURCE_DIR}/")
  add_library(orbiter_native_zlib STATIC ${native_zlib_sources})
  target_include_directories(orbiter_native_zlib PUBLIC "${zlib_SOURCE_DIR}")
  add_library(ZLIB::ZLIB ALIAS orbiter_native_zlib)
else()
  find_package(ZLIB REQUIRED)
endif()
target_link_libraries(OrbiterCore PRIVATE ZLIB::ZLIB)
add_executable(orbiter "${PROJECT_SOURCE_DIR}/Src/Orbiter/NativeMain.cpp")
target_link_libraries(orbiter PRIVATE OrbiterCore)
# Optional manual comparison driver; separate from the validated mission suites.
add_executable(native_parity_probe "${PROJECT_SOURCE_DIR}/Tests/NativeParity.cpp")
target_link_libraries(native_parity_probe PRIVATE OrbiterCore)
add_executable(native_world_test "${PROJECT_SOURCE_DIR}/Tests/NativeWorld.cpp")
target_link_libraries(native_world_test PRIVATE OrbiterCore)
add_test(NAME native_world COMMAND native_world_test "${PROJECT_SOURCE_DIR}/Tests/Fixtures/NativeWorld")
add_executable(native_cockpit_test "${PROJECT_SOURCE_DIR}/Tests/NativeCockpit.cpp")
target_link_libraries(native_cockpit_test PRIVATE OrbiterCore)
add_test(NAME native_cockpit COMMAND native_cockpit_test)
add_executable(mesh_normals_test "${PROJECT_SOURCE_DIR}/Tests/MeshNormals.cpp")
target_link_libraries(mesh_normals_test PRIVATE OrbiterCore)
add_test(NAME mesh_normals COMMAND mesh_normals_test)

add_executable(tile_archive_test "${PROJECT_SOURCE_DIR}/Tests/TileArchive.cpp")
target_link_libraries(tile_archive_test PRIVATE OrbiterCore ZLIB::ZLIB)
add_test(NAME tile_archive COMMAND tile_archive_test "${CMAKE_BINARY_DIR}/tile-archive-test")

add_executable(elevation_tile_test "${PROJECT_SOURCE_DIR}/Tests/ElevationTile.cpp")
target_link_libraries(elevation_tile_test PRIVATE OrbiterCore ZLIB::ZLIB)
add_test(NAME elevation_tile COMMAND elevation_tile_test "${PROJECT_SOURCE_DIR}/Tests/Fixtures/NativeWorld")
