#include "AudioDeviceMonitor.h"
#include <CoreAudio/CoreAudio.h>

static const AudioObjectPropertyAddress kDefaultOutputDeviceAddr {
    kAudioHardwarePropertyDefaultOutputDevice,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMain
};

static OSStatus listenerCallback(AudioObjectID, UInt32,
                                  const AudioObjectPropertyAddress*,
                                  void* userData)
{
    (*static_cast<std::function<void()>*>(userData))();
    return noErr;
}

AudioDeviceMonitor::AudioDeviceMonitor(std::function<void()> onChanged)
    : m_onChanged(std::move(onChanged))
{
    AudioObjectAddPropertyListener(kAudioObjectSystemObject,
                                   &kDefaultOutputDeviceAddr,
                                   listenerCallback, &m_onChanged);
}

AudioDeviceMonitor::~AudioDeviceMonitor()
{
    AudioObjectRemovePropertyListener(kAudioObjectSystemObject,
                                      &kDefaultOutputDeviceAddr,
                                      listenerCallback, &m_onChanged);
}
