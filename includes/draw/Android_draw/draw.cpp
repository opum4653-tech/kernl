#include "Android_draw/draw.h"
#include "Font.h"
#include "globals.hpp"
#include <dlfcn.h>

typedef void (*pfn_ANW_acquire)(ANativeWindow*);
typedef int32_t (*pfn_ANW_setBuffersGeometry)(ANativeWindow*, int32_t, int32_t, int32_t);
typedef void (*pfn_ANW_release)(ANativeWindow*);

static pfn_ANW_acquire g_fn_anw_acquire = nullptr;
static pfn_ANW_setBuffersGeometry g_fn_anw_setBuffersGeometry = nullptr;
static pfn_ANW_release g_fn_anw_release = nullptr;

static void init_anw_funcs() {
    static bool inited = false;
    if (inited) return;
    inited = true;
    void* h = dlopen("libnativewindow.so", RTLD_NOW | RTLD_LOCAL);
    if (!h) h = dlopen("/system/lib64/libnativewindow.so", RTLD_NOW | RTLD_LOCAL);
    if (!h) h = dlopen("/system/lib/libnativewindow.so", RTLD_NOW | RTLD_LOCAL);
    if (!h) h = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
    if (!h) h = dlopen("/system/lib64/libandroid.so", RTLD_NOW | RTLD_LOCAL);
    if (!h) h = dlopen("/system/lib/libandroid.so", RTLD_NOW | RTLD_LOCAL);
    if (h) {
        g_fn_anw_acquire = reinterpret_cast<pfn_ANW_acquire>(dlsym(h, "ANativeWindow_acquire"));
        g_fn_anw_setBuffersGeometry = reinterpret_cast<pfn_ANW_setBuffersGeometry>(dlsym(h, "ANativeWindow_setBuffersGeometry"));
        g_fn_anw_release = reinterpret_cast<pfn_ANW_release>(dlsym(h, "ANativeWindow_release"));
    }
}

EGLDisplay display = EGL_NO_DISPLAY;
EGLConfig config;
EGLSurface surface = EGL_NO_SURFACE;
EGLContext context = EGL_NO_CONTEXT;

ANativeWindow *native_window;

int native_window_screen_x = 0;
int native_window_screen_y = 0;
android::ANativeWindowCreator::DisplayInfo displayInfo{0};
uint32_t orientation = 999;
bool g_Initialized = false;

bool initGUI_draw(uint32_t _screen_x, uint32_t _screen_y, bool log, bool hide) {

    #if defined(USE_OPENGL)
        if (!init_egl(_screen_x, _screen_y, log, hide)) {
            return false;
        }
    #else
        InitVulkan();
        SetupVulkan();
        ::native_window = android::ANativeWindowCreator::Create("AImGui", _screen_x, _screen_y, false, false);
        SetupVulkanWindow(::native_window, (int) _screen_x, (int) _screen_y);
    #endif
    if (!ImGui_init()) {
        return false;
    }   

    #ifndef USE_OPENGL
        UploadFonts();
    #endif
     
    return true;
}

bool init_egl(uint32_t _screen_x, uint32_t _screen_y, bool log, bool hide) {
    ::native_window = android::ANativeWindowCreator::Create("AImGui", _screen_x, _screen_y, false, false);
    if (!::native_window) {
        return false;
    }

    init_anw_funcs();
    if (g_fn_anw_acquire) g_fn_anw_acquire(native_window);
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        return false;
    }
    if (eglInitialize(display, 0, 0) != EGL_TRUE) {
        return false;
    }
    EGLint num_config = 0;
    const EGLint attribList[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_BLUE_SIZE, 5,
        EGL_GREEN_SIZE, 6,
        EGL_RED_SIZE, 5,
        EGL_BUFFER_SIZE, 32,
        EGL_DEPTH_SIZE, 16,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };
    const EGLint attrib_list[] = {
        EGL_CONTEXT_CLIENT_VERSION,
        3,
        EGL_NONE
    };

    if (eglChooseConfig(display, attribList, &config, 1, &num_config) != EGL_TRUE || num_config <= 0) {
        // Fallback config without strict depth/stencil
        const EGLint fallbackAttribList[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_NONE
        };
        if (eglChooseConfig(display, fallbackAttribList, &config, 1, &num_config) != EGL_TRUE || num_config <= 0) {
            return false;
        }
    }
    EGLint egl_format = 0;
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &egl_format);
    if (g_fn_anw_setBuffersGeometry) g_fn_anw_setBuffersGeometry(native_window, 0, 0, egl_format);
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, attrib_list);
    if (context == EGL_NO_CONTEXT) {
        const EGLint gles2_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, gles2_attribs);
        if (context == EGL_NO_CONTEXT) {
            return false;
        }
    }
    surface = eglCreateWindowSurface(display, config, native_window, nullptr);
    if (surface == EGL_NO_SURFACE) {
        return false;
    }
    if (!eglMakeCurrent(display, surface, surface, context)) {
        return false;
    }
    return true;
}

void screen_config() {
    displayInfo = android::ANativeWindowCreator::GetDisplayInfo();
    if (displayInfo.width <= 0) displayInfo.width = 1080;
    if (displayInfo.height <= 0) displayInfo.height = 2400;
}
bool ImGui_init() {
    if (g_Initialized) {
        return true;
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplAndroid_Init(native_window);
    #if defined(USE_OPENGL)
        ImGui_ImplOpenGL3_Init("#version 300 es");
    #endif
    ImGuiIO &io = ImGui::GetIO();
    ImGuiStyle *style = &ImGui::GetStyle();
    style->WindowTitleAlign = ImVec2(0.5, 0.5);
    style->PopupRounding = 3;
    style->WindowPadding = ImVec2(4, 4);
    style->FramePadding  = ImVec2(2, 2);
    style->ItemSpacing   = ImVec2(2, 2);
    style->ScrollbarSize = 10;
    style->WindowBorderSize = 3;
    style->ChildBorderSize  = 1;
    style->PopupBorderSize  = 1;
    style->FrameBorderSize  = 1;
    style->WindowRounding    = 3;
    style->ChildRounding     = 3;
    style->FrameRounding     = 3;
    style->ScrollbarRounding = 2;
    style->GrabRounding      = 3;
    
    io.IniFilename = NULL;

    // FontDataOwnedByAtlas MUST stay false for every static TTF blob.
    //
    // The mechanism, precisely: ImFontAtlas::AddFont() only takes a private
    // heap copy when the flag is FALSE —
    //     if (!cfg.FontDataOwnedByAtlas) { cfg.FontData = IM_ALLOC(size);
    //                                      memcpy(...); cfg.FontDataOwnedByAtlas = true; }
    // — and when the flag is TRUE it keeps the caller's pointer as-is. Roboto_Regular
    // and the MAIN_FONT blobs are const arrays in .rodata, so the default (true)
    // meant the atlas stored a .rodata pointer and ImFontAtlas::ClearInputData()
    // later IM_FREE()d it. That is a free() on a non-heap address on every
    // shutdown (ImGui::DestroyContext -> ~ImFontAtlas -> Clear), which corrupts
    // the allocator's free-list bookkeeping.
    //
    // false makes ImGui copy the blob to the heap and own the copy, so the
    // matching free is legitimate. The ownership transfer is the fix; the flag
    // name reads backwards from what it does here.
    static ImFontConfig s_fontCfg;
    s_fontCfg.FontDataOwnedByAtlas = false;
    s_fontCfg.SizePixels = 22.0f;
    s_fontCfg.OversampleH = 2;
    s_fontCfg.OversampleV = 1;

    // Fonts[0]. The menu layout (every 17/14/12px AddText plus ScaleAllSizes)
    // was tuned against this 32px face, so it stays the default text face.
    io.Fonts->AddFontFromMemoryTTF((void*)Roboto_Regular, sizeof(Roboto_Regular),
                                   32.0f, &s_fontCfg, io.Fonts->GetGlyphRangesChineseFull());

    // AddFontDefault -> AddFontFromMemoryCompressedTTF allocates a heap buffer
    // and forces FontDataOwnedByAtlas = true itself, so the flag needs no
    // override here - only the size bump matters.
    ImFontConfig s_defCfg;
    s_defCfg.SizePixels = 22.0f;
    io.Fonts->AddFontDefault(&s_defCfg);

    // Dedicated faces for the places that were sampling the 32px CJK menu face
    // at a very different size. Both are ASCII-only by design: their only
    // consumers are literals ("GUN", "ARMED", "KERNEL CORE", weapon names) and
    // an address hit on the player-name path, which keeps the CJK-capable
    // Fonts[0] so Chinese/Cyrillic names still render.
    //
    //  - ESP loot/weapon pills draw at 10px. Sampling a 32px raster down to 10
    //    loses the stem, so the text looked thin and the advance was skewed.
    //  - Menu toggle-row labels draw at 17px.
    ImFontConfig s_smallCfg;
    s_smallCfg.FontDataOwnedByAtlas = false;
    s_smallCfg.SizePixels = 14.0f;
    s_smallCfg.OversampleH = 2;
    s_smallCfg.OversampleV = 2;
    ::g_EspFont = io.Fonts->AddFontFromMemoryTTF((void*)Roboto_Regular, sizeof(Roboto_Regular),
                                                 14.0f, &s_smallCfg, io.Fonts->GetGlyphRangesDefault());

    // Menu body text: the toggle rows draw their labels at 17px.
    ImFontConfig s_uiCfg;
    s_uiCfg.FontDataOwnedByAtlas = false;
    s_uiCfg.SizePixels = 18.0f;
    s_uiCfg.OversampleH = 2;
    s_uiCfg.OversampleV = 2;
    ::g_UiFont = io.Fonts->AddFontFromMemoryTTF((void*)Roboto_Regular, sizeof(Roboto_Regular),
                                                18.0f, &s_uiCfg, io.Fonts->GetGlyphRangesDefault());

    // Single-glyph face: only 'N' is ever drawn with it, so the atlas cost is
    // one 64px quad instead of a full Cyrillic set at 64px.
    static const ImWchar s_logoGlyphs[] = { 'N', 'N', 0 };
    ImFontConfig s_logoCfg;
    s_logoCfg.FontDataOwnedByAtlas = false;
    s_logoCfg.SizePixels = 64.0f;
    s_logoCfg.OversampleH = 2;
    s_logoCfg.OversampleV = 2;
    ::g_LogoFont = io.Fonts->AddFontFromMemoryTTF((void*)Roboto_Regular, sizeof(Roboto_Regular),
                                                  64.0f, &s_logoCfg, s_logoGlyphs);

    ImGui::GetStyle().ScaleAllSizes(3.0f);
    ::g_Initialized = true;
    return true;
}

void drawBegin() {
    screen_config();
    if (::orientation != displayInfo.orientation) {
        ::orientation = displayInfo.orientation;

        // Rotate the viewport with the panel. Leaving native_window_screen_*
        // stale here fed the wrong size to ImGui_ImplAndroid_NewFrame, which
        // sets io.DisplaySize, so the first frame after a rotation laid the
        // menu out against the previous orientation's dimensions.
        int newW = (displayInfo.height > displayInfo.width ? displayInfo.height : displayInfo.width);
        int newH = (displayInfo.height < displayInfo.width ? displayInfo.height : displayInfo.width);
        ::native_window_screen_x = newW;
        ::native_window_screen_y = newH;
        ::g_sw = static_cast<float>(newW);
        ::g_sh = static_cast<float>(newH);

        UpdateScreenData(displayInfo.width, displayInfo.height, displayInfo.orientation);
    }
    
    #ifdef USE_OPENGL
        ImGui_ImplOpenGL3_NewFrame();
    #else
        ImGui_ImplVulkan_NewFrame();
    #endif        
    ImGui_ImplAndroid_NewFrame(native_window_screen_x, native_window_screen_y);
    ImGui::NewFrame();
}

void drawEnd() {
    ImGui::Render();
    
    #ifdef USE_OPENGL
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        eglSwapBuffers(display, surface);
    #else
        FrameRender(ImGui::GetDrawData());
        FramePresent();
    #endif
}



void shutdown() {
    if (!g_Initialized) {
        return;
    }
    // Cleanup
    #ifdef USE_OPENGL
        ImGui_ImplOpenGL3_Shutdown();
    #else
        DeviceWait();
        ImGui_ImplVulkan_Shutdown();
    #endif
    ImGui_ImplAndroid_Shutdown();
    ImGui::DestroyContext();
    
    
    #ifdef USE_OPENGL
        if (display != EGL_NO_DISPLAY) {
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (context != EGL_NO_CONTEXT) {
                eglDestroyContext(display, context);
            }
            if (surface != EGL_NO_SURFACE) {
                eglDestroySurface(display, surface);
            }
            eglTerminate(display);
        }
        display = EGL_NO_DISPLAY;
        context = EGL_NO_CONTEXT;
        surface = EGL_NO_SURFACE;
    #else    
        CleanupVulkanWindow();
        CleanupVulkan();
    #endif
    
    if (g_fn_anw_release) g_fn_anw_release(native_window);
    android::ANativeWindowCreator::Destroy(native_window);
    ::g_Initialized = false;
}
