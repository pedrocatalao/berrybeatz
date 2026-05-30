#import <Cocoa/Cocoa.h>
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <vector>
#include "LogoView.h"
#include "RenderSettings.h"
#include "imgui.h"
#include <cmath>
#include <algorithm>

// ── Texture loading ───────────────────────────────────────────────────────────

static GLuint loadPNGTexture(NSString* resource, float& outW, float& outH)
{
    outW = outH = 0.f;
    @autoreleasepool {
        NSString* path = [[NSBundle mainBundle] pathForResource:resource ofType:@"png"];
        if (!path) return 0;

        NSImage* nsImg = [[NSImage alloc] initWithContentsOfFile:path];
        if (!nsImg) return 0;

        CGImageRef cgImg = [nsImg CGImageForProposedRect:nil context:nil hints:nil];
        if (!cgImg) return 0;

        size_t w = CGImageGetWidth(cgImg);
        size_t h = CGImageGetHeight(cgImg);

        std::vector<uint8_t> pixels(w * h * 4, 0);
        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx = CGBitmapContextCreate(
            pixels.data(), w, h, 8, w * 4, cs,
            kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
        CGContextDrawImage(ctx, CGRectMake(0, 0, (CGFloat)w, (CGFloat)h), cgImg);
        CGContextRelease(ctx);
        CGColorSpaceRelease(cs);

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     (GLsizei)w, (GLsizei)h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);

        outW = (float)w;
        outH = (float)h;
        return tex;
    }
}

GLuint loadLogoTexture(float& outW, float& outH)      { return loadPNGTexture(@"banner",  outW, outH); }
GLuint loadSpeakerConeTexture(float& outW, float& outH){ return loadPNGTexture(@"speaker", outW, outH); }

// ── LogoView::draw ────────────────────────────────────────────────────────────

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

    // Gradient mask constrained to logo's visible pixels (texture alpha clips it)
    {
        ImU32 cL = IM_COL32(50, 50, 60, 100);
        ImU32 cR = IM_COL32(0, 0, 0,  0);
        fdl->PushTextureID((ImTextureID)(intptr_t)logoTex);
        fdl->PrimReserve(6, 4);
        ImDrawIdx base = (ImDrawIdx)fdl->_VtxCurrentIdx;
        fdl->PrimWriteVtx({p0.x,      p0.y     }, {0.f, 0.f}, cL);
        fdl->PrimWriteVtx({p0.x + lw, p0.y     }, {1.f, 0.f}, cR);
        fdl->PrimWriteVtx({p0.x + lw, p0.y + lh}, {1.f, 1.f}, cR);
        fdl->PrimWriteVtx({p0.x,      p0.y + lh}, {0.f, 1.f}, cL);
        fdl->PrimWriteIdx(base+0); fdl->PrimWriteIdx(base+1); fdl->PrimWriteIdx(base+2);
        fdl->PrimWriteIdx(base+0); fdl->PrimWriteIdx(base+2); fdl->PrimWriteIdx(base+3);
        fdl->PopTextureID();
    }

    if (speakerTex) {
        const float coneW = speakerW * (lh / 463.f) * kConeScale;
        const float coneH = speakerH * (lh / 463.f) * kConeScale;

        const float vuPeak = std::min((vuLeft + vuRight) * 0.5f, 1.0f);
        const float vibX   = std::sin((float)ImGui::GetTime() * 2.f * 3.14159265f * 28.f) * vuPeak * 2.f;

        ImVec2 cTL = {p0.x + kConeX + vibX,          p0.y + kConeY        };
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
