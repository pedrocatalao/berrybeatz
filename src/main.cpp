#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "App.h"
#include "CocoaHelpers.h"
#include <cstdio>
#include <memory>

static App* g_app = nullptr;

static void dropCallback(GLFWwindow*, int count, const char** paths)
{
    if (g_app && count > 0)
        g_app->openFile(paths[0]);
}

int main(int argc, char* argv[])
{
    if (!glfwInit()) {
        fprintf(stderr, "glfwInit failed\n");
        return 1;
    }
    setDockIcon();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);  // no native chrome — we draw our own

    GLFWwindow* window = glfwCreateWindow(1280, 800, "BerryBeatz", nullptr, nullptr);
    if (!window) {
        fprintf(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // vsync

    // ── ImGui setup ───────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename  = nullptr;  // no imgui.ini clutter

    // Style: dark, square, dense
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 0.f;
    style.FrameRounding     = 0.f;
    style.ScrollbarRounding = 2.f;
    style.TabRounding       = 3.f;
    style.WindowBorderSize  = 0.f;
    style.FrameBorderSize   = 0.f;
    style.Colors[ImGuiCol_WindowBg]  = ImVec4(0.08f, 0.08f, 0.13f, 1.f);
    style.Colors[ImGuiCol_TitleBg]   = ImVec4(0.08f, 0.08f, 0.13f, 1.f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.20f, 1.f);

    ImGui_ImplGlfw_InitForOpenGL(window, false);  // we install callbacks manually below
    ImGui_ImplOpenGL3_Init("#version 150");

    // ── Fonts ─────────────────────────────────────────────────────────────
    ImFont* fontMono = nullptr;
    ImFont* fontUI   = nullptr;
    {
        // Pixel-sharp config for bitmap-style fonts — no oversampling
        ImFontConfig pixCfg;
        pixCfg.OversampleH = 1;
        pixCfg.OversampleV = 1;
        pixCfg.PixelSnapH  = true;

        // Smooth config for the UI font
        ImFontConfig uiCfg;
        uiCfg.OversampleH = 2;
        uiCfg.OversampleV = 2;

        // BigBlue Terminal: authentic IBM VGA/DOS bitmap font for patterns
        fontMono = io.Fonts->AddFontFromFileTTF(
            "/Users/pedro/Library/Fonts/BigBlueTermPlusNerdFontMono-Regular.ttf",
            11.f, &pixCfg);

        // Monaco: classic Mac monospace, clean retro feel for UI
        fontUI = io.Fonts->AddFontFromFileTTF(
            "/System/Library/Fonts/Monaco.ttf",
            13.f, &uiCfg);

        // Fall back to default if fonts not found
        if (!fontMono) fontMono = io.Fonts->AddFontDefault();
        if (!fontUI)   fontUI   = io.Fonts->AddFontDefault();

        io.Fonts->Build();
    }

    // ── Application ───────────────────────────────────────────────────────
    auto app = std::make_unique<App>();
    app->setFonts(fontMono, fontUI);
    g_app = app.get();

    app->setGLFWWindow(window);
    setupWindowEventMonitor(window);
    glfwSetDropCallback(window, dropCallback);

    // ── GLFW callbacks ────────────────────────────────────────────────────
    // Forward all standard events to ImGui, then handle toolbar drag ourselves.
    // Drag is initiated here — inside GLFW's mouseDown: delegate chain — which
    // is the only context where performWindowDragWithEvent: is valid on macOS.
    glfwSetWindowFocusCallback(window, ImGui_ImplGlfw_WindowFocusCallback);
    glfwSetCursorEnterCallback(window, ImGui_ImplGlfw_CursorEnterCallback);
    glfwSetCursorPosCallback  (window, ImGui_ImplGlfw_CursorPosCallback);
    glfwSetScrollCallback     (window, ImGui_ImplGlfw_ScrollCallback);
    glfwSetKeyCallback        (window, ImGui_ImplGlfw_KeyCallback);
    glfwSetCharCallback       (window, ImGui_ImplGlfw_CharCallback);

    glfwSetMouseButtonCallback(window, [](GLFWwindow* win, int button, int action, int mods) {
        ImGui_ImplGlfw_MouseButtonCallback(win, button, action, mods);

        // Left press in the toolbar drag zone (top 60 px, no ImGui item hovered)
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
            double x, y;
            glfwGetCursorPos(win, &x, &y);
            if (y < 78.0 && !ImGui::IsAnyItemHovered()) {
                // performWindowDragWithEvent: blocks until mouse-up.
                // Runs a nested NSRunLoop — render loop resumes afterwards.
                beginWindowDrag(win);
            }
        }
    });

    if (argc > 1)
        app->openFile(argv[1]);

    // ── Render loop ───────────────────────────────────────────────────────
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Logical size for ImGui layout; framebuffer size for OpenGL viewport
        int winW, winH, fbW, fbH;
        glfwGetWindowSize(window, &winW, &winH);
        glfwGetFramebufferSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        glClearColor(0.08f, 0.08f, 0.13f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        app->render(winW, winH);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // ── Cleanup ───────────────────────────────────────────────────────────
    g_app = nullptr;
    app.reset();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
