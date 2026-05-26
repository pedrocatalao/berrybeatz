#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include "LogoView.h"
#include "RenderSettings.h"
#include "imgui.h"
#include <cmath>
#include <algorithm>

void LogoView::draw(float winX, float winY, float winW,
                    GLuint logoTex, float logoW, float logoH,
                    GLuint speakerTex, float speakerW, float speakerH,
                    float vuLeft, float vuRight)
{
    if (!logoTex || logoH <= 0.f) return;

    const float lh = kHeight;
    const float lw = lh * (logoW / logoH);
    ImVec2 p0 = {winX + winW - lw - 6.f, winY + 6.f};

    ImDrawList* fdl = ImGui::GetForegroundDrawList();

    auto drawQuad = [&](float ox, float oy, ImU32 col) {
        fdl->AddImageQuad(
            (ImTextureID)(intptr_t)logoTex,
            {p0.x + ox,      p0.y + oy     },
            {p0.x + lw + ox, p0.y + oy     },
            {p0.x + lw + ox, p0.y + lh + oy},
            {p0.x + ox,      p0.y + lh + oy},
            {0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f},
            col);
    };

    fdl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    }, nullptr);

    if (g_glowLogo > 0.f) {
        auto ga = [](int a) { return (uint8_t)(a * g_glowLogo); };
        static const float kDirs[8][2] = {
            {-1.f, 0.f},{1.f, 0.f},{0.f,-1.f},{0.f, 1.f},
            {-1.f,-1.f},{1.f,-1.f},{-1.f,1.f},{1.f, 1.f}
        };
        for (auto& d : kDirs) drawQuad(d[0]*5.f,  d[1]*5.f,  IM_COL32(160, 160, 255, ga( 8)));
        for (auto& d : kDirs) drawQuad(d[0]*3.f,  d[1]*3.f,  IM_COL32(180, 180, 255, ga(13)));
        for (auto& d : kDirs) drawQuad(d[0]*1.5f, d[1]*1.5f, IM_COL32(200, 200, 255, ga(18)));
    }

    fdl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }, nullptr);

    drawQuad(0.f, 0.f, IM_COL32(255, 255, 255, 240));

    if (speakerTex) {
        const float coneW = speakerW * (lh / 463.f) * kConeScale;
        const float coneH = speakerH * (lh / 463.f) * kConeScale;

        const float vuPeak = std::min((vuLeft + vuRight) * 0.5f, 1.0f);
        const float vibX   = std::sin((float)ImGui::GetTime() * 2.f * 3.14159265f * 28.f) * vuPeak * 2.f;

        ImVec2 cTL = {p0.x + kConeX + vibX,         p0.y + kConeY        };
        ImVec2 cTR = {p0.x + kConeX + coneW + vibX,  p0.y + kConeY        };
        ImVec2 cBR = {p0.x + kConeX + coneW + vibX,  p0.y + kConeY + coneH};
        ImVec2 cBL = {p0.x + kConeX + vibX,          p0.y + kConeY + coneH};

        fdl->AddImageQuad(
            (ImTextureID)(intptr_t)speakerTex,
            cTL, cTR, cBR, cBL,
            {0.f,0.f}, {1.f,0.f}, {1.f,1.f}, {0.f,1.f},
            IM_COL32(255, 255, 255, 255));
    }
}
