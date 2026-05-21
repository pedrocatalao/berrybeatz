#include "MidiEngine.h"
#import <CoreMIDI/CoreMIDI.h>

// CoreMIDI types are typedef'd UInt32 — same as uint32_t on LP64 macOS
#define CLIENT ((MIDIClientRef)m_client)
#define PORT   ((MIDIPortRef)m_port)

// File-static callback with the exact MIDIReadProc signature CoreMIDI requires
static void midiReadCallback(const MIDIPacketList* pktlist, void* refCon, void* connRefCon)
{
    MidiEngine::readProc(pktlist, refCon, connRefCon);
}

MidiEngine::MidiEngine()
{
    MIDIClientRef client = 0;
    MIDIClientCreate(CFSTR("BerryBeatz"), nullptr, nullptr, &client);
    m_client = (uint32_t)client;

    MIDIPortRef port = 0;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    MIDIInputPortCreate(CLIENT, CFSTR("Input"), midiReadCallback, this, &port);
#pragma clang diagnostic pop
    m_port = (uint32_t)port;
}

MidiEngine::~MidiEngine()
{
    setSource(-1);
    if (m_port)   MIDIPortDispose(PORT);
    if (m_client) MIDIClientDispose(CLIENT);
}

void MidiEngine::setSource(int sourceIndex)
{
    if (m_connectedSource >= 0 && m_port) {
        MIDIEndpointRef src = MIDIGetSource((ItemCount)m_connectedSource);
        MIDIPortDisconnectSource(PORT, src);
        m_connectedSource = -1;
    }
    if (sourceIndex >= 0 && m_port) {
        MIDIEndpointRef src = MIDIGetSource((ItemCount)sourceIndex);
        if (src != 0) {
            MIDIPortConnectSource(PORT, src, nullptr);
            m_connectedSource = sourceIndex;
        }
    }
    std::lock_guard<std::mutex> lk(m_mutex);
    m_queue.clear();
}

std::vector<CCEvent> MidiEngine::poll()
{
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<CCEvent> result;
    result.swap(m_queue);
    return result;
}

void MidiEngine::enqueue(int cc, int value)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    m_queue.push_back({cc, value});
}

void MidiEngine::readProc(const void* pktlistVoid, void* refCon, void*)
{
    auto* self = static_cast<MidiEngine*>(refCon);
    const auto* pktlist = static_cast<const MIDIPacketList*>(pktlistVoid);
    const MIDIPacket* pkt = &pktlist->packet[0];

    for (UInt32 i = 0; i < pktlist->numPackets; ++i) {
        for (UInt16 j = 0; j < pkt->length; ) {
            uint8_t status = pkt->data[j];
            uint8_t type   = status & 0xF0;
            if (type == 0xB0 && j + 2 < pkt->length) {     // Control Change
                self->enqueue(pkt->data[j+1] & 0x7F, pkt->data[j+2] & 0x7F);
                j += 3;
            } else if ((type == 0x80 || type == 0x90 ||     // Note On/Off
                        type == 0xA0 || type == 0xE0) &&    // Aftertouch / Pitch Bend
                       j + 2 < pkt->length) {
                j += 3;
            } else if ((type == 0xC0 || type == 0xD0) &&   // Program / Channel Pressure
                       j + 1 < pkt->length) {
                j += 2;
            } else if (status == 0xF0) {                    // SysEx — skip rest
                break;
            } else {
                ++j;
            }
        }
        pkt = MIDIPacketNext(pkt);
    }
}
