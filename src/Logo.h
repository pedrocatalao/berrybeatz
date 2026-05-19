#pragma once
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>

// Loads assets/logo.png from the app bundle's Resources folder into a GL texture.
// Returns 0 on failure. outW/outH receive the pixel dimensions.
GLuint loadLogoTexture(float& outW, float& outH);
