/*
 * CZ Lua — vi-style modal editor screen (implementation).
 */

#include "editor_screen.h"

#include "asset_manager.h"
#include "linux_input.h"
#include "theme.h"

#include <algorithm>

namespace screen {
namespace {

constexpr uint32_t kKeyTab = 9; // LV_KEY_NEXT doubles as Tab from evdev

int text_width(const lv_font_t* font, const std::string& s) {
    lv_point_t size{};
    lv_text_get_size(&size, s.c_str(), font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return size.x;
}

} // namespace

EditorScreen::EditorScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets,
                           CzNavigator* nav, CzProject project)
    : BaseScreen(view_model, assets), nav_(nav), project_(std::move(project)) {
    init();
    init_editor(readProjectMain(project_), false);
    platform::set_key_capture_listener(capture_cb, this);
}

EditorScreen::EditorScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets,
                           CzNavigator* nav, CzProject project, std::string initial_text,
                           bool initial_dirty)
    : BaseScreen(view_model, assets), nav_(nav), project_(std::move(project)) {
    init();
    init_editor(initial_text, initial_dirty);
    platform::set_key_capture_listener(capture_cb, this);
}

EditorScreen::~EditorScreen() {
    platform::clear_key_capture_listener(capture_cb, this);
}

void EditorScreen::init_editor(const std::string& text, bool dirty) {
    buffer_.setText(text);
    if (dirty) buffer_.markDirty();
    refresh();
}

void EditorScreen::build_content(lv_obj_t* content) {
    font_ = assets().load_standard_font(12);
    if (!font_) font_ = &lv_font_montserrat_12;
    line_height_ = lv_font_get_line_height(font_);
    if (line_height_ <= 0) line_height_ = 14;
    visible_rows_ = std::min(kMaxRows, kTextHeight / line_height_);

    text_box_ = lv_obj_create(content);
    lv_obj_remove_style_all(text_box_);
    lv_obj_set_size(text_box_, view::kScreenWidth, kTextHeight);
    lv_obj_align(text_box_, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(text_box_, LV_OBJ_FLAG_SCROLLABLE);

    const auto colors = view::palette(view_model().is_dark_mode());
    for (int i = 0; i < kMaxRows; ++i) {
        auto* label = lv_label_create(text_box_);
        lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_font(label, font_, 0);
        lv_obj_set_style_text_color(label, colors.text, 0);
        lv_obj_set_width(label, view::kScreenWidth * 2);
        line_labels_[i] = label;
    }

    cursor_obj_ = lv_obj_create(text_box_);
    lv_obj_remove_style_all(cursor_obj_);
    lv_obj_set_style_bg_color(cursor_obj_, colors.primary, 0);
    lv_obj_set_style_bg_opa(cursor_obj_, LV_OPA_60, 0);
    lv_obj_clear_flag(cursor_obj_, LV_OBJ_FLAG_SCROLLABLE);

    status_label_ = lv_label_create(content);
    lv_obj_set_style_text_font(status_label_, font_, 0);
    lv_obj_set_style_text_color(status_label_, colors.primary, 0);
    lv_obj_align(status_label_, LV_ALIGN_TOP_LEFT, 4, kTextHeight + 1);

    hint_label_ = lv_label_create(content);
    lv_obj_set_style_text_font(hint_label_, font_, 0);
    lv_obj_set_style_text_color(hint_label_, colors.text, 0);
    lv_obj_align(hint_label_, LV_ALIGN_TOP_LEFT, 4, kTextHeight + 15);
}

void EditorScreen::refresh() {
    if (!text_box_) return;
    const auto& lines = buffer_.lines();
    const int cur_line = buffer_.cursorLine();
    const int cur_col = buffer_.cursorCol();

    // Vertical scroll: keep the cursor line visible.
    if (cur_line < first_line_) first_line_ = cur_line;
    if (cur_line >= first_line_ + visible_rows_) first_line_ = cur_line - visible_rows_ + 1;

    // Horizontal shift: keep the cursor column visible (no wrapping).
    const std::string& cur = lines[cur_line];
    const int prefix_w = text_width(font_, cur.substr(0, static_cast<size_t>(cur_col)));
    constexpr int kLeftPad = 4;
    constexpr int kRightLimit = view::kScreenWidth - 14;
    if (prefix_w - x_offset_px_ < 0) x_offset_px_ = prefix_w;
    if (prefix_w - x_offset_px_ > kRightLimit) x_offset_px_ = prefix_w - kRightLimit;
    if (x_offset_px_ < 0) x_offset_px_ = 0;

    for (int row = 0; row < kMaxRows; ++row) {
        auto* label = line_labels_[row];
        if (!label) continue;
        const int li = first_line_ + row;
        if (row >= visible_rows_ || li >= static_cast<int>(lines.size())) {
            lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(label, lines[li].c_str());
        lv_obj_set_pos(label, kLeftPad - x_offset_px_, row * line_height_);
    }

    // Block cursor over the character cell (or line end).
    const int char_w = std::max(4, text_width(font_, "M"));
    lv_obj_set_size(cursor_obj_, char_w, line_height_);
    lv_obj_set_pos(cursor_obj_, kLeftPad + prefix_w - x_offset_px_,
                   (cur_line - first_line_) * line_height_);
    lv_obj_move_foreground(cursor_obj_);

    // Status line.
    std::string status;
    if (mode_ == Mode::Command) {
        status = ":" + command_line_;
    } else if (!status_msg_.empty()) {
        status = status_msg_;
    } else {
        status = (mode_ == Mode::Insert) ? "-- INSERT -- " : "NORMAL ";
        status += project_.name + "/main.lua";
        if (buffer_.dirty()) status += " *";
        status += "  " + std::to_string(cur_line + 1) + ":" + std::to_string(cur_col + 1);
        if (pending_d_) status += "  d-";
    }
    lv_label_set_text(status_label_, status.c_str());

    const char* hint = "i:edit hjkl/arrows :w :q r:run";
    if (mode_ == Mode::Insert) hint = "ESC:normal  arrows:move";
    if (mode_ == Mode::Command) hint = "Enter:OK  ESC:cancel";
    lv_label_set_text(hint_label_, hint);
}

void EditorScreen::do_save() {
    if (writeProjectMain(project_, buffer_.text())) {
        buffer_.clearDirty();
        status_msg_ = "saved";
    } else {
        status_msg_ = "save FAILED";
    }
}

void EditorScreen::do_quit() {
    if (nav_) nav_->openFileList();
}

void EditorScreen::execute_command(const std::string& cmd) {
    if (cmd == "w") {
        do_save();
    } else if (cmd == "q") {
        if (buffer_.dirty()) {
            status_msg_ = "unsaved changes (:wq / :q!)";
        } else {
            do_quit();
        }
    } else if (cmd == "wq") {
        do_save();
        if (!buffer_.dirty()) do_quit();
    } else if (cmd == "q!") {
        do_quit();
    } else if (cmd.empty()) {
        // bare ':' + Enter: nothing
    } else {
        status_msg_ = "unknown command: " + cmd;
    }
}

bool EditorScreen::handle_key(uint32_t key) {
    status_msg_.clear();

    if (mode_ == Mode::Command) {
        if (key == LV_KEY_ESC) {
            mode_ = Mode::Normal;
            command_line_.clear();
        } else if (key == LV_KEY_ENTER) {
            const std::string cmd = command_line_;
            command_line_.clear();
            mode_ = Mode::Normal;
            execute_command(cmd);
        } else if (key == LV_KEY_BACKSPACE) {
            if (command_line_.empty()) {
                mode_ = Mode::Normal;
            } else {
                command_line_.pop_back();
            }
        } else if (key >= 32 && key < 127) {
            command_line_ += static_cast<char>(key);
        }
        refresh();
        return true;
    }

    if (mode_ == Mode::Insert) {
        switch (key) {
            case LV_KEY_ESC: mode_ = Mode::Normal; break;
            case LV_KEY_ENTER: buffer_.insertNewline(); break;
            case LV_KEY_BACKSPACE: buffer_.backspace(); break;
            case LV_KEY_DEL: buffer_.deleteForward(); break;
            case LV_KEY_LEFT: buffer_.moveLeft(); break;
            case LV_KEY_RIGHT: buffer_.moveRight(); break;
            case LV_KEY_UP: buffer_.moveUp(); break;
            case LV_KEY_DOWN: buffer_.moveDown(); break;
            case LV_KEY_HOME: buffer_.moveLineStart(); break;
            case LV_KEY_END: buffer_.moveLineEnd(); break;
            case kKeyTab: buffer_.insertChar(' '); buffer_.insertChar(' '); break;
            default:
                if (key >= 32 && key < 127) buffer_.insertChar(static_cast<char>(key));
                break;
        }
        refresh();
        return true;
    }

    // Normal mode.
    if (pending_d_) {
        pending_d_ = false;
        if (key == 'd') {
            buffer_.deleteLine();
            refresh();
            return true;
        }
        // fall through and process the key normally
    }
    switch (key) {
        case LV_KEY_LEFT: case 'h': buffer_.moveLeft(); break;
        case LV_KEY_RIGHT: case 'l': buffer_.moveRight(); break;
        case LV_KEY_UP: case 'k': buffer_.moveUp(); break;
        case LV_KEY_DOWN: case 'j': buffer_.moveDown(); break;
        case LV_KEY_HOME: buffer_.moveLineStart(); break;
        case LV_KEY_END: buffer_.moveLineEnd(); break;
        case 'i': mode_ = Mode::Insert; break;
        case 'x': buffer_.deleteCharUnder(); break;
        case 'd': pending_d_ = true; break;
        case 'u': if (!buffer_.undo()) status_msg_ = "nothing to undo"; break;
        case ':': mode_ = Mode::Command; command_line_.clear(); break;
        case 'r':
            if (nav_) nav_->openRunner(project_, buffer_.text(), buffer_.dirty());
            break;
        case LV_KEY_ESC: break; // already normal; swallow
        default: break;
    }
    refresh();
    return true; // the editor consumes every key it is offered
}

bool EditorScreen::capture_cb(uint32_t key, void* user_data) {
    auto* self = static_cast<EditorScreen*>(user_data);
    if (!self) return false;
    return self->handle_key(key);
}

} // namespace screen
