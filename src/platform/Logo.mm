#import <Cocoa/Cocoa.h>
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <vector>
#include "Logo.h"

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

GLuint loadLogoTexture(float& outW, float& outH)
{
    return loadPNGTexture(@"banner", outW, outH);
}

GLuint loadSpeakerConeTexture(float& outW, float& outH)
{
    return loadPNGTexture(@"speaker", outW, outH);
}
