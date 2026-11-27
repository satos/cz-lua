/*
 * CZ Lua — project file list screen (v1).
 *
 * Lists user projects ($HOME/cz_lua/projects) and bundled samples,
 * plus a "[ New project ]" entry. j/k and the arrow keys move,
 * Enter opens in the editor (bundled samples are copied to the user
 * area first). Keys arrive through the platform key-capture listener
 * (the device keypad has no LVGL group, so object-level LV_EVENT_KEY
 * never fires there). Esc is deliberately NOT captured: it keeps the
 * template behaviour (hold Esc to exit the app).
 */

#pragma once

#include "base_screen.h"
#include "cz_project.h"

#include <vector>

namespace screen {

class FileListScreen : public BaseScreen {
public:
    FileListScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets,
                   CzNavigator* nav);
    ~FileListScreen() override;

private:
    void build_content(lv_obj_t* content) override;
    void refresh_rows();
    void activate_selection();
    static bool capture_cb(uint32_t key, void* user_data);

    CzNavigator* nav_ = nullptr;
    std::vector<CzProject> projects_;
    int selected_ = 0;
    int first_visible_ = 0;
    lv_obj_t* rows_box_ = nullptr;
    lv_obj_t* hint_label_ = nullptr;
    const lv_font_t* font_ = nullptr;
    static constexpr int kVisibleRows = 6;
};

} // namespace screen
