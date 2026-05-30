#pragma once
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>

// Load PNG assets from the app bundle as GL textures.
GLuint loadLogoTexture(float& outW, float& outH);
GLuint loadSpeakerConeTexture(float& outW, float& outH);

class LogoView {
public:
    void draw(float winX, float winY, float winW,
              GLuint logoTex, float logoW, float logoH,
              GLuint speakerTex, float speakerW, float speakerH,
              float vuLeft, float vuRight);

private:
    static constexpr float kHeight    = 90.f;
    static constexpr float kConeX     = 231.f;
    static constexpr float kConeY     =  38.f;
    static constexpr float kConeScale =   1.0f;
};
