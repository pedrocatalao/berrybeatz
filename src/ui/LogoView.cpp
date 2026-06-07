#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <CoreFoundation/CoreFoundation.h>
#include <ImageIO/ImageIO.h>
#include <vector>
#include "LogoView.h"
#include "imgui.h"

// ── Texture loading ───────────────────────────────────────────────────────────

static GLuint loadPNGTexture(const char* resource, float& outW, float& outH)
{
    outW = outH = 0.f;

    CFBundleRef bundle = CFBundleGetMainBundle();
    if (!bundle) return 0;

    CFStringRef name = CFStringCreateWithCString(nullptr, resource, kCFStringEncodingUTF8);
    CFURLRef url = CFBundleCopyResourceURL(bundle, name, CFSTR("png"), nullptr);
    CFRelease(name);
    if (!url) return 0;

    CGImageSourceRef src = CGImageSourceCreateWithURL(url, nullptr);
    CFRelease(url);
    if (!src) return 0;

    CGImageRef img = CGImageSourceCreateImageAtIndex(src, 0, nullptr);
    CFRelease(src);
    if (!img) return 0;

    size_t w = CGImageGetWidth(img);
    size_t h = CGImageGetHeight(img);

    std::vector<uint8_t> pixels(w * h * 4, 0);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(
        pixels.data(), w, h, 8, w * 4, cs,
        (CGBitmapInfo)(kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big));
    CGContextDrawImage(ctx, CGRectMake(0, 0, (CGFloat)w, (CGFloat)h), img);
    CGContextRelease(ctx);
    CGColorSpaceRelease(cs);
    CGImageRelease(img);

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

GLuint loadLogoTexture(float& outW, float& outH)
{
    // Load normally, then repaint all pixels white using alpha as mask
    // so tint colour works correctly regardless of source pixel colour.
    GLuint tex = 0;
    float w = 0, h = 0;

    // Re-implement load inline so we can mutate pixels before upload
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (!bundle) return 0;
    CFStringRef name = CFStringCreateWithCString(nullptr, "banner", kCFStringEncodingUTF8);
    CFURLRef url = CFBundleCopyResourceURL(bundle, name, CFSTR("png"), nullptr);
    CFRelease(name);
    if (!url) return 0;
    CGImageSourceRef src = CGImageSourceCreateWithURL(url, nullptr);
    CFRelease(url);
    if (!src) return 0;
    CGImageRef img = CGImageSourceCreateImageAtIndex(src, 0, nullptr);
    CFRelease(src);
    if (!img) return 0;

    size_t iw = CGImageGetWidth(img), ih = CGImageGetHeight(img);
    std::vector<uint8_t> pixels(iw * ih * 4, 0);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(pixels.data(), iw, ih, 8, iw*4, cs,
        (CGBitmapInfo)(kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big));
    CGContextDrawImage(ctx, CGRectMake(0,0,(CGFloat)iw,(CGFloat)ih), img);
    CGContextRelease(ctx); CGColorSpaceRelease(cs); CGImageRelease(img);

    // Paint white — premultiplied: store (A,A,A,A) for each pixel
    for (size_t i = 0; i < iw * ih * 4; i += 4) {
        uint8_t a    = pixels[i + 3];
        pixels[i]    = a;
        pixels[i+1]  = a;
        pixels[i+2]  = a;
    }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)iw, (GLsizei)ih, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    outW = (float)iw; outH = (float)ih;
    return tex;
}

// ── LogoView::draw ────────────────────────────────────────────────────────────

void LogoView::draw(float winX, float winY, float winW,
                    GLuint logoTex, float logoW, float logoH)
{
    if (!logoTex || logoH <= 0.f) return;

    const float lh = kHeight;
    const float lw = lh * (logoW / logoH);
    const ImVec2 p0 = {winX + winW - lw - 15.f, winY + (78.f - lh) * 0.5f};
    const ImVec2 p1 = {p0.x + lw, p0.y + lh};

    ImDrawList* fdl = ImGui::GetForegroundDrawList();

    auto drawAt = [&](float ox, float oy, ImU32 col) {
        fdl->AddImage((ImTextureID)(intptr_t)logoTex,
                      {p0.x + ox, p0.y + oy}, {p1.x + ox, p1.y + oy},
                      {0.f, 0.f}, {1.f, 1.f}, col);
    };

    drawAt(1.f, 1.f, IM_COL32(99, 99, 105, 150));  // shadow below

    // Top layer — vertical gradient: opaque at top, transparent at bottom
    constexpr ImU32 kTop = IM_COL32(20, 20, 35, 200);
    constexpr ImU32 kBot = IM_COL32(20, 20, 35,   100);
    fdl->PushTextureID((ImTextureID)(intptr_t)logoTex);
    fdl->PrimReserve(6, 4);
    ImDrawIdx base = (ImDrawIdx)fdl->_VtxCurrentIdx;
    fdl->PrimWriteVtx({p0.x, p0.y}, {0.f, 0.f}, kTop);
    fdl->PrimWriteVtx({p1.x, p0.y}, {1.f, 0.f}, kTop);
    fdl->PrimWriteVtx({p1.x, p1.y}, {1.f, 1.f}, kBot);
    fdl->PrimWriteVtx({p0.x, p1.y}, {0.f, 1.f}, kBot);
    fdl->PrimWriteIdx(base+0); fdl->PrimWriteIdx(base+1); fdl->PrimWriteIdx(base+2);
    fdl->PrimWriteIdx(base+0); fdl->PrimWriteIdx(base+2); fdl->PrimWriteIdx(base+3);
    fdl->PopTextureID();
}
