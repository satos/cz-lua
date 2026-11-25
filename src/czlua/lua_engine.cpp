#include "lua_engine.h"

#include <cstdlib>

#include "czlua_api.h"
#include "gfx.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace czlua {

// ---------------------------------------------------------------- allocator
void *LuaEngine::alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
  auto *self = static_cast<LuaEngine *>(ud);
  if (nsize == 0) {
    self->memUsed_ -= osize;
    std::free(ptr);
    return nullptr;
  }
  if (self->memUsed_ - osize + nsize > self->memCap_) return nullptr; // OOM -> Lua error
  void *np = std::realloc(ptr, nsize);
  if (np) self->memUsed_ = self->memUsed_ - osize + nsize;
  return np;
}

// ------------------------------------------------------- instruction budget
void LuaEngine::countHook(lua_State *L, lua_Debug *) {
  auto *self = static_cast<LuaEngine *>(lua_getextraspace(L) ? *(LuaEngine **)lua_getextraspace(L) : nullptr);
  if (self && !self->consumeInstructions(10000)) {
    luaL_error(L, "czlua: instruction budget exceeded (infinite loop?)");
  }
}

int LuaEngine::msgHandler(lua_State *L) {
  const char *msg = lua_tostring(L, 1);
  if (msg) {
    luaL_traceback(L, L, msg, 1);
  } else {
    lua_pushstring(L, "(non-string error)");
  }
  return 1;
}

bool LuaEngine::protectedCall(int nargs, std::string &err) {
  resetInstructions();
  int funcIdx = lua_gettop(L_) - nargs;
  lua_pushcfunction(L_, msgHandler);
  lua_insert(L_, funcIdx);
  int rc = lua_pcall(L_, nargs, 0, funcIdx);
  lua_remove(L_, funcIdx);
  if (rc != LUA_OK) {
    const char *s = lua_tostring(L_, -1);
    err = s ? s : "unknown Lua error";
    lua_pop(L_, 1);
    return false;
  }
  return true;
}

// ---------------------------------------------------------------- lifecycle
LuaEngine::LuaEngine(IGfx &gfx, std::string projectDir)
    : gfx_(gfx), projectDir_(std::move(projectDir)) {
  L_ = lua_newstate(alloc, this);
  // Stash `this` in the extraspace so the static hook can find the engine.
  *static_cast<LuaEngine **>(lua_getextraspace(L_)) = this;

  // Sandbox (memo §6): only these standard libraries are opened.
  luaL_requiref(L_, "_G", luaopen_base, 1);
  lua_pop(L_, 1);
  luaL_requiref(L_, "string", luaopen_string, 1);
  lua_pop(L_, 1);
  luaL_requiref(L_, "math", luaopen_math, 1);
  lua_pop(L_, 1);
  luaL_requiref(L_, "table", luaopen_table, 1);
  lua_pop(L_, 1);
  luaL_requiref(L_, "coroutine", luaopen_coroutine, 1);
  lua_pop(L_, 1);

  // The base library still exposes file/system loaders; remove them so a
  // script cannot escape the project folder or the sandbox.
  static const char *const kBlocked[] = {
      "dofile", "loadfile", "load", "require", "package",
      "os",     "io",       "debug", "collectgarbage", nullptr};
  for (int i = 0; kBlocked[i]; ++i) {
    lua_pushnil(L_);
    lua_setglobal(L_, kBlocked[i]);
  }
  // print() would have nowhere sensible to go on device; route it to a
  // no-op for v0 (a debug console is a v1 editor feature).
  lua_pushcfunction(L_, [](lua_State *) { return 0; });
  lua_setglobal(L_, "print");

  lua_sethook(L_, countHook, LUA_MASKCOUNT, 10000);

  registerCzlua(L_, this);
}

LuaEngine::~LuaEngine() {
  if (L_) lua_close(L_);
}

// ------------------------------------------------------------------ running
bool LuaEngine::loadScript(const std::string &source, std::string &err) {
  if (luaL_loadbuffer(L_, source.data(), source.size(), "=main.lua") != LUA_OK) {
    const char *s = lua_tostring(L_, -1);
    err = s ? s : "syntax error";
    lua_pop(L_, 1);
    return false;
  }
  return protectedCall(0, err);
}

bool LuaEngine::callLoad(std::string &err) {
  lua_getglobal(L_, "load");
  if (!lua_isfunction(L_, -1)) {
    lua_pop(L_, 1);
    return true; // load() is optional
  }
  return protectedCall(0, err);
}

bool LuaEngine::frame(double dt, std::string &err) {
  lua_getglobal(L_, "update");
  if (lua_isfunction(L_, -1)) {
    lua_pushnumber(L_, dt);
    if (!protectedCall(1, err)) return false;
  } else {
    lua_pop(L_, 1);
  }
  lua_getglobal(L_, "draw");
  if (lua_isfunction(L_, -1)) {
    if (!protectedCall(0, err)) return false;
  } else {
    lua_pop(L_, 1);
  }
  return true;
}

// -------------------------------------------------------------------- input
void LuaEngine::setKey(const std::string &key, bool down) {
  if (down) keys_.insert(key); else keys_.erase(key);
}

bool LuaEngine::isDown(const std::string &key) const {
  return keys_.find(key) != keys_.end();
}

bool LuaEngine::keyPressedEvent(const std::string &key, std::string &err) {
  lua_getglobal(L_, "keypressed");
  if (!lua_isfunction(L_, -1)) {
    lua_pop(L_, 1);
    return true; // keypressed() is optional
  }
  lua_pushstring(L_, key.c_str());
  return protectedCall(1, err);
}

} // namespace czlua
