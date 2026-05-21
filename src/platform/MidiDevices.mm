#include "MidiDevices.h"
#import <CoreMIDI/CoreMIDI.h>

std::vector<MidiSourceInfo> enumerateMidiSources()
{
    std::vector<MidiSourceInfo> result;
    ItemCount n = MIDIGetNumberOfSources();
    for (ItemCount i = 0; i < n; ++i) {
        MIDIEndpointRef src = MIDIGetSource(i);
        CFStringRef cfName = nullptr;
        MIDIObjectGetStringProperty(src, kMIDIPropertyDisplayName, &cfName);
        std::string name;
        if (cfName) {
            char buf[256];
            CFStringGetCString(cfName, buf, sizeof(buf), kCFStringEncodingUTF8);
            name = buf;
            CFRelease(cfName);
        } else {
            name = "(unnamed)";
        }
        result.push_back({(int)i, std::move(name)});
    }
    return result;
}
