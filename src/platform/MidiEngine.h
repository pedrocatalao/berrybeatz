#pragma once
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <cstdint>

struct CCEvent   { int cc;   int value; };
struct NoteEvent { int note; int vel;   };   // vel==0 means note off

class MidiEngine {
public:
    MidiEngine();
    ~MidiEngine();

    void setSource(int sourceIndex);
    std::vector<CCEvent>   poll();           // drain CC queue; call once per frame
    std::vector<NoteEvent> pollNotes();      // drain note queue (unused when onNoteRT is set)
    int  getPitchBend() const { return m_pitchBend.load(); }  // 0–16383, center=8192

    // When set, note-on/off events are dispatched here directly from the CoreMIDI
    // thread instead of being queued — eliminates the 16ms frame-poll latency.
    // vel==0 means note off. Called from a real-time thread: must be lock-free.
    std::function<void(int note, int vel)> onNoteRT;

public:
    static void readProc(const void* pktlist, void* refCon, void* connRefCon);
private:
    void enqueueCC(int cc, int value);
    void enqueueNote(int note, int vel);
    void storePitchBend(int raw14) { m_pitchBend.store(raw14); }

    uint32_t m_client           {0};
    uint32_t m_port             {0};
    int      m_connectedSource  {-1};

    std::mutex             m_mutex;
    std::vector<CCEvent>   m_queue;
    std::vector<NoteEvent> m_noteQueue;
    std::atomic<int>       m_pitchBend {8192};  // 14-bit, center=8192
};
