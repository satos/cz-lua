/*
 * CZ Lua — Lua runner screen.
 *
 * Hosts the CZ Lua engine inside the template's screen framework: an
 * LVGL canvas backed directly by the engine's RGB565 framebuffer, a
 * ~30fps timer driving load/update/draw, and an error label that takes
 * over when the script fails (the app itself never crashes on a Lua
 * error — dev memo §5).
 */

#pragma once

#include "base_screen.h"
#include "cz_project.h"

#include <memory>
#include <string>

namespace czlua {
class FramebufferGfx;
class LuaEngine;
} // namespace czlua

namespace screen {

class LuaScreen : public BaseScreen {
public:
    // Legacy mode: run the bundled Life sample (v0 behaviour).
    LuaScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);
    // Runner mode (v1): run `source` with the project's folder as the
    // sandbox root. Esc (or czlua.system.quit()) returns to the
    // editor via the navigator.
    LuaScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets,
              CzNavigator* nav, CzProject project, std::string source);
    ~LuaScreen() override;

private:
    void build_content(lv_obj_t* content) override;
    void show_error(const std::string& message);
    void exit_to_editor();
    static void timer_cb(lv_timer_t* timer);
    static void key_cb(lv_event_t* e);
    static bool capture_cb(uint32_t key, void* user_data);

    CzNavigator* nav_ = nullptr;
    CzProject project_;
    std::string source_override_;
    bool runner_mode_ = false;

    std::unique_ptr<czlua::FramebufferGfx> gfx_;
    std::unique_ptr<czlua::LuaEngine> engine_;
    lv_obj_t* canvas_ = nullptr;
    lv_obj_t* error_label_ = nullptr;
    lv_timer_t* timer_ = nullptr;
    uint32_t last_tick_ = 0;
    bool running_ = false;
};

} // namespace screen
