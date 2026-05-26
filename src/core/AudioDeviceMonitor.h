#pragma once
#include <functional>

// Watches for macOS default output device changes and fires a callback.
class AudioDeviceMonitor {
public:
    explicit AudioDeviceMonitor(std::function<void()> onChanged);
    ~AudioDeviceMonitor();

    AudioDeviceMonitor(const AudioDeviceMonitor&)            = delete;
    AudioDeviceMonitor& operator=(const AudioDeviceMonitor&) = delete;

private:
    std::function<void()> m_onChanged;
};
