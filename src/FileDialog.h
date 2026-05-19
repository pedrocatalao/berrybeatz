#pragma once
#include <string>
std::string nativeOpenFileDialog(const char* title = "Open Module");
std::string nativeSaveFileDialog(const char* title = "Save Module", const char* defaultName = "");
