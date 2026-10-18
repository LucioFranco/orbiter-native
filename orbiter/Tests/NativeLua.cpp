extern "C" {
#include "Lua/lua.h"
#include "Lua/lauxlib.h"
#include "Lua/lualib.h"
}
#include <cstdio>

static int setting(lua_State *state)
{
    lua_pushnumber(state, luaL_checknumber(state, 1) * 2);
    return 1;
}

int main()
{
    lua_State *state = luaL_newstate();
    if (!state) return 1;
    luaL_openlibs(state);
    const luaL_Reg functions[] = {{"setting", setting}, {nullptr, nullptr}};
    luaL_openlib(state, "vessel", functions, 0);
    lua_settop(state, 0);
    int result = luaL_dostring(state,
        "assert(type(vessel.setting) == 'function'); return vessel.setting(21)");
    if (result || lua_tonumber(state, -1) != 42) {
        std::fprintf(stderr, "Lua runtime/SDK callback test failed: %s\n",
                     lua_tostring(state, -1));
        lua_close(state);
        return 1;
    }
    lua_settop(state, 0);
    result = luaL_dostring(state, "vessel.setting({})");
    const bool caughtError = result != 0 && lua_isstring(state, -1);
    lua_close(state);
    return caughtError ? 0 : 1;
}
