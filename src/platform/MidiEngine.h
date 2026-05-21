#pragma once
#include <vector>
#include <mutex>
#include <cstdint>

struct CCEvent { int cc; int value; };

class MidiEngine {
public:
    MidiEngine();
    ~MidiEngine();

    void setSource(int sourceIndex);       // -1 = disconnect
    std::vector<CCEvent> poll();           // drain queue; call once per frame from UI thread

public:
    static void readProc(const void* pktlist, void* refCon, void* connRefCon);
private:
    void enqueue(int cc, int value);

    uint32_t m_client           {0};
    uint32_t m_port             {0};
    int      m_connectedSource  {-1};

    std::mutex           m_mutex;
    std::vector<CCEvent> m_queue;
};
