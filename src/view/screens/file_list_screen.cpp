/*
 * CZ Lua — project file list screen (implementation).
 */

#include "file_list_screen.h"

#include "asset_manager.h"
#include "linux_input.h"
#include "theme.h"

namespace screen {

FileListScreen::FileListScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets,
                               CzNavigator* nav)
    : BaseScreen(view_model, assets), nav_(nav) {
    init();
    platform::set_key_capture_listener(capture_cb, this);
}

FileListScreen::~FileListScreen() {
    platform::clear_key_capture_listener(capture_cb, this);
}

void FileListScreen::build_content(lv_obj_t* content) {
    projects_ = scanProjects();

    font_ = assets().load_standard_font(12);
    if (!font_) font_ = &lv_font_montserrat_12;

    auto* title = lv_label_create(content);
    lv_label_set_text(title, "Projects");
    lv_obj_set_style_text_font(title, font_, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 6, 2);

    rows_box_ = lv_obj_create(content);
    lv_obj_remove_style_all(rows_box_);
    lv_obj_set_size(rows_box_, view::kScreenWidth, kVisibleRows * 14);
    lv_obj_align(rows_box_, LV_ALIGN_TOP_LEFT, 0, 18);
    lv_obj_clear_flag(rows_box_, LV_OBJ_FLAG_SCROLLABLE);

    hint_label_ = lv_label_create(content);
    lv_label_set_text(hint_label_, "j/k Arrows:move  Enter:open");
    lv_obj_set_style_text_font(hint_label_, font_, 0);
    lv_obj_align(hint_label_, LV_ALIGN_BOTTOM_LEFT, 6, -1);

    refresh_rows();
}

void FileListScreen::refresh_rows() {
    if (!rows_box_) return;
    lv_obj_clean(rows_box_);

    const auto colors = view::palette(view_model().is_dark_mode());
    const int total = static_cast<int>(projects_.size()) + 1; // + "[ New ]"
    if (selected_ >= total) selected_ = total - 1;
    if (selected_ < first_visible_) first_visible_ = selected_;
    if (selected_ >= first_visible_ + kVisibleRows) first_visible_ = selected_ - kVisibleRows + 1;

    for (int row = 0; row < kVisibleRows; ++row) {
        const int index = first_visible_ + row;
        if (index >= total) break;

        auto* item = lv_obj_create(rows_box_);
        lv_obj_remove_style_all(item);
        lv_obj_set_size(item, view::kScreenWidth, 14);
        lv_obj_align(item, LV_ALIGN_TOP_LEFT, 0, row * 14);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        if (index == selected_) {
            lv_obj_set_style_bg_color(item, colors.primary, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        }

        auto* label = lv_label_create(item);
        std::string text;
        if (index < static_cast<int>(projects_.size())) {
            const auto& p = projects_[index];
            text = (p.bundled ? "[sample] " : "") + p.name + "/main.lua";
        } else {
            text = "[ New project ]";
        }
        lv_label_set_text(label, text.c_str());
        lv_obj_set_style_text_font(label, font_, 0);
        if (index == selected_) {
            lv_obj_set_style_text_color(label, colors.background, 0);
        }
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 6, 0);
    }
}

void FileListScreen::activate_selection() {
    if (!nav_) return;
    if (selected_ < static_cast<int>(projects_.size())) {
        CzProject p = projects_[selected_];
        if (p.bundled) p = copyBundledToUser(p);
        nav_->openEditor(p);
    } else {
        nav_->openEditor(createNewProject());
    }
}

bool FileListScreen::capture_cb(uint32_t key, void* user_data) {
    auto* self = static_cast<FileListScreen*>(user_data);
    if (!self) return false;
    const int total = static_cast<int>(self->projects_.size()) + 1;
    switch (key) {
        case LV_KEY_DOWN:
            if (self->selected_ + 1 < total) ++self->selected_;
            self->refresh_rows();
            return true;
        case LV_KEY_UP:
            if (self->selected_ > 0) --self->selected_;
            self->refresh_rows();
            return true;
        case LV_KEY_ENTER:
            self->activate_selection();
            return true;
        default:
            break;
    }
    if (key == 'j') {
        if (self->selected_ + 1 < total) ++self->selected_;
        self->refresh_rows();
        return true;
    }
    if (key == 'k') {
        if (self->selected_ > 0) --self->selected_;
        self->refresh_rows();
        return true;
    }
    return false;
}

} // namespace screen
