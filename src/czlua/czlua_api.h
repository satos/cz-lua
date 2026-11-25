#pragma once
// Registers the `czlua` global table (graphics / keyboard / system /
// files) into the engine's lua_State. All functions receive the engine
// pointer as a light-userdata upvalue.

struct lua_State;

namespace czlua {
class LuaEngine;
void registerCzlua(lua_State *L, LuaEngine *engine);
} // namespace czlua
