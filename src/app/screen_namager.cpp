/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "screen_manager.h"

#include "editor_screen.h"
#include "file_list_screen.h"

#include <algorithm>

#include "apple_screen.h"
#include "lua_screen.h"
#include "butter_screen.h"

namespace app {
namespace {

void flush_requested_page_async(void* user_data) {
    auto* manager = static_cast<ScreenManager*>(user_data);
    if (manager) {
        manager->flush_requested_page();
    }
}

void current_page_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* manager = static_cast<ScreenManager*>(lv_observer_get_target(observer));
    if (!manager) {
        return;
    }

    manager->request_page(static_cast<model::AppPage>(lv_subject_get_int(subject)));
}

} // namespace

ScreenManager::ScreenManager(viewmodel::BaseViewModel& view_model, AssetManager& assets)
    : view_model_(view_model), assets_(assets) {}

ScreenManager::~ScreenManager() {
    lv_async_call_cancel(flush_requested_page_async, this);
    for (auto* entry : pending_nav_) {
        lv_async_call_cancel(nav_async_cb, entry);
        delete entry;
    }
    pending_nav_.clear();

    if (page_observer_) {
        lv_observer_remove(page_observer_);
        page_observer_ = nullptr;
    }
}

void ScreenManager::start() {
    if (page_observer_) {
        return;
    }

    page_observer_ = lv_subject_add_observer_with_target(view_model_.current_page_subject(),
                                                         current_page_observer,
                                                         this,
                                                         nullptr);
}

void ScreenManager::show_apple_screen() {
    // CZ Lua v1: the main page is the project file list; the editor
    // and the runner hang off it via the CzNavigator interface.
    load_screen(std::make_unique<screen::FileListScreen>(view_model_, assets_, this));
    loaded_page_ = model::AppPage::Apple;
    has_loaded_page_ = true;
}

void ScreenManager::show_butter_screen() {
    load_screen(std::make_unique<screen::ButterScreen>(view_model_, assets_));
    loaded_page_ = model::AppPage::Butter;
    has_loaded_page_ = true;
}

void ScreenManager::request_page(model::AppPage page) {
    requested_page_ = page;

    if (page_switch_scheduled_) {
        return;
    }

    page_switch_scheduled_ = lv_async_call(flush_requested_page_async, this) == LV_RESULT_OK;
}

void ScreenManager::flush_requested_page() {
    page_switch_scheduled_ = false;

    if (has_loaded_page_ && requested_page_ == loaded_page_) {
        return;
    }

    if (requested_page_ == model::AppPage::Butter) {
        show_butter_screen();
        return;
    }

    show_apple_screen();
}

lv_obj_t* ScreenManager::current_screen() const {
    return current_screen_ ? current_screen_->root() : nullptr;
}

// ------------------------------------------------------- CZ Lua nav
void ScreenManager::openFileList() {
    queue_nav(NavRequest{NavKind::FileList, screen::CzProject{}, std::string(), false});
}

void ScreenManager::openEditor(const screen::CzProject& project) {
    queue_nav(NavRequest{NavKind::Editor, project, std::string(), false});
}

void ScreenManager::openRunner(const screen::CzProject& project, const std::string& source,
                               bool editor_dirty) {
    queue_nav(NavRequest{NavKind::Runner, project, source, editor_dirty});
}

void ScreenManager::queue_nav(NavRequest request) {
    auto* heap = new NavEntry{this, std::move(request)};
    pending_nav_.push_back(heap);
    if (lv_async_call(nav_async_cb, heap) != LV_RESULT_OK) {
        pending_nav_.erase(std::remove(pending_nav_.begin(), pending_nav_.end(), heap),
                           pending_nav_.end());
        delete heap;
    }
}

void ScreenManager::nav_async_cb(void* user_data) {
    auto* entry = static_cast<NavEntry*>(user_data);
    if (entry && entry->manager) {
        auto& pending = entry->manager->pending_nav_;
        pending.erase(std::remove(pending.begin(), pending.end(), entry), pending.end());
        entry->manager->apply_nav(entry->request);
    }
    delete entry;
}

void ScreenManager::apply_nav(const NavRequest& request) {
    switch (request.kind) {
        case NavKind::FileList:
            has_session_ = false;
            load_screen(std::make_unique<screen::FileListScreen>(view_model_, assets_, this));
            break;
        case NavKind::Editor:
            if (has_session_ && session_project_.mainPath == request.project.mainPath) {
                load_screen(std::make_unique<screen::EditorScreen>(
                    view_model_, assets_, this, request.project, session_text_, session_dirty_));
            } else {
                load_screen(std::make_unique<screen::EditorScreen>(view_model_, assets_, this,
                                                                    request.project));
            }
            break;
        case NavKind::Runner:
            session_project_ = request.project;
            session_text_ = request.source;
            session_dirty_ = request.editor_dirty;
            has_session_ = true;
            load_screen(std::make_unique<screen::LuaScreen>(view_model_, assets_, this,
                                                            request.project, request.source));
            break;
    }
    loaded_page_ = model::AppPage::Apple;
    has_loaded_page_ = true;
}

void ScreenManager::load_screen(std::unique_ptr<screen::BaseScreen> next_screen) {
    if (!next_screen || !next_screen->root()) {
        return;
    }

    lv_screen_load(next_screen->root());
    current_screen_ = std::move(next_screen);
}

} // namespace app
