/*
 * CZ Lua — vi-style modal editor screen (v1, dev memo §5).
 *
 * Modes: Normal / Insert / Command (':' line at the bottom).
 *   Normal : hjkl and arrow keys move (identical handling — Sato's
 *            requirement: Fn-locked arrows must work exactly like
 *            hjkl in both modes), i = insert, x = delete char,
 *            dd = delete line, u = undo, ':' = command, r = run.
 *   Insert : printable keys type; arrows still move; Esc = Normal.
 *   Command: w = save, q = quit to file list, wq = both, q! = discard.
 * The bottom two lines are a status line and a permanent survival-key
 * hint line. Rendering is one label per visible line over a clipped
 * container (no wrapping; the view shifts horizontally to keep the
 * cursor visible) with a block cursor.
 *
 * All keys are captured (consumed) while this screen is up, so the
 * template's number-key nav shortcuts can never fire mid-edit.
 */

#pragma once

#include "base_screen.h"
#include "cz_project.h"
#include "editor_buffer.h"

#include <string>

namespace screen {

class EditorScreen : public BaseScreen {
public:
    // Load the project's main.lua from disk.
    EditorScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets,
                 CzNavigator* nav, CzProject project);
    // Restore an in-progress edit session (used when returning from
    // the runner, so unsaved changes survive the round trip).
    EditorScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets,
                 CzNavigator* nav, CzProject project, std::string initial_text,
                 bool initial_dirty);
    ~EditorScreen() override;

private:
    enum class Mode { Normal, Insert, Command };

    void build_content(lv_obj_t* content) override;
    void init_editor(const std::string& text, bool dirty);
    bool handle_key(uint32_t key);
    void execute_command(const std::string& cmd);
    void do_save();
    void do_quit();
    void refresh();
    static bool capture_cb(uint32_t key, void* user_data);

    CzNavigator* nav_ = nullptr;
    CzProject project_;
    EditorBuffer buffer_;
    Mode mode_ = Mode::Normal;
    bool pending_d_ = false;
    std::string command_line_;
    std::string status_msg_;

    const lv_font_t* font_ = nullptr;
    int line_height_ = 14;
    int visible_rows_ = 5;
    int first_line_ = 0;
    int x_offset_px_ = 0;

    lv_obj_t* text_box_ = nullptr;
    lv_obj_t* line_labels_[8] = {};
    lv_obj_t* cursor_obj_ = nullptr;
    lv_obj_t* status_label_ = nullptr;
    lv_obj_t* hint_label_ = nullptr;
    static constexpr int kMaxRows = 8;
    static constexpr int kTextHeight = 82;
};

} // namespace screen
