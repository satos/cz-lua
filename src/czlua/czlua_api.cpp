#include "czlua_api.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "gfx.h"
#include "lua_engine.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

namespace czlua {
namespace {

LuaEngine *eng(lua_State *L) {
  return static_cast<LuaEngine *>(lua_touserdata(L, lua_upvalueindex(1)));
}

uint8_t toU8(double v) {
  v = std::clamp(v, 0.0, 1.0);
  return (uint8_t)std::lround(v * 255.0);
}

// ------------------------------------------------------------- primitives
// Everything below is drawn through IGfx::setPixel only.

void plot(LuaEngine *e, int x, int y) {
  e->gfx().setPixel(x, y, toU8(e->colorR), toU8(e->colorG), toU8(e->colorB));
}

void drawLine(LuaEngine *e, int x0, int y0, int x1, int y1) {
  int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    plot(e, x0, y0);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

// -------------------------------------------------------------- graphics
int g_setColor(lua_State *L) {
  LuaEngine *e = eng(L);
  e->colorR = (float)luaL_checknumber(L, 1);
  e->colorG = (float)luaL_checknumber(L, 2);
  e->colorB = (float)luaL_checknumber(L, 3);
  return 0;
}

int g_clear(lua_State *L) {
  LuaEngine *e = eng(L);
  double r = luaL_optnumber(L, 1, 0.0);
  double g = luaL_optnumber(L, 2, 0.0);
  double b = luaL_optnumber(L, 3, 0.0);
  e->gfx().clear(toU8(r), toU8(g), toU8(b));
  return 0;
}

int g_rectangle(lua_State *L) {
  LuaEngine *e = eng(L);
  const char *mode = luaL_checkstring(L, 1);
  int x = (int)luaL_checknumber(L, 2);
  int y = (int)luaL_checknumber(L, 3);
  int w = (int)luaL_checknumber(L, 4);
  int h = (int)luaL_checknumber(L, 5);
  if (w <= 0 || h <= 0) return 0;
  if (mode[0] == 'f') { // "fill"
    for (int yy = y; yy < y + h; ++yy)
      for (int xx = x; xx < x + w; ++xx) plot(e, xx, yy);
  } else { // "line"
    drawLine(e, x, y, x + w - 1, y);
    drawLine(e, x, y + h - 1, x + w - 1, y + h - 1);
    drawLine(e, x, y, x, y + h - 1);
    drawLine(e, x + w - 1, y, x + w - 1, y + h - 1);
  }
  return 0;
}

int g_circle(lua_State *L) {
  LuaEngine *e = eng(L);
  const char *mode = luaL_checkstring(L, 1);
  int cx = (int)luaL_checknumber(L, 2);
  int cy = (int)luaL_checknumber(L, 3);
  int r = (int)luaL_checknumber(L, 4);
  if (r <= 0) return 0;
  if (mode[0] == 'f') {
    for (int yy = -r; yy <= r; ++yy) {
      int half = (int)std::sqrt((double)(r * r - yy * yy));
      for (int xx = -half; xx <= half; ++xx) plot(e, cx + xx, cy + yy);
    }
  } else {
    // Midpoint circle.
    int x = r, y = 0, err = 0;
    while (x >= y) {
      plot(e, cx + x, cy + y); plot(e, cx + y, cy + x);
      plot(e, cx - y, cy + x); plot(e, cx - x, cy + y);
      plot(e, cx - x, cy - y); plot(e, cx - y, cy - x);
      plot(e, cx + y, cy - x); plot(e, cx + x, cy - y);
      ++y;
      if (err <= 0) err += 2 * y + 1;
      if (err > 0) { --x; err -= 2 * x + 1; }
    }
  }
  return 0;
}

int g_line(lua_State *L) {
  LuaEngine *e = eng(L);
  drawLine(e, (int)luaL_checknumber(L, 1), (int)luaL_checknumber(L, 2),
           (int)luaL_checknumber(L, 3), (int)luaL_checknumber(L, 4));
  return 0;
}

int g_points(lua_State *L) {
  LuaEngine *e = eng(L);
  int n = lua_gettop(L);
  for (int i = 1; i + 1 <= n; i += 2)
    plot(e, (int)luaL_checknumber(L, i), (int)luaL_checknumber(L, i + 1));
  return 0;
}

int g_print(lua_State *L) {
  LuaEngine *e = eng(L);
  const char *text = luaL_checkstring(L, 1);
  int x = (int)luaL_checknumber(L, 2);
  int y = (int)luaL_checknumber(L, 3);
  e->gfx().text(x, y, text); // host-rendered; no-op on framebuffer hosts
  return 0;
}

// -------------------------------------------------------------- keyboard
int k_isDown(lua_State *L) {
  lua_pushboolean(L, eng(L)->isDown(luaL_checkstring(L, 1)));
  return 1;
}

// ---------------------------------------------------------------- system
int s_getSize(lua_State *L) {
  LuaEngine *e = eng(L);
  lua_pushinteger(L, e->gfx().width());
  lua_pushinteger(L, e->gfx().height());
  return 2;
}

int s_quit(lua_State *L) {
  eng(L)->requestQuit();
  return 0;
}

// ----------------------------------------------------------------- files
// Sandboxed to the project directory (memo §6 + 2026-11-25 "want"):
// relative names only, no "..", no absolute paths.

bool resolveInProject(LuaEngine *e, const char *name, std::filesystem::path &out) {
  std::filesystem::path rel(name);
  if (rel.is_absolute()) return false;
  for (const auto &part : rel) {
    if (part == "..") return false;
  }
  out = std::filesystem::path(e->projectDir()) / rel;
  return true;
}

int f_readText(lua_State *L) {
  std::filesystem::path p;
  if (!resolveInProject(eng(L), luaL_checkstring(L, 1), p)) {
    lua_pushnil(L);
    return 1;
  }
  std::ifstream in(p, std::ios::binary);
  if (!in) {
    lua_pushnil(L);
    return 1;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  lua_pushstring(L, ss.str().c_str());
  return 1;
}

int f_writeText(lua_State *L) {
  std::filesystem::path p;
  const char *name = luaL_checkstring(L, 1);
  size_t len = 0;
  const char *data = luaL_checklstring(L, 2, &len);
  if (!resolveInProject(eng(L), name, p)) {
    lua_pushboolean(L, 0);
    return 1;
  }
  std::error_code ec;
  if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
  std::ofstream out(p, std::ios::binary | std::ios::trunc);
  if (!out) {
    lua_pushboolean(L, 0);
    return 1;
  }
  out.write(data, (std::streamsize)len);
  lua_pushboolean(L, out.good() ? 1 : 0);
  return 1;
}

// ------------------------------------------------------------ registration
void pushFn(lua_State *L, lua_CFunction fn, LuaEngine *e) {
  lua_pushlightuserdata(L, e);
  lua_pushcclosure(L, fn, 1);
}

void subtable(lua_State *L, const char *name) {
  lua_newtable(L);
  lua_setfield(L, -2, name);
  lua_getfield(L, -1, name);
}

} // namespace

void registerCzlua(lua_State *L, LuaEngine *engine) {
  lua_newtable(L); // czlua

  subtable(L, "graphics");
  lua_pushlightuserdata(L, engine);
  lua_pushcclosure(L, g_setColor, 1);
  lua_setfield(L, -2, "setColor");
  pushFn(L, g_clear, engine);
  lua_setfield(L, -2, "clear");
  pushFn(L, g_rectangle, engine);
  lua_setfield(L, -2, "rectangle");
  pushFn(L, g_circle, engine);
  lua_setfield(L, -2, "circle");
  pushFn(L, g_line, engine);
  lua_setfield(L, -2, "line");
  pushFn(L, g_points, engine);
  lua_setfield(L, -2, "points");
  pushFn(L, g_print, engine);
  lua_setfield(L, -2, "print");
  lua_pop(L, 1); // graphics

  subtable(L, "keyboard");
  pushFn(L, k_isDown, engine);
  lua_setfield(L, -2, "isDown");
  lua_pop(L, 1); // keyboard

  subtable(L, "system");
  pushFn(L, s_getSize, engine);
  lua_setfield(L, -2, "getSize");
  pushFn(L, s_quit, engine);
  lua_setfield(L, -2, "quit");
  lua_pop(L, 1); // system

  subtable(L, "files");
  pushFn(L, f_readText, engine);
  lua_setfield(L, -2, "readText");
  pushFn(L, f_writeText, engine);
  lua_setfield(L, -2, "writeText");
  lua_pop(L, 1); // files

  lua_setglobal(L, "czlua");
}

} // namespace czlua
