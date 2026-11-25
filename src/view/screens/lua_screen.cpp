/*
 * CZ Lua — Lua runner screen (implementation).
 *
 * v0 scope: run one bundled project (Game of Life) full-content-area.
 * Script lookup order:
 *   1. /usr/share/cz_lua/projects/life/main.lua  (installed by the .deb)
 *   2. projects/life/main.lua                    (source-tree run)
 *   3. an embedded copy of the same script       (last-resort fallback)
 *
 * Known v0 limitations (unverified on hardware):
 *   - RGB565 byte order is assumed to match the LVGL canvas/fbdev path;
 *     if colours look swapped on device, this is the first suspect.
 *   - Key forwarding uses LV_EVENT_KEY on the screen root; whether the
 *     platform keypad indev delivers events here needs a hardware check.
 *     Keys are treated as momentary presses (cleared after each frame).
 */

#include "lua_screen.h"

#include "theme.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

#include "gfx.h"
#include "lua_engine.h"

namespace screen {
namespace {

// Content area inside BaseScreen's title/nav bars: 320 x 110.
constexpr int kCanvasW = view::kScreenWidth;
constexpr int kCanvasH = view::kScreenHeight - view::kTitleBarHeight - view::kNavBarHeight;

const char* const kProjectCandidates[] = {
    "/usr/share/cz_lua/projects/life/main.lua",
    "projects/life/main.lua",
};

// Embedded fallback: mirrors projects/life/main.lua (keep in sync).
const char kFallbackScript[] = R"LUA(
local CELL = 4
local W, H, grid, nxt, tick
local function seed()
  math.randomseed(20261125)
  grid, nxt = {}, {}
  for y = 1, H do
    grid[y], nxt[y] = {}, {}
    for x = 1, W do grid[y][x] = math.random() < 0.22; nxt[y][x] = false end
  end
end
function load()
  local w, h = czlua.system.getSize()
  W, H = math.floor(w / CELL), math.floor(h / CELL)
  seed()
end
local function step()
  for y = 1, H do for x = 1, W do
    local n = 0
    for dy = -1, 1 do for dx = -1, 1 do
      if not (dx == 0 and dy == 0) then
        if grid[(y + dy - 1) % H + 1][(x + dx - 1) % W + 1] then n = n + 1 end
      end
    end end
    nxt[y][x] = grid[y][x] and (n == 2 or n == 3) or (n == 3)
  end end
  grid, nxt = nxt, grid
end
function update(dt)
  tick = (tick or 0) + 1
  if tick % 3 == 0 then step() end
end
function draw()
  czlua.graphics.clear(0.02, 0.02, 0.05)
  czlua.graphics.setColor(0.55, 0.95, 0.65)
  for y = 1, H do for x = 1, W do
    if grid[y][x] then
      czlua.graphics.rectangle("fill", (x - 1) * CELL, (y - 1) * CELL, CELL - 1, CELL - 1)
    end
  end end
end
function keypressed(key) if key == "space" then seed() end end
)LUA";

std::string read_file(const char* path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return "";
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string key_name(uint32_t lv_key) {
    switch (lv_key) {
        case LV_KEY_ESC: return "escape";
        case LV_KEY_ENTER: return "return";
        case LV_KEY_BACKSPACE: return "backspace";
        case LV_KEY_LEFT: return "left";
        case LV_KEY_RIGHT: return "right";
        case LV_KEY_UP: return "up";
        case LV_KEY_DOWN: return "down";
        case ' ': return "space";
        default:
            if (lv_key >= 32 && lv_key < 127) return std::string(1, (char)lv_key);
            return "";
    }
}

} // namespace

LuaScreen::LuaScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

LuaScreen::~LuaScreen() {
    if (timer_) {
        lv_timer_delete(timer_);
        timer_ = nullptr;
    }
    running_ = false;
    engine_.reset();
    if (canvas_ && lv_obj_is_valid(canvas_)) {
        lv_obj_delete(canvas_);
    }
    canvas_ = nullptr;
    gfx_.reset();
}

void LuaScreen::build_content(lv_obj_t* content) {
    gfx_ = std::make_unique<czlua::FramebufferGfx>(kCanvasW, kCanvasH);

    canvas_ = lv_canvas_create(content);
    lv_canvas_set_buffer(canvas_, gfx_->pixels(), kCanvasW, kCanvasH, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_size(canvas_, kCanvasW, kCanvasH);
    lv_obj_align(canvas_, LV_ALIGN_TOP_MID, 0, 0);

    error_label_ = lv_label_create(content);
    lv_label_set_long_mode(error_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(error_label_, kCanvasW - 8);
    lv_obj_align(error_label_, LV_ALIGN_TOP_LEFT, 4, 4);
    lv_obj_add_flag(error_label_, LV_OBJ_FLAG_HIDDEN);

    // Find the project script.
    std::string source;
    std::string project_dir = "/tmp";
    for (const char* path : kProjectCandidates) {
        source = read_file(path);
        if (!source.empty()) {
            const char* slash = std::strrchr(path, '/');
            if (slash) project_dir = std::string(path, (size_t)(slash - path));
            break;
        }
    }
    if (source.empty()) source = kFallbackScript;

    engine_ = std::make_unique<czlua::LuaEngine>(*gfx_, project_dir);

    std::string err;
    if (!engine_->loadScript(source, err) || !engine_->callLoad(err)) {
        show_error(err);
        return;
    }

    lv_obj_add_event_cb(root(), key_cb, LV_EVENT_KEY, this);
    last_tick_ = lv_tick_get();
    running_ = true;
    timer_ = lv_timer_create(timer_cb, 33, this);
}

void LuaScreen::show_error(const std::string& message) {
    running_ = false;
    if (timer_) {
        lv_timer_delete(timer_);
        timer_ = nullptr;
    }
    if (error_label_) {
        lv_label_set_text(error_label_, ("CZ Lua error:\n" + message).c_str());
        lv_obj_clear_flag(error_label_, LV_OBJ_FLAG_HIDDEN);
    }
    if (canvas_) lv_obj_add_flag(canvas_, LV_OBJ_FLAG_HIDDEN);
}

void LuaScreen::timer_cb(lv_timer_t* timer) {
    auto* self = static_cast<LuaScreen*>(lv_timer_get_user_data(timer));
    if (!self || !self->running_ || !self->engine_) return;

    uint32_t now = lv_tick_get();
    double dt = (now - self->last_tick_) / 1000.0;
    self->last_tick_ = now;
    if (dt <= 0.0 || dt > 0.25) dt = 1.0 / 30.0;

    std::string err;
    if (!self->engine_->frame(dt, err)) {
        self->show_error(err);
        return;
    }
    if (self->engine_->quitRequested()) {
        self->running_ = false;
        return;
    }
    if (self->canvas_) lv_obj_invalidate(self->canvas_);
}

void LuaScreen::key_cb(lv_event_t* e) {
    auto* self = static_cast<LuaScreen*>(lv_event_get_user_data(e));
    if (!self || !self->engine_) return;
    std::string name = key_name(lv_event_get_key(e));
    if (name.empty()) return;
    self->engine_->setKey(name, true);
    std::string err;
    self->engine_->keyPressedEvent(name, err);
    self->engine_->setKey(name, false); // momentary press (v0)
}

} // namespace screen
