/*
 * UI Module - Pure dlopen implementation
 * Loads system GUI libraries at runtime
 */

#include "src/cosmorun.h"
#include "ui.h"
#include <stdlib.h>

#define RTLD_LAZY 1
#define RTLD_NOW 2

// ============================================================================
// Global State
// ============================================================================

static int g_initialized = 0;
static const char* g_platform_name = "Unknown";

// ============================================================================
// macOS AppKit Backend
// ============================================================================

#ifdef __APPLE__

static void* g_appkit_handle = NULL;
static void* g_foundation_handle = NULL;
static void* g_autorelease_pool = NULL;

// Objective-C runtime functions
static void* (*objc_getClass)(const char*) = NULL;
static void* (*sel_registerName)(const char*) = NULL;
static void* (*objc_msgSend)(void*, void*, ...) = NULL;

// Helper macro for Objective-C calls
#define CALL(obj, sel, ...) objc_msgSend(obj, sel_registerName(sel), ##__VA_ARGS__)
#define CLASS(name) objc_getClass(name)

static int macos_init(void) {
    printf("[ui] Initializing macOS AppKit backend...\n");

    // Load Objective-C runtime (built into libSystem)
    void* objc_handle = dlopen("/usr/lib/libobjc.dylib", RTLD_LAZY);
    if (!objc_handle) {
        fprintf(stderr, "[ui] Failed to load Objective-C runtime\n");
        return -1;
    }

    objc_getClass = dlsym(objc_handle, "objc_getClass");
    sel_registerName = dlsym(objc_handle, "sel_registerName");
    objc_msgSend = dlsym(objc_handle, "objc_msgSend");

    if (!objc_getClass || !sel_registerName || !objc_msgSend) {
        fprintf(stderr, "[ui] Failed to load Objective-C symbols\n");
        return -1;
    }

    // Load Foundation framework
    g_foundation_handle = dlopen("/System/Library/Frameworks/Foundation.framework/Foundation", RTLD_LAZY);
    if (!g_foundation_handle) {
        fprintf(stderr, "[ui] Failed to load Foundation.framework\n");
        return -1;
    }

    // Load AppKit framework
    g_appkit_handle = dlopen("/System/Library/Frameworks/AppKit.framework/AppKit", RTLD_LAZY);
    if (!g_appkit_handle) {
        fprintf(stderr, "[ui] Failed to load AppKit.framework\n");
        return -1;
    }

    // Create autorelease pool FIRST
    void* pool_class = CLASS("NSAutoreleasePool");
    g_autorelease_pool = CALL(CALL(pool_class, "alloc"), "init");

    // Initialize NSApplication
    void* app_class = CLASS("NSApplication");
    void* app = CALL(app_class, "sharedApplication");

    // Set activation policy to Regular (normal app with dock icon)
    // NSApplicationActivationPolicyRegular = 0
    CALL(app, "setActivationPolicy:", 0);

    // Create a simple menu with Quit option
    void* menu_bar = CALL(CALL(CLASS("NSMenu"), "alloc"), "init");
    void* app_menu_item = CALL(CALL(CLASS("NSMenuItem"), "alloc"), "init");
    CALL(menu_bar, "addItem:", app_menu_item);
    CALL(app, "setMainMenu:", menu_bar);

    void* app_menu = CALL(CALL(CLASS("NSMenu"), "alloc"), "init");
    void* quit_title = CALL(CLASS("NSString"), "stringWithUTF8String:", "Quit");
    void* quit_item = CALL(CALL(CLASS("NSMenuItem"), "alloc"),
                          "initWithTitle:action:keyEquivalent:",
                          quit_title,
                          sel_registerName("terminate:"),
                          CALL(CLASS("NSString"), "stringWithUTF8String:", "q"));
    CALL(app_menu, "addItem:", quit_item);
    CALL(app_menu_item, "setSubmenu:", app_menu);

    // Activate app (make it foreground)
    CALL(app, "activateIgnoringOtherApps:", (bool)true);

    printf("[ui] ✓ AppKit initialized (Cmd+Q to quit)\n");
    return 0;
}

UIWindow* macos_window_create(const char* title, int width, int height) {
    printf("[ui] Creating macOS window: %s (%dx%d)\n", title, width, height);

    // NSWindow* window = [[NSWindow alloc] initWithContentRect:... styleMask:... backing:... defer:...]
    void* window_class = CLASS("NSWindow");

    // Create NSRect for content rect
    typedef struct { float x, y, w, h; } NSRect;
    NSRect content_rect = {100, 100, (float)width, (float)height};

    // Window style: titled, closable, miniaturizable, resizable
    unsigned long style_mask = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);

    // Buffered backing store
    unsigned long backing = 2;

    void* window = CALL(CALL(window_class, "alloc"),
                       "initWithContentRect:styleMask:backing:defer:",
                       content_rect, style_mask, backing, (bool)false);

    // Set title
    void* ns_string_class = CLASS("NSString");
    void* ns_title = CALL(ns_string_class, "stringWithUTF8String:", title);
    CALL(window, "setTitle:", ns_title);

    // Make window close terminate the app
    // This is a simple approach: when window closes, quit the app
    // A proper delegate would be more elegant but requires more setup

    printf("[ui] ✓ Window created: %p\n", window);
    return (UIWindow*)window;
}

void macos_window_show(UIWindow* window) {
    CALL((void*)window, "makeKeyAndOrderFront:", NULL);
}

void macos_window_center(UIWindow* window) {
    CALL((void*)window, "center");
}

void macos_window_set_title(UIWindow* window, const char* title) {
    void* ns_string_class = CLASS("NSString");
    void* ns_title = CALL(ns_string_class, "stringWithUTF8String:", title);
    CALL((void*)window, "setTitle:", ns_title);
}

void macos_run(void) {
    void* app = CALL(CLASS("NSApplication"), "sharedApplication");
    CALL(app, "run");
}

void macos_quit(void) {
    void* app = CALL(CLASS("NSApplication"), "sharedApplication");
    CALL(app, "terminate:", NULL);
}

void macos_cleanup(void) {
    if (g_autorelease_pool) {
        CALL(g_autorelease_pool, "drain");
        g_autorelease_pool = NULL;
    }
    if (g_appkit_handle) dlclose(g_appkit_handle);
    if (g_foundation_handle) dlclose(g_foundation_handle);
}

// Helper macro for creating NSString
#define NSSTRING(str) CALL(CLASS("NSString"), "stringWithUTF8String:", str)

// macOS Button implementation
struct macOSButtonImpl {
    void* nsbutton;  // NSButton*
    UIButtonCallback callback;
    void* userdata;
};

UIButton* macos_button_create(UIWindow* window, const char* title, UIRect rect) {
    printf("[ui] Creating macOS button: %s\n", title);

    // Allocate button implementation
    struct macOSButtonImpl* impl = (struct macOSButtonImpl*)malloc(sizeof(struct macOSButtonImpl));
    if (!impl) {
        fprintf(stderr, "[ui] Failed to allocate button\n");
        return NULL;
    }

    impl->callback = NULL;
    impl->userdata = NULL;

    // Create NSButton
    void* button_class = CLASS("NSButton");

    // Create NSRect for button frame
    typedef struct { float x, y, w, h; } NSRect;
    NSRect frame = {rect.x, rect.y, rect.width, rect.height};

    // Allocate and initialize NSButton
    impl->nsbutton = CALL(CALL(button_class, "alloc"), "initWithFrame:", frame);

    // Set button title
    void* ns_title = NSSTRING(title);
    CALL(impl->nsbutton, "setTitle:", ns_title);

    // Set bezel style to NSRoundedBezelStyle (1)
    CALL(impl->nsbutton, "setBezelStyle:", 1);

    // Add button to window's content view
    void* content_view = CALL((void*)window, "contentView");
    CALL(content_view, "addSubview:", impl->nsbutton);

    printf("[ui] ✓ Button created: %p\n", impl->nsbutton);
    return (UIButton*)impl;
}

void macos_button_set_title(UIButton* button, const char* title) {
    if (!button) return;
    struct macOSButtonImpl* impl = (struct macOSButtonImpl*)button;

    void* ns_title = NSSTRING(title);
    CALL(impl->nsbutton, "setTitle:", ns_title);
}

void macos_button_set_callback(UIButton* button, UIButtonCallback callback, void* userdata) {
    if (!button) return;
    struct macOSButtonImpl* impl = (struct macOSButtonImpl*)button;

    impl->callback = callback;
    impl->userdata = userdata;

    // Note: Full callback implementation would require target-action setup
    // This is a simplified version - full implementation needs Objective-C block/target
    printf("[ui] Button callback set (simplified implementation)\n");
}

// macOS Label implementation
struct macOSLabelImpl {
    void* nstextfield;  // NSTextField*
};

UILabel* macos_label_create(UIWindow* window, const char* text, UIRect rect) {
    printf("[ui] Creating macOS label: %s\n", text);

    // Allocate label implementation
    struct macOSLabelImpl* impl = (struct macOSLabelImpl*)malloc(sizeof(struct macOSLabelImpl));
    if (!impl) {
        fprintf(stderr, "[ui] Failed to allocate label\n");
        return NULL;
    }

    // Create NSTextField
    void* textfield_class = CLASS("NSTextField");

    // Create NSRect for label frame
    typedef struct { float x, y, w, h; } NSRect;
    NSRect frame = {rect.x, rect.y, rect.width, rect.height};

    // Allocate and initialize NSTextField
    impl->nstextfield = CALL(CALL(textfield_class, "alloc"), "initWithFrame:", frame);

    // Set text
    void* ns_text = NSSTRING(text);
    CALL(impl->nstextfield, "setStringValue:", ns_text);

    // Make it non-editable (label style)
    CALL(impl->nstextfield, "setEditable:", (bool)false);

    // Remove border/bezel (label style)
    CALL(impl->nstextfield, "setBezeled:", (bool)false);

    // Make background transparent
    CALL(impl->nstextfield, "setDrawsBackground:", (bool)false);

    // Add label to window's content view
    void* content_view = CALL((void*)window, "contentView");
    CALL(content_view, "addSubview:", impl->nstextfield);

    printf("[ui] ✓ Label created: %p\n", impl->nstextfield);
    return (UILabel*)impl;
}

void macos_label_set_text(UILabel* label, const char* text) {
    if (!label) return;
    struct macOSLabelImpl* impl = (struct macOSLabelImpl*)label;

    void* ns_text = NSSTRING(text);
    CALL(impl->nstextfield, "setStringValue:", ns_text);
}

void macos_label_set_color(UILabel* label, UIColor color) {
    if (!label) return;
    struct macOSLabelImpl* impl = (struct macOSLabelImpl*)label;

    // Create NSColor from UIColor
    void* nscolor_class = CLASS("NSColor");
    float r = color.r / 255.0f;
    float g = color.g / 255.0f;
    float b = color.b / 255.0f;
    float a = color.a / 255.0f;

    void* ns_color = CALL(nscolor_class, "colorWithRed:green:blue:alpha:", r, g, b, a);
    CALL(impl->nstextfield, "setTextColor:", ns_color);

    printf("[ui] Label color set to RGBA(%d,%d,%d,%d)\n", color.r, color.g, color.b, color.a);
}

#endif // __APPLE__

// ============================================================================
// Linux GTK Backend
// ============================================================================

#ifdef __linux__

static void* g_gtk_handle = NULL;

// GTK function pointers
static void (*gtk_init)(int*, char***) = NULL;
static void* (*gtk_window_new)(int) = NULL;
static void (*gtk_window_set_title)(void*, const char*) = NULL;
static void (*gtk_window_set_default_size)(void*, int, int) = NULL;
static void (*gtk_widget_show_all)(void*) = NULL;
static void (*gtk_widget_show)(void*) = NULL;
static void (*gtk_main)(void) = NULL;
static void (*gtk_main_quit)(void) = NULL;
static void (*gtk_container_add)(void*, void*) = NULL;
static void* (*gtk_fixed_new)(void) = NULL;
static void (*gtk_fixed_put)(void*, void*, int, int) = NULL;
static void (*gtk_widget_set_size_request)(void*, int, int) = NULL;
static void* (*gtk_button_new_with_label)(const char*) = NULL;
static void (*gtk_button_set_label)(void*, const char*) = NULL;
static void* (*gtk_label_new)(const char*) = NULL;
static void (*gtk_label_set_text)(void*, const char*) = NULL;
static unsigned long (*g_signal_connect_data)(void*, const char*, void*, void*, void*, int) = NULL;

static int linux_init(void) {
    printf("[ui] Initializing Linux GTK backend...\n");

    // Try loading GTK-3
    g_gtk_handle = dlopen("libgtk-3.so.0", RTLD_LAZY);
    if (!g_gtk_handle) {
        g_gtk_handle = dlopen("libgtk-3.so", RTLD_LAZY);
    }

    if (!g_gtk_handle) {
        fprintf(stderr, "[ui] Failed to load GTK-3 library\n");
        fprintf(stderr, "[ui] Install: sudo apt install libgtk-3-0\n");
        return -1;
    }

    // Load GTK symbols
    gtk_init = dlsym(g_gtk_handle, "gtk_init");
    gtk_window_new = dlsym(g_gtk_handle, "gtk_window_new");
    gtk_window_set_title = dlsym(g_gtk_handle, "gtk_window_set_title");
    gtk_window_set_default_size = dlsym(g_gtk_handle, "gtk_window_set_default_size");
    gtk_widget_show_all = dlsym(g_gtk_handle, "gtk_widget_show_all");
    gtk_widget_show = dlsym(g_gtk_handle, "gtk_widget_show");
    gtk_main = dlsym(g_gtk_handle, "gtk_main");
    gtk_main_quit = dlsym(g_gtk_handle, "gtk_main_quit");
    gtk_container_add = dlsym(g_gtk_handle, "gtk_container_add");
    gtk_fixed_new = dlsym(g_gtk_handle, "gtk_fixed_new");
    gtk_fixed_put = dlsym(g_gtk_handle, "gtk_fixed_put");
    gtk_widget_set_size_request = dlsym(g_gtk_handle, "gtk_widget_set_size_request");
    gtk_button_new_with_label = dlsym(g_gtk_handle, "gtk_button_new_with_label");
    gtk_button_set_label = dlsym(g_gtk_handle, "gtk_button_set_label");
    gtk_label_new = dlsym(g_gtk_handle, "gtk_label_new");
    gtk_label_set_text = dlsym(g_gtk_handle, "gtk_label_set_text");
    g_signal_connect_data = dlsym(g_gtk_handle, "g_signal_connect_data");

    if (!gtk_init || !gtk_window_new || !gtk_main || !gtk_fixed_new) {
        fprintf(stderr, "[ui] Failed to load GTK symbols\n");
        dlclose(g_gtk_handle);
        return -1;
    }

    // Initialize GTK
    gtk_init(NULL, NULL);

    printf("[ui] ✓ GTK initialized\n");
    return 0;
}

// Linux window implementation stores both window and fixed container
struct UIWindowImpl {
    void* gtkwindow;  // GtkWindow*
    void* gtkfixed;   // GtkFixed* (container for absolute positioning)
};

UIWindow* linux_window_create(const char* title, int width, int height) {
    printf("[ui] Creating Linux window: %s (%dx%d)\n", title, width, height);

    struct UIWindowImpl* impl = malloc(sizeof(struct UIWindowImpl));
    if (!impl) return NULL;

    // Create window
    impl->gtkwindow = gtk_window_new(0); // GTK_WINDOW_TOPLEVEL = 0
    gtk_window_set_title(impl->gtkwindow, title);
    gtk_window_set_default_size(impl->gtkwindow, width, height);

    // Create fixed container for absolute positioning
    impl->gtkfixed = gtk_fixed_new();
    gtk_container_add(impl->gtkwindow, impl->gtkfixed);

    printf("[ui] ✓ Window created: %p\n", impl);
    return (UIWindow*)impl;
}

void linux_window_show(UIWindow* window) {
    if (!window) return;
    struct UIWindowImpl* impl = (struct UIWindowImpl*)window;
    gtk_widget_show_all(impl->gtkwindow);
}

void linux_run(void) {
    gtk_main();
}

void linux_quit(void) {
    gtk_main_quit();
}

void linux_cleanup(void) {
    if (g_gtk_handle) dlclose(g_gtk_handle);
}

// Linux button implementation
struct LinuxButtonImpl {
    void* gtkbutton;  // GtkWidget*
    UIButtonCallback callback;
    void* userdata;
};

// Button click callback wrapper
static void linux_button_clicked(void* widget, void* data) {
    struct LinuxButtonImpl* impl = (struct LinuxButtonImpl*)data;
    if (impl && impl->callback) {
        impl->callback((UIButton*)impl, impl->userdata);
    }
}

UIButton* linux_button_create(UIWindow* window, const char* title, UIRect rect) {
    if (!window) return NULL;

    printf("[ui] Creating Linux button: %s at (%d,%d) size %dx%d\n",
           title, rect.x, rect.y, rect.width, rect.height);

    struct UIWindowImpl* win_impl = (struct UIWindowImpl*)window;
    struct LinuxButtonImpl* impl = malloc(sizeof(struct LinuxButtonImpl));
    if (!impl) return NULL;

    impl->callback = NULL;
    impl->userdata = NULL;

    // Create button
    impl->gtkbutton = gtk_button_new_with_label(title);

    // Add to fixed container at specified position
    gtk_fixed_put(win_impl->gtkfixed, impl->gtkbutton, rect.x, rect.y);

    // Set size
    gtk_widget_set_size_request(impl->gtkbutton, rect.width, rect.height);

    // Show widget
    gtk_widget_show(impl->gtkbutton);

    printf("[ui] ✓ Button created: %p\n", impl);
    return (UIButton*)impl;
}

void linux_button_set_title(UIButton* button, const char* title) {
    if (!button) return;
    struct LinuxButtonImpl* impl = (struct LinuxButtonImpl*)button;
    gtk_button_set_label(impl->gtkbutton, title);
}

void linux_button_set_callback(UIButton* button, UIButtonCallback callback, void* userdata) {
    if (!button) return;
    struct LinuxButtonImpl* impl = (struct LinuxButtonImpl*)button;
    impl->callback = callback;
    impl->userdata = userdata;

    // Connect clicked signal
    if (g_signal_connect_data) {
        g_signal_connect_data(impl->gtkbutton, "clicked",
                             (void*)linux_button_clicked, impl, NULL, 0);
    }
}

// Linux label implementation
struct LinuxLabelImpl {
    void* gtklabel;  // GtkWidget*
};

UILabel* linux_label_create(UIWindow* window, const char* text, UIRect rect) {
    if (!window) return NULL;

    printf("[ui] Creating Linux label: %s at (%d,%d) size %dx%d\n",
           text, rect.x, rect.y, rect.width, rect.height);

    struct UIWindowImpl* win_impl = (struct UIWindowImpl*)window;
    struct LinuxLabelImpl* impl = malloc(sizeof(struct LinuxLabelImpl));
    if (!impl) return NULL;

    // Create label
    impl->gtklabel = gtk_label_new(text);

    // Add to fixed container at specified position
    gtk_fixed_put(win_impl->gtkfixed, impl->gtklabel, rect.x, rect.y);

    // Set size
    gtk_widget_set_size_request(impl->gtklabel, rect.width, rect.height);

    // Show widget
    gtk_widget_show(impl->gtklabel);

    printf("[ui] ✓ Label created: %p\n", impl);
    return (UILabel*)impl;
}

void linux_label_set_text(UILabel* label, const char* text) {
    if (!label) return;
    struct LinuxLabelImpl* impl = (struct LinuxLabelImpl*)label;
    gtk_label_set_text(impl->gtklabel, text);
}

void linux_label_set_color(UILabel* label, UIColor color) {
    // TODO: Implement using CSS provider
    // For now, just a stub
    (void)label;
    (void)color;
}

#endif // __linux__

// ============================================================================
// Windows Win32 Backend
// ============================================================================

#ifdef _WIN32

static void* g_user32_handle = NULL;
static void* g_kernel32_handle = NULL;

// Win32 types (simplified)
typedef void* HWND;
typedef void* HINSTANCE;
typedef void* HMENU;
typedef void* HICON;
typedef void* HCURSOR;
typedef void* HBRUSH;
typedef unsigned long DWORD;
typedef unsigned int UINT;
typedef long LONG;
typedef long long LONG_PTR;
typedef unsigned long long UINT_PTR;
typedef const char* LPCSTR;
typedef const unsigned short* LPCWSTR;

typedef struct {
    UINT    cbSize;
    UINT    style;
    void*   lpfnWndProc;
    int     cbClsExtra;
    int     cbWndExtra;
    HINSTANCE hInstance;
    HICON   hIcon;
    HCURSOR hCursor;
    HBRUSH  hbrBackground;
    LPCSTR  lpszMenuName;
    LPCSTR  lpszClassName;
    HICON   hIconSm;
} WNDCLASSEX;

typedef struct {
    HWND   hwnd;
    UINT   message;
    UINT_PTR wParam;
    LONG_PTR lParam;
    DWORD  time;
    struct { LONG x; LONG y; } pt;
} MSG;

// Win32 function pointers
static HWND (*CreateWindowExA)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, void*) = NULL;
static int (*ShowWindow)(HWND, int) = NULL;
static int (*UpdateWindow)(HWND) = NULL;
static int (*GetMessageA)(MSG*, HWND, UINT, UINT) = NULL;
static int (*TranslateMessage)(const MSG*) = NULL;
static LONG_PTR (*DispatchMessageA)(const MSG*) = NULL;
static void (*PostQuitMessage)(int) = NULL;
static LONG_PTR (*DefWindowProcA)(HWND, UINT, UINT_PTR, LONG_PTR) = NULL;
static unsigned short (*RegisterClassExA)(const WNDCLASSEX*) = NULL;
static HINSTANCE (*GetModuleHandleA)(LPCSTR) = NULL;
static HCURSOR (*LoadCursorA)(HINSTANCE, LPCSTR) = NULL;
static int (*SetWindowTextA)(HWND, LPCSTR) = NULL;
static LONG_PTR (*SendMessageA)(HWND, UINT, UINT_PTR, LONG_PTR) = NULL;
static LONG_PTR (*GetWindowLongPtrA)(HWND, int) = NULL;
static LONG_PTR (*SetWindowLongPtrA)(HWND, int, LONG_PTR) = NULL;

// Window class name
static const char* g_window_class_name = "CosmorunUIWindow";
static int g_window_class_registered = 0;

// Button/Label implementation structures
struct WindowsButtonImpl {
    HWND hwnd;
    UIButtonCallback callback;
    void* userdata;
};

struct WindowsLabelImpl {
    HWND hwnd;
};

// Simple button tracking (max 64 buttons)
#define MAX_BUTTONS 64
static struct WindowsButtonImpl* g_button_map[MAX_BUTTONS] = {0};

// Default window procedure
static LONG_PTR CALLBACK WndProc(HWND hwnd, UINT msg, UINT_PTR wParam, LONG_PTR lParam) {
    switch (msg) {
        case 0x0002: // WM_DESTROY
            PostQuitMessage(0);
            return 0;
        case 0x0010: // WM_CLOSE
            PostQuitMessage(0);
            return 0;
        case 0x0111: // WM_COMMAND
            if ((wParam >> 16) == 0) {  // HIWORD(wParam) == 0 means button click
                HWND button_hwnd = (HWND)lParam;
                // Find button in map and invoke callback
                for (int i = 0; i < MAX_BUTTONS; i++) {
                    if (g_button_map[i] && g_button_map[i]->hwnd == button_hwnd) {
                        if (g_button_map[i]->callback) {
                            g_button_map[i]->callback((UIButton*)g_button_map[i], g_button_map[i]->userdata);
                        }
                        break;
                    }
                }
            }
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static int windows_init(void) {
    printf("[ui] Initializing Windows Win32 backend...\n");

    // Load User32.dll
    g_user32_handle = dlopen("User32.dll", RTLD_LAZY);
    if (!g_user32_handle) {
        g_user32_handle = dlopen("user32.dll", RTLD_LAZY);
    }
    if (!g_user32_handle) {
        fprintf(stderr, "[ui] Failed to load User32.dll\n");
        return -1;
    }

    // Load Kernel32.dll
    g_kernel32_handle = dlopen("Kernel32.dll", RTLD_LAZY);
    if (!g_kernel32_handle) {
        g_kernel32_handle = dlopen("kernel32.dll", RTLD_LAZY);
    }

    // Load Win32 symbols
    CreateWindowExA = dlsym(g_user32_handle, "CreateWindowExA");
    ShowWindow = dlsym(g_user32_handle, "ShowWindow");
    UpdateWindow = dlsym(g_user32_handle, "UpdateWindow");
    GetMessageA = dlsym(g_user32_handle, "GetMessageA");
    TranslateMessage = dlsym(g_user32_handle, "TranslateMessage");
    DispatchMessageA = dlsym(g_user32_handle, "DispatchMessageA");
    PostQuitMessage = dlsym(g_user32_handle, "PostQuitMessage");
    DefWindowProcA = dlsym(g_user32_handle, "DefWindowProcA");
    RegisterClassExA = dlsym(g_user32_handle, "RegisterClassExA");
    LoadCursorA = dlsym(g_user32_handle, "LoadCursorA");
    SetWindowTextA = dlsym(g_user32_handle, "SetWindowTextA");
    SendMessageA = dlsym(g_user32_handle, "SendMessageA");
    GetWindowLongPtrA = dlsym(g_user32_handle, "GetWindowLongPtrA");
    SetWindowLongPtrA = dlsym(g_user32_handle, "SetWindowLongPtrA");

    if (g_kernel32_handle) {
        GetModuleHandleA = dlsym(g_kernel32_handle, "GetModuleHandleA");
    }

    if (!CreateWindowExA || !ShowWindow || !GetMessageA || !RegisterClassExA) {
        fprintf(stderr, "[ui] Failed to load Win32 symbols\n");
        return -1;
    }

    printf("[ui] ✓ Win32 API loaded\n");
    return 0;
}

UIWindow* windows_window_create(const char* title, int width, int height) {
    printf("[ui] Creating Windows window: %s (%dx%d)\n", title, width, height);

    // Register window class (once)
    if (!g_window_class_registered) {
        HINSTANCE hInstance = GetModuleHandleA ? GetModuleHandleA(NULL) : NULL;

        WNDCLASSEX wc = {0};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = 0;
        wc.lpfnWndProc = (void*)WndProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = hInstance;
        wc.hIcon = NULL;
        wc.hCursor = LoadCursorA ? LoadCursorA(NULL, (LPCSTR)32512) : NULL; // IDC_ARROW
        wc.hbrBackground = (HBRUSH)(5 + 1); // COLOR_WINDOW + 1
        wc.lpszMenuName = NULL;
        wc.lpszClassName = g_window_class_name;
        wc.hIconSm = NULL;

        if (!RegisterClassExA(&wc)) {
            fprintf(stderr, "[ui] Failed to register window class\n");
            return NULL;
        }

        g_window_class_registered = 1;
        printf("[ui] ✓ Window class registered\n");
    }

    // Create window
    HINSTANCE hInstance = GetModuleHandleA ? GetModuleHandleA(NULL) : NULL;

    // Window styles
    DWORD dwStyle = 0x00000000L      // WS_OVERLAPPED
                  | 0x00C00000L      // WS_CAPTION
                  | 0x00080000L      // WS_SYSMENU
                  | 0x00040000L      // WS_THICKFRAME
                  | 0x00020000L      // WS_MINIMIZEBOX
                  | 0x00010000L;     // WS_MAXIMIZEBOX

    HWND hwnd = CreateWindowExA(
        0,                          // dwExStyle
        g_window_class_name,        // lpClassName
        title,                      // lpWindowName
        dwStyle,                    // dwStyle
        100,                        // x (CW_USEDEFAULT)
        100,                        // y
        width,                      // nWidth
        height,                     // nHeight
        NULL,                       // hWndParent
        NULL,                       // hMenu
        hInstance,                  // hInstance
        NULL                        // lpParam
    );

    if (!hwnd) {
        fprintf(stderr, "[ui] Failed to create window\n");
        return NULL;
    }

    printf("[ui] ✓ Window created: %p\n", hwnd);
    return (UIWindow*)hwnd;
}

void windows_window_show(UIWindow* window) {
    if (!window) return;
    ShowWindow((HWND)window, 5); // SW_SHOW = 5
    if (UpdateWindow) {
        UpdateWindow((HWND)window);
    }
}

void windows_run(void) {
    printf("[ui] Running Windows message loop...\n");

    MSG msg = {0};
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    printf("[ui] Message loop exited\n");
}

void windows_quit(void) {
    if (PostQuitMessage) {
        PostQuitMessage(0);
    }
}

void windows_cleanup(void) {
    if (g_user32_handle) dlclose(g_user32_handle);
    if (g_kernel32_handle) dlclose(g_kernel32_handle);
}

// Button implementation
UIButton* windows_button_create(UIWindow* window, const char* title, UIRect rect) {
    printf("[ui] Creating Windows button: %s\n", title);

    if (!window) {
        fprintf(stderr, "[ui] Button creation failed: NULL window\n");
        return NULL;
    }

    // Allocate button structure
    struct WindowsButtonImpl* impl = (struct WindowsButtonImpl*)malloc(sizeof(struct WindowsButtonImpl));
    if (!impl) {
        fprintf(stderr, "[ui] Failed to allocate button structure\n");
        return NULL;
    }

    impl->callback = NULL;
    impl->userdata = NULL;

    // Get parent window HWND
    HWND parent_hwnd = (HWND)window;

    // Get module instance
    HINSTANCE hInstance = GetModuleHandleA ? GetModuleHandleA(NULL) : NULL;

    // Button styles
    DWORD dwStyle = 0x40000000L      // WS_CHILD
                  | 0x10000000L      // WS_VISIBLE
                  | 0x00000000L;     // BS_PUSHBUTTON

    // Create button window
    impl->hwnd = CreateWindowExA(
        0,                          // dwExStyle
        "BUTTON",                   // lpClassName
        title,                      // lpWindowName
        dwStyle,                    // dwStyle
        (int)rect.x,                // x
        (int)rect.y,                // y
        (int)rect.width,            // nWidth
        (int)rect.height,           // nHeight
        parent_hwnd,                // hWndParent
        NULL,                       // hMenu
        hInstance,                  // hInstance
        NULL                        // lpParam
    );

    if (!impl->hwnd) {
        fprintf(stderr, "[ui] Failed to create button window\n");
        free(impl);
        return NULL;
    }

    // Add to button map
    for (int i = 0; i < MAX_BUTTONS; i++) {
        if (!g_button_map[i]) {
            g_button_map[i] = impl;
            break;
        }
    }

    printf("[ui] ✓ Button created: %p\n", impl->hwnd);
    return (UIButton*)impl;
}

void windows_button_set_title(UIButton* button, const char* title) {
    if (!button || !SetWindowTextA) return;

    struct WindowsButtonImpl* impl = (struct WindowsButtonImpl*)button;
    SetWindowTextA(impl->hwnd, title);
}

void windows_button_set_callback(UIButton* button, UIButtonCallback callback, void* userdata) {
    if (!button) return;

    struct WindowsButtonImpl* impl = (struct WindowsButtonImpl*)button;
    impl->callback = callback;
    impl->userdata = userdata;
}

// Label implementation
UILabel* windows_label_create(UIWindow* window, const char* text, UIRect rect) {
    printf("[ui] Creating Windows label: %s\n", text);

    if (!window) {
        fprintf(stderr, "[ui] Label creation failed: NULL window\n");
        return NULL;
    }

    // Allocate label structure
    struct WindowsLabelImpl* impl = (struct WindowsLabelImpl*)malloc(sizeof(struct WindowsLabelImpl));
    if (!impl) {
        fprintf(stderr, "[ui] Failed to allocate label structure\n");
        return NULL;
    }

    // Get parent window HWND
    HWND parent_hwnd = (HWND)window;

    // Get module instance
    HINSTANCE hInstance = GetModuleHandleA ? GetModuleHandleA(NULL) : NULL;

    // Static control styles
    DWORD dwStyle = 0x40000000L      // WS_CHILD
                  | 0x10000000L      // WS_VISIBLE
                  | 0x00000000L;     // SS_LEFT

    // Create static window (label)
    impl->hwnd = CreateWindowExA(
        0,                          // dwExStyle
        "STATIC",                   // lpClassName
        text,                       // lpWindowName
        dwStyle,                    // dwStyle
        (int)rect.x,                // x
        (int)rect.y,                // y
        (int)rect.width,            // nWidth
        (int)rect.height,           // nHeight
        parent_hwnd,                // hWndParent
        NULL,                       // hMenu
        hInstance,                  // hInstance
        NULL                        // lpParam
    );

    if (!impl->hwnd) {
        fprintf(stderr, "[ui] Failed to create label window\n");
        free(impl);
        return NULL;
    }

    printf("[ui] ✓ Label created: %p\n", impl->hwnd);
    return (UILabel*)impl;
}

void windows_label_set_text(UILabel* label, const char* text) {
    if (!label || !SetWindowTextA) return;

    struct WindowsLabelImpl* impl = (struct WindowsLabelImpl*)label;
    SetWindowTextA(impl->hwnd, text);
}

void windows_label_set_color(UILabel* label, UIColor color) {
    // Note: Setting label color in Win32 requires handling WM_CTLCOLORSTATIC
    // in the window procedure and using GDI functions. This is a simplified
    // implementation that doesn't support color yet.
    // Full implementation would require:
    // 1. Store color in UILabelImpl
    // 2. Handle WM_CTLCOLORSTATIC in WndProc
    // 3. Use SetTextColor/SetBkColor with HDC
    if (!label) return;
}

#endif // _WIN32

// ============================================================================
// Unified API Implementation
// ============================================================================

int ui_init(void) {
    if (g_initialized) {
        return 0;
    }

    printf("[ui] Initializing UI module...\n");

    // Platform detection
    if (IsXnu()) {
        g_platform_name = "macOS";
        #ifdef __APPLE__
        if (macos_init() != 0) return -1;
        #else
        fprintf(stderr, "[ui] macOS detected but not compiled with Apple SDK\n");
        return -1;
        #endif

    } else if (IsLinux()) {
        g_platform_name = "Linux";
        #ifdef __linux__
        if (linux_init() != 0) return -1;
        #else
        fprintf(stderr, "[ui] Linux detected but not compiled for Linux\n");
        return -1;
        #endif

    } else if (IsWindows()) {
        g_platform_name = "Windows";
        #ifdef _WIN32
        if (windows_init() != 0) return -1;
        #else
        fprintf(stderr, "[ui] Windows detected but not compiled for Windows\n");
        return -1;
        #endif

    } else {
        fprintf(stderr, "[ui] Unknown platform\n");
        return -1;
    }

    g_initialized = 1;
    printf("[ui] ✓ UI module initialized on %s\n", g_platform_name);
    return 0;
}

void ui_run(void) {
    if (!g_initialized) {
        fprintf(stderr, "[ui] Not initialized\n");
        return;
    }

    #ifdef __APPLE__
    if (IsXnu()) macos_run();
    #endif

    #ifdef __linux__
    if (IsLinux()) linux_run();
    #endif

    #ifdef _WIN32
    if (IsWindows()) windows_run();
    #endif
}

void ui_quit(void) {
    #ifdef __APPLE__
    if (IsXnu()) macos_quit();
    #endif

    #ifdef __linux__
    if (IsLinux()) linux_quit();
    #endif

    #ifdef _WIN32
    if (IsWindows()) windows_quit();
    #endif
}

void ui_cleanup(void) {
    if (!g_initialized) return;

    #ifdef __APPLE__
    if (IsXnu()) macos_cleanup();
    #endif

    #ifdef __linux__
    if (IsLinux()) linux_cleanup();
    #endif

    #ifdef _WIN32
    if (IsWindows()) windows_cleanup();
    #endif

    g_initialized = 0;
    printf("[ui] ✓ UI module cleaned up\n");
}

const char* ui_get_platform(void) {
    return g_platform_name;
}

// Window API
UIWindow* ui_window_create(const char* title, int width, int height) {
    if (!g_initialized) return NULL;

    #ifdef __APPLE__
    if (IsXnu()) return macos_window_create(title, width, height);
    #endif

    #ifdef __linux__
    if (IsLinux()) return linux_window_create(title, width, height);
    #endif

    #ifdef _WIN32
    if (IsWindows()) return windows_window_create(title, width, height);
    #endif

    return NULL;
}

void ui_window_show(UIWindow* window) {
    if (!window) return;

    #ifdef __APPLE__
    if (IsXnu()) macos_window_show(window);
    #endif

    #ifdef __linux__
    if (IsLinux()) linux_window_show(window);
    #endif

    #ifdef _WIN32
    if (IsWindows()) windows_window_show(window);
    #endif
}

void ui_window_center(UIWindow* window) {
    if (!window) return;

    #ifdef __APPLE__
    if (IsXnu()) macos_window_center(window);
    #endif
}

void ui_window_set_title(UIWindow* window, const char* title) {
    if (!window) return;

    #ifdef __APPLE__
    if (IsXnu()) macos_window_set_title(window, title);
    #endif
}

// Stubs for other APIs
void ui_window_destroy(UIWindow* window) {}
void ui_window_hide(UIWindow* window) {}
void ui_window_set_close_callback(UIWindow* window, UIWindowCloseCallback callback, void* userdata) {}

// Button API
UIButton* ui_button_create(UIWindow* window, const char* title, UIRect rect) {
    if (!g_initialized || !window) return NULL;

    #ifdef __APPLE__
    if (IsXnu()) return macos_button_create(window, title, rect);
    #endif

    #ifdef __linux__
    if (IsLinux()) return linux_button_create(window, title, rect);
    #endif

    #ifdef _WIN32
    if (IsWindows()) return windows_button_create(window, title, rect);
    #endif

    return NULL;
}

void ui_button_set_title(UIButton* button, const char* title) {
    if (!button) return;

    #ifdef __APPLE__
    if (IsXnu()) macos_button_set_title(button, title);
    #endif

    #ifdef __linux__
    if (IsLinux()) linux_button_set_title(button, title);
    #endif

    #ifdef _WIN32
    if (IsWindows()) windows_button_set_title(button, title);
    #endif
}

void ui_button_set_callback(UIButton* button, UIButtonCallback callback, void* userdata) {
    if (!button) return;

    #ifdef __APPLE__
    if (IsXnu()) macos_button_set_callback(button, callback, userdata);
    #endif

    #ifdef __linux__
    if (IsLinux()) linux_button_set_callback(button, callback, userdata);
    #endif

    #ifdef _WIN32
    if (IsWindows()) windows_button_set_callback(button, callback, userdata);
    #endif
}

// Label API
UILabel* ui_label_create(UIWindow* window, const char* text, UIRect rect) {
    if (!g_initialized || !window) return NULL;

    #ifdef __APPLE__
    if (IsXnu()) return macos_label_create(window, text, rect);
    #endif

    #ifdef __linux__
    if (IsLinux()) return linux_label_create(window, text, rect);
    #endif

    #ifdef _WIN32
    if (IsWindows()) return windows_label_create(window, text, rect);
    #endif

    return NULL;
}

void ui_label_set_text(UILabel* label, const char* text) {
    if (!label) return;

    #ifdef __APPLE__
    if (IsXnu()) macos_label_set_text(label, text);
    #endif

    #ifdef __linux__
    if (IsLinux()) linux_label_set_text(label, text);
    #endif

    #ifdef _WIN32
    if (IsWindows()) windows_label_set_text(label, text);
    #endif
}

void ui_label_set_color(UILabel* label, UIColor color) {
    if (!label) return;

    #ifdef __APPLE__
    if (IsXnu()) macos_label_set_color(label, color);
    #endif

    #ifdef __linux__
    if (IsLinux()) linux_label_set_color(label, color);
    #endif

    #ifdef _WIN32
    if (IsWindows()) windows_label_set_color(label, color);
    #endif
}
