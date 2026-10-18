project(lua)

Include(FetchContent)

FetchContent_Declare(
  lua
  GIT_REPOSITORY https://github.com/lua/lua.git
  GIT_TAG 69ea087dff1daba25a2000dfb8f1883c17545b7a # v5.1
)
FetchContent_MakeAvailable(lua)

file(GLOB LUA_SRC_FILES "${lua_SOURCE_DIR}/*.c")
file(GLOB LUA_HDR_FILES "${lua_SOURCE_DIR}/*.h")

list(REMOVE_ITEM LUA_SRC_FILES "${lua_SOURCE_DIR}/lua.c")
list(REMOVE_ITEM LUA_SRC_FILES "${lua_SOURCE_DIR}/luac.c")
list(REMOVE_ITEM LUA_SRC_FILES "${lua_SOURCE_DIR}/print.c")

# Native hosts and addons share one Lua runtime. The command-line interpreter
# and the legacy install layout are not required by this build.
if(TARGET OrbiterCore)
  add_library(lua SHARED ${LUA_SRC_FILES})
  target_include_directories(lua PUBLIC "${lua_SOURCE_DIR}")
  if(WIN32)
    target_compile_definitions(lua PRIVATE LUA_BUILD_AS_DLL)
    set_target_properties(lua PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
  else()
    target_link_libraries(lua PRIVATE m ${CMAKE_DL_LIBS})
  endif()
  add_library(lua::lib ALIAS lua)
  return()
endif()

# Executable - lua.exe
add_executable(lua-exe ${LUA_SRC_FILES} "${lua_SOURCE_DIR}/lua.c")
set_target_properties(lua-exe PROPERTIES OUTPUT_NAME lua)

set_target_properties(lua-exe PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
)

install(TARGETS lua-exe RUNTIME DESTINATION ${ORBITER_INSTALL_ROOT_DIR})
install(TARGETS lua-exe DESTINATION ${ORBITER_INSTALL_SDK_DIR}/lib/Lua)
install(TARGETS lua-exe DESTINATION ${ORBITER_INSTALL_SDK_DIR}/Utils/)

# Library - lua.dll
add_library(lua SHARED ${LUA_SRC_FILES})

if(MSVC)
  target_compile_definitions(lua PUBLIC LUA_BUILD_AS_DLL)
else()
  target_link_libraries(lua-exe PUBLIC m)
  target_link_libraries(lua PUBLIC m)
endif()

set_target_properties(lua PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
)
install(TARGETS lua DESTINATION ${ORBITER_INSTALL_ROOT_DIR})

set(LUA_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/include)
file(COPY ${LUA_HDR_FILES} DESTINATION ${LUA_INCLUDE_DIR})
target_include_directories(lua INTERFACE ${LUA_INCLUDE_DIR})

# Aliases for easier reference
add_executable(lua::exe ALIAS lua-exe)
add_library(lua::lib ALIAS lua)
