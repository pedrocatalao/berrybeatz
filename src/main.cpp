#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "App.h"
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

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 800, "OpenMPT", nullptr, nullptr);
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
    style.TabRounding       = 0.f;
    style.WindowBorderSize  = 0.f;
    style.FrameBorderSize   = 0.f;
    style.Colors[ImGuiCol_WindowBg]  = ImVec4(0.08f, 0.08f, 0.13f, 1.f);
    style.Colors[ImGuiCol_TitleBg]   = ImVec4(0.08f, 0.08f, 0.13f, 1.f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.20f, 1.f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    // ── Fonts ─────────────────────────────────────────────────────────────
    ImFont* fontMono = nullptr;
    ImFont* fontUI   = nullptr;
    {
        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.OversampleV = 2;

        // Monospaced font for the pattern editor
        fontMono = io.Fonts->AddFontFromFileTTF(
            "/System/Library/Fonts/Menlo.ttc", 13.f, &cfg);

        // UI font (Helvetica Neue) for toolbar and controls
        fontUI = io.Fonts->AddFontFromFileTTF(
            "/System/Library/Fonts/HelveticaNeue.ttc", 13.f, &cfg);

        // Fall back to default if system font not found
        if (!fontMono) fontMono = io.Fonts->AddFontDefault();
        if (!fontUI)   fontUI   = io.Fonts->AddFontDefault();

        io.Fonts->Build();
    }

    // ── Application ───────────────────────────────────────────────────────
    auto app = std::make_unique<App>();
    app->setFonts(fontMono, fontUI);
    g_app = app.get();

    glfwSetDropCallback(window, dropCallback);

    if (argc > 1)
        app->openFile(argv[1]);

    // ── Render loop ───────────────────────────────────────────────────────
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        glClearColor(0.08f, 0.08f, 0.13f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        app->render(fbW, fbH);

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
