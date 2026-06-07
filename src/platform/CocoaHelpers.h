#pragma once
#include <string>
std::string nativeOpenFileDialog(const char* title = "Open Module");
std::string nativeSaveFileDialog(const char* title = "Save Module", const char* defaultName = "");

struct GLFWwindow;

// Call once after window creation.  Installs a local NSEvent monitor so
// the mouseDown event is available to beginWindowDrag at any point.
void setupWindowEventMonitor(GLFWwindow* win);

// Hand the window drag off to the macOS compositor via
// performWindowDragWithEvent:.  Smooth, blink-free, no manual pos math.
// Call this when ImGui detects a mouseDown in the drag zone.
void beginWindowDrag(GLFWwindow* win);

// Round the window's content view corners (macOS layer mask).
void setWindowCornerRadius(GLFWwindow* win, float radius);

// Set the Dock icon from the app bundle's AppIcon.icns resource.
void setDockIcon();

// Native macOS cursor helpers — call each frame from within widget rendering.
void setCursorOpenHand();
void setCursorClosedHand();
