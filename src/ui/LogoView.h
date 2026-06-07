#pragma once
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>

// Load PNG assets from the app bundle as GL textures.
GLuint loadLogoTexture(float& outW, float& outH);

class LogoView {
public:
    void draw(float winX, float winY, float winW,
              GLuint logoTex, float logoW, float logoH);

private:
    static constexpr float kHeight = 54.f;
};
