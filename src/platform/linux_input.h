#pragma once

#include "lvgl.h"

#include <cstddef>
#include <cstdint>

namespace platform {

constexpr size_t kNavKeyCount = 5;
constexpr uint32_t kSpecialKeyBase = 0x200000U;
constexpr uint32_t kKeyPrintScreen = kSpecialKeyBase | 18U;
constexpr uint32_t kKeyHelp = kSpecialKeyBase | 19U;

using KeyReleaseListener = void (*)(uint32_t key, const char* key_name, void* user_data);
using GlobalKeyListener = bool (*)(uint32_t key,
                                   const char* key_name,
                                   bool long_pressed,
                                   void* user_data);
// CZ Lua v1: a screen (file list / editor / runner) can capture key
// presses before the global shortcut handler and the nav-button router
// see them. Return true to consume the key. Special keys (PrintScreen,
// Help) are never offered to the capture listener so screenshots and
// the help popup keep working while a screen captures input.
using KeyCaptureListener = bool (*)(uint32_t key, void* user_data);

void init_key_input(lv_display_t* display);
void attach_key_router(lv_indev_t* indev);
void set_nav_shortcut_mode(bool enabled);
void register_nav_button(size_t index, lv_obj_t* button);
void unregister_nav_button(size_t index, lv_obj_t* button);
void set_key_release_listener(KeyReleaseListener listener, void* user_data);
void clear_key_release_listener(KeyReleaseListener listener, void* user_data);
void set_global_key_listener(GlobalKeyListener listener, void* user_data);
void clear_global_key_listener(GlobalKeyListener listener, void* user_data);
void set_key_capture_listener(KeyCaptureListener listener, void* user_data);
void clear_key_capture_listener(KeyCaptureListener listener, void* user_data);
const char* describe_key(uint32_t key);

} // namespace platform
