#include "EXS24Loader.h"
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <dirent.h>
#include <algorithm>
#include <unordered_map>

// ── Block parsing ────────────────────────────────────────────────────────────

static bool fileExists(const std::string& p)
{
    struct stat st{};
    return stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

// Try to find an audio file given a folder path and a filename.
// Searches for .caf, .aif, .aiff, .wav variants if exact match fails.
static std::string resolveAudioFile(const std::string& folder, const std::string& filename)
{
    // 1. Exact path
    std::string exact = folder + "/" + filename;
    if (fileExists(exact)) return exact;

    // 2. Strip extension, try alternate extensions
    static const char* kExts[] = {".caf", ".aif", ".aiff", ".wav", ".CAF", ".AIF", nullptr};
    auto dot = filename.rfind('.');
    std::string base = (dot != std::string::npos) ? filename.substr(0, dot) : filename;

    for (int i = 0; kExts[i]; ++i) {
        std::string candidate = folder + "/" + base + kExts[i];
        if (fileExists(candidate)) return candidate;
    }

    // 3. Search siblings and common Logic/GarageBand sample roots
    static const char* kRoots[] = {
        "/Library/Application Support/Logic/EXS Factory Samples",
        "/Library/Application Support/GarageBand/Instrument Library/Sampler/Sampler Files",
        nullptr
    };
    for (int r = 0; kRoots[r]; ++r) {
        // Try root/folder_basename/filename
        std::string folderBase = folder.substr(folder.rfind('/') + 1);
        for (int i = 0; kExts[i]; ++i) {
            std::string c = std::string(kRoots[r]) + "/" + folderBase + "/" + base + kExts[i];
            if (fileExists(c)) return c;
        }
        // Try root/filename directly
        for (int i = 0; kExts[i]; ++i) {
            std::string c = std::string(kRoots[r]) + "/" + base + kExts[i];
            if (fileExists(c)) return c;
        }
    }
    return {};
}

// ── Name helper ─────────────────────────────────────────────────────────────

static std::string lowered(const std::string& s)
{
    std::string r = s;
    for (auto& c : r) c = (char)tolower(c);
    return r;
}

// Returns true if 'needle' appears as a substring (case-insensitive) of 'haystack'.
static bool containsCI(const std::string& haystack, const std::string& needle)
{
    if (needle.empty()) return false;
    std::string h = lowered(haystack);
    std::string n = lowered(needle);
    return h.find(n) != std::string::npos;
}

// ── EXS24 block scanner ──────────────────────────────────────────────────────

static uint32_t readU32LE(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}

struct RawBlock {
    int         type;     // byte[3] & 0x0F
    uint32_t    id;
    std::string name;     // from TBOS header
    std::vector<uint8_t> payload;
};

static std::vector<RawBlock> scanBlocks(const std::vector<uint8_t>& data)
{
    std::vector<RawBlock> blocks;
    size_t pos = 0;
    while (pos + 16 <= data.size()) {
        if (data[pos] != 0x01 || data[pos+1] != 0x01) { ++pos; continue; }

        int  btype   = data[pos+3] & 0x0F;
        auto csize   = readU32LE(&data[pos+4]);
        auto blkId   = readU32LE(&data[pos+12]);

        if (csize < 4 || pos + 16 + csize > data.size()) { ++pos; continue; }

        const uint8_t* payload_ptr = &data[pos+16];
        std::vector<uint8_t> payload(payload_ptr, payload_ptr + csize);

        // Extract TBOS name
        std::string name;
        if (csize >= 5 && payload[0]=='T' && payload[1]=='B' && payload[2]=='O' && payload[3]=='S') {
            const uint8_t* raw = &payload[4];
            size_t maxLen = std::min((size_t)64, (size_t)(csize - 4));
            size_t len = 0;
            while (len < maxLen && raw[len] != 0) ++len;
            name = std::string(reinterpret_cast<const char*>(raw), len);
        }

        blocks.push_back({btype, blkId, name, std::move(payload)});
        pos += 16 + csize;
    }
    return blocks;
}

// ── EXS24 loader ─────────────────────────────────────────────────────────────

bool loadEXS24(const std::string& path, EXS24Instrument& out)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 64 * 1024 * 1024) { fclose(f); return false; }
    std::vector<uint8_t> data((size_t)sz);
    if (fread(data.data(), 1, (size_t)sz, f) != (size_t)sz) { fclose(f); return false; }
    fclose(f);

    auto blocks = scanBlocks(data);

    // ── Separate block types ─────────────────────────────────────────────────
    std::vector<RawBlock*> zones;
    std::vector<RawBlock*> samples;
    std::string instrName;

    for (auto& b : blocks) {
        if (b.type == 0 && instrName.empty()) instrName = b.name;
        else if (b.type == 1) zones.push_back(&b);
        else if (b.type == 3) samples.push_back(&b);
    }

    if (zones.empty() && samples.empty()) return false;

    out.name = instrName;
    out.zones.clear();

    // ── Resolve sample file paths ────────────────────────────────────────────
    // Each type-3 block has: name = filename, payload = contains absolute folder path
    struct SampleEntry {
        std::string filename;   // "Kick 1 - Kit.aif"
        std::string baseName;   // "Kick 1" (stripped suffix and extension)
        std::string filePath;   // resolved absolute path
    };
    std::vector<SampleEntry> sampleEntries;

    for (auto* sb : samples) {
        std::string filename = sb->name;
        if (filename.empty()) continue;

        // Find folder path in payload (first null-terminated string starting with '/')
        std::string folder;
        const auto& pl = sb->payload;
        for (size_t i = 0; i + 1 < pl.size(); ++i) {
            if (pl[i] == '/') {
                size_t end = i;
                while (end < pl.size() && pl[end] != 0) ++end;
                if (end > i) { folder = std::string((const char*)&pl[i], end - i); break; }
            }
        }

        // Also look for a secondary filename at a later offset in the payload
        // (sometimes the filename appears again near the end of the block)
        // Use the name field as-is.
        std::string filePath = folder.empty() ? std::string() : resolveAudioFile(folder, filename);

        // Try same directory as the .exs file
        if (filePath.empty()) {
            std::string exsDir = path.substr(0, path.rfind('/'));
            filePath = resolveAudioFile(exsDir, filename);
        }

        // Build base name: strip everything after and including the first " - " (kit suffix)
        std::string baseName = filename;
        auto dot = baseName.rfind('.');
        if (dot != std::string::npos) baseName = baseName.substr(0, dot);
        auto dash = baseName.find(" - ");
        if (dash == std::string::npos) dash = baseName.rfind(" - ");
        if (dash != std::string::npos) baseName = baseName.substr(0, dash);

        sampleEntries.push_back({filename, baseName, filePath});
    }

    // ── Parse zones ──────────────────────────────────────────────────────────
    // Zone data layout (after 68-byte TBOS+name field in payload):
    //   byte 0: flags
    //   byte 1: root note (MIDI, C-1=0)
    //   byte 2: coarse tune (int8)
    //   byte 3: fine tune (int8)
    //   byte 4-5: group/sample ref (not reliably decoded — use name match)
    //   byte 6: note_low
    //   byte 7: note_high
    //   byte 8: ?
    //   byte 9: vel_low
    //   byte 10: vel_high

    for (auto* zb : zones) {
        if (zb->payload.size() < 68 + 11) continue;
        const uint8_t* zd = &zb->payload[68];

        EXS24Zone zone;
        zone.rootNote  = zd[1];
        zone.coarseTune= (int8_t)zd[2];
        zone.fineTune  = (int8_t)zd[3];
        zone.noteLow   = zd[6];
        zone.noteHigh  = zd[7];
        zone.velLow    = zd[9];
        zone.velHigh   = zd[10];

        // Clamp to valid MIDI range
        zone.noteLow  = std::min(zone.noteLow,  (uint8_t)127);
        zone.noteHigh = std::max(zone.noteHigh, zone.noteLow);
        zone.noteHigh = std::min(zone.noteHigh, (uint8_t)127);
        if (zone.velHigh < zone.velLow) zone.velHigh = 127;

        // ── Match zone to sample by name ─────────────────────────────────────
        // Strategy: zone name is a substring of sample basename, or vice versa.
        int bestMatch = -1;
        if (!sampleEntries.empty()) {
            if (sampleEntries.size() == 1) {
                bestMatch = 0;
            } else {
                const std::string& zname = zb->name;
                // Exact base name match first
                for (int i = 0; i < (int)sampleEntries.size(); ++i) {
                    if (lowered(sampleEntries[i].baseName) == lowered(zname)) {
                        bestMatch = i; break;
                    }
                }
                // Zone name contained in sample base name
                if (bestMatch < 0 && !zname.empty()) {
                    for (int i = 0; i < (int)sampleEntries.size(); ++i) {
                        if (containsCI(sampleEntries[i].baseName, zname)) {
                            bestMatch = i; break;
                        }
                    }
                }
                // Sample base name contained in zone name
                if (bestMatch < 0) {
                    for (int i = 0; i < (int)sampleEntries.size(); ++i) {
                        if (!sampleEntries[i].baseName.empty() &&
                            containsCI(zname, sampleEntries[i].baseName)) {
                            bestMatch = i; break;
                        }
                    }
                }
                // Final fallback: sequential
                if (bestMatch < 0)
                    bestMatch = (int)(out.zones.size() % sampleEntries.size());
            }
        }

        if (bestMatch >= 0 && !sampleEntries[bestMatch].filePath.empty()) {
            zone.samplePath = sampleEntries[bestMatch].filePath;
            zone.sampleName = sampleEntries[bestMatch].baseName;
        } else if (bestMatch >= 0) {
            zone.sampleName = sampleEntries[bestMatch].baseName;
        } else {
            zone.sampleName = zb->name;
        }

        // Skip zones with no resolvable file (can't render them)
        if (zone.samplePath.empty() && !sampleEntries.empty()) {
            // use baseName for display even if file not found
        }

        out.zones.push_back(zone);
    }

    return !out.zones.empty();
}
