#pragma once
#include <vector>
#include <mutex>
#include <cstdint>

struct CCEvent   { int cc;   int value; };
struct NoteEvent { int note; int vel;   };   // vel==0 means note off

class MidiEngine {
public:
    MidiEngine();
    ~MidiEngine();

    void setSource(int sourceIndex);
    std::vector<CCEvent>   poll();           // drain CC queue; call once per frame
    std::vector<NoteEvent> pollNotes();      // drain note queue; call once per frame

public:
    static void readProc(const void* pktlist, void* refCon, void* connRefCon);
private:
    void enqueueCC(int cc, int value);
    void enqueueNote(int note, int vel);

    uint32_t m_client           {0};
    uint32_t m_port             {0};
    int      m_connectedSource  {-1};

    std::mutex             m_mutex;
    std::vector<CCEvent>   m_queue;
    std::vector<NoteEvent> m_noteQueue;
};
