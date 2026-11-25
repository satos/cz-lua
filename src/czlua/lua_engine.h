#pragma once
// CZ Lua engine: owns the lua_State, enforces the sandbox and the
// instruction/memory budgets, and drives the LÖVE-style callbacks
// (load / update / draw / keypressed) defined by the user's script.

#include <cstddef>
#include <set>
#include <string>

struct lua_State;
struct lua_Debug;

namespace czlua {

struct IGfx;

class LuaEngine {
public:
  LuaEngine(IGfx &gfx, std::string projectDir);
  ~LuaEngine();
  LuaEngine(const LuaEngine &) = delete;
  LuaEngine &operator=(const LuaEngine &) = delete;

  // Compile and run the script body (defines the callbacks). Returns
  // false and fills `err` (with line info) on a syntax/runtime error.
  bool loadScript(const std::string &source, std::string &err);
  // Call load() once, if the script defined it.
  bool callLoad(std::string &err);
  // Call update(dt) then draw(), if defined.
  bool frame(double dt, std::string &err);

  // Host input feed. Key names are the device's key names (decided when
  // the evdev layer is wired up on hardware).
  void setKey(const std::string &key, bool down);
  bool isDown(const std::string &key) const;
  bool keyPressedEvent(const std::string &key, std::string &err);

  bool quitRequested() const { return quit_; }
  void requestQuit() { quit_ = true; }

  IGfx &gfx() { return gfx_; }
  const std::string &projectDir() const { return projectDir_; }
  size_t memUsed() const { return memUsed_; }

  // Current pen color (0..1 floats), owned here so the API closures and
  // the engine agree on it.
  float colorR = 1.0f, colorG = 1.0f, colorB = 1.0f;

  // Instruction budget accounting, used by the count hook.
  bool consumeInstructions(long n) {
    instrUsed_ += n;
    return instrUsed_ <= instrCap_;
  }
  void resetInstructions() { instrUsed_ = 0; }

private:
  static void *alloc(void *ud, void *ptr, size_t osize, size_t nsize);
  static void countHook(lua_State *L, lua_Debug *ar);
  static int msgHandler(lua_State *L);
  bool protectedCall(int nargs, std::string &err);

  lua_State *L_ = nullptr;
  IGfx &gfx_;
  std::string projectDir_;
  std::set<std::string> keys_;
  bool quit_ = false;
  size_t memUsed_ = 0;
  size_t memCap_ = 16u * 1024u * 1024u; // memo §6: hard cap for user scripts
  long instrUsed_ = 0;
  long instrCap_ = 100000000L; // per protected call; hook fires every 10k
};

} // namespace czlua
