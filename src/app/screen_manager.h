/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "asset_manager.h"
#include "base_screen.h"
#include "base_viewmodel.h"
#include "cz_project.h"
#include "lvgl.h"

#include <memory>
#include <string>
#include <vector>

namespace app {

// ScreenManager also implements the CZ Lua screen navigation
// (file list <-> editor <-> runner). Navigation requests are
// deferred through lv_async_call because they are typically issued
// from inside key/timer callbacks, where destroying the current
// screen immediately would be unsafe.
class ScreenManager : public screen::CzNavigator {
public:
    ScreenManager(viewmodel::BaseViewModel& view_model, AssetManager& assets);
    ~ScreenManager();

    ScreenManager(const ScreenManager&) = delete;
    ScreenManager& operator=(const ScreenManager&) = delete;

    void start();
    void show_apple_screen();
    void show_butter_screen();
    void request_page(model::AppPage page);
    void flush_requested_page();
    lv_obj_t* current_screen() const;

    // screen::CzNavigator
    void openFileList() override;
    void openEditor(const screen::CzProject& project) override;
    void openRunner(const screen::CzProject& project, const std::string& source,
                    bool editor_dirty) override;

private:
    enum class NavKind { FileList, Editor, Runner };
    struct NavRequest {
        NavKind kind;
        screen::CzProject project;
        std::string source;
        bool editor_dirty = false;
    };
    struct NavEntry {
        ScreenManager* manager;
        NavRequest request;
    };
    void queue_nav(NavRequest request);
    void apply_nav(const NavRequest& request);
    static void nav_async_cb(void* user_data);

    void load_screen(std::unique_ptr<screen::BaseScreen> next_screen);

    viewmodel::BaseViewModel& view_model_;
    AssetManager& assets_;
    std::unique_ptr<screen::BaseScreen> current_screen_;
    lv_observer_t* page_observer_ = nullptr;
    model::AppPage requested_page_ = model::AppPage::Apple;
    model::AppPage loaded_page_ = model::AppPage::Apple;
    bool has_loaded_page_ = false;
    bool page_switch_scheduled_ = false;

    // In-progress edit session, stashed while the runner is up so the
    // editor can be restored verbatim (unsaved changes included).
    screen::CzProject session_project_;
    std::string session_text_;
    bool session_dirty_ = false;
    bool has_session_ = false;

    std::vector<NavEntry*> pending_nav_;
};

} // namespace app
