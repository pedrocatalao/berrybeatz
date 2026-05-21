#pragma once
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>

// Returns 0 on failure. outW/outH receive the pixel dimensions.
GLuint loadLogoTexture(float& outW, float& outH);
GLuint loadSpeakerConeTexture(float& outW, float& outH);
