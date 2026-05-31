#pragma once
#include <string>
#include <memory>
#include "Transport.h"

class SoundModule;

struct SongDocument {
    std::unique_ptr<SoundModule> mod;
    std::string path;
    std::string displayName;
    Transport   transport {Transport::Stopped};
    double      position  {0.0};
};
