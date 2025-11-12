/*
 * UI Module - Cross-platform GUI via dlopen system libraries
 *
 * Pure C API, zero dependencies, runtime platform detection
 */

#ifndef UI_H
#define UI_H

#include <stddef.h>
#include <stdbool.h>

// Opaque types
typedef struct UIWindow UIWindow;
typedef struct UIButton UIButton;
typedef struct UILabel UILabel;

// Callbacks
typedef void (*UIButtonCallback)(UIButton* button, void* userdata);
typedef bool (*UIWindowCloseCallback)(UIWindow* window, void* userdata);

// Geometry
typedef struct {
    float x, y, width, height;
} UIRect;

typedef struct {
    unsigned char r, g, b, a;
} UIColor;

// Lifecycle
int ui_init(void);
void ui_run(void);
void ui_quit(void);
void ui_cleanup(void);
const char* ui_get_platform(void);

// Window
UIWindow* ui_window_create(const char* title, int width, int height);
void ui_window_destroy(UIWindow* window);
void ui_window_show(UIWindow* window);
void ui_window_hide(UIWindow* window);
void ui_window_center(UIWindow* window);
void ui_window_set_title(UIWindow* window, const char* title);
void ui_window_set_close_callback(UIWindow* window, UIWindowCloseCallback callback, void* userdata);

// Button
UIButton* ui_button_create(UIWindow* window, const char* title, UIRect rect);
void ui_button_set_title(UIButton* button, const char* title);
void ui_button_set_callback(UIButton* button, UIButtonCallback callback, void* userdata);

// Label
UILabel* ui_label_create(UIWindow* window, const char* text, UIRect rect);
void ui_label_set_text(UILabel* label, const char* text);
void ui_label_set_color(UILabel* label, UIColor color);

#endif // UI_H
