#include "ModuleController.h"
#include "SoundModule.h"
#include "OpenMPTInternal.h"

ModuleController::ModuleController(ModuleState& state) : m_state(state) {}

void ModuleController::setModule(SoundModule* mod) { m_mod = mod; }
void ModuleController::clearModule()               { m_mod = nullptr; }

// ── Transport parameters ───────────────────────────────────────────────────────

void ModuleController::setTempo(int bpm) {
    if (!m_mod) return;
    m_mod->setTempo(bpm);
}

void ModuleController::setSpeed(int speed) {
    if (!m_mod) return;
    m_mod->setSpeed(speed);
}

void ModuleController::setGlobalVolume(double factor) {
    if (!m_mod) return;
    m_mod->setGlobalVolume(factor);
}

void ModuleController::setTempoFactor(double factor) {
    if (!m_mod) return;
    m_mod->setTempoFactor(factor);
}

void ModuleController::setPitchFactor(double factor) {
    if (!m_mod) return;
    m_mod->setPitchFactor(factor);
}

// ── Render parameters ──────────────────────────────────────────────────────────

void ModuleController::setMasterGain(int gain) {
    if (!m_mod) return;
    m_mod->setRenderParam(SoundModule::RENDER_MASTERGAIN_MILLIBEL, gain);
}

void ModuleController::setStereoSeparation(int sep) {
    if (!m_mod) return;
    m_mod->setRenderParam(SoundModule::RENDER_STEREOSEPARATION_PERCENT, sep);
}

void ModuleController::setInterpolation(int mode) {
    if (!m_mod) return;
    m_mod->setRenderParam(SoundModule::RENDER_INTERPOLATIONFILTER_LENGTH, mode);
}

void ModuleController::setVolumeRamping(int mode) {
    if (!m_mod) return;
    m_mod->setRenderParam(SoundModule::RENDER_VOLUMERAMPING_STRENGTH, mode);
}

// ── Pattern editing ────────────────────────────────────────────────────────────

void ModuleController::setPatternCell(int32_t pat, int32_t row, int32_t ch, const PatternCell& cell) {
    if (!m_mod) return;
    pe_set_cell(m_mod, pat, row, ch, cell);
}

void ModuleController::clearPatternCell(int32_t pat, int32_t row, int32_t ch) {
    if (!m_mod) return;
    pe_clear_cell(m_mod, pat, row, ch);
}

void ModuleController::insertPatternRow(int32_t pat, int32_t row, int32_t nch) {
    if (!m_mod) return;
    pe_insert_row(m_mod, pat, row, nch);
}

void ModuleController::deletePatternRow(int32_t pat, int32_t row, int32_t nch) {
    if (!m_mod) return;
    pe_delete_row(m_mod, pat, row, nch);
}

int ModuleController::insertPattern(int rows) {
    if (!m_mod) return -1;
    return bb_insert_pattern(m_mod, rows);
}

// ── Note preview ───────────────────────────────────────────────────────────────

void ModuleController::playNote(int32_t channel, int note, int instrument, double volume, double panning) {
    if (!m_mod) return;
    m_mod->playNote(instrument, note, volume, panning);
    (void)channel;
}

void ModuleController::stopNote(int32_t channel) {
    if (!m_mod) return;
    m_mod->stopNote(channel);
}

void ModuleController::setNotePitch(int32_t channel, double semitones) {
    if (!m_mod) return;
    m_mod->setNotePitch(channel, semitones);
}

int32_t ModuleController::playInstrumentNote(int32_t instrument, int note, double volume, double panning) {
    if (!m_mod) return -1;
    return m_mod->playNote(instrument, note, volume, panning);
}

int32_t ModuleController::playSample(int sampleIndex, int note, double volume, double panning) {
    if (!m_mod) return -1;
    return bb_play_sample(m_mod, sampleIndex, note, volume, panning);
}

// ── File ───────────────────────────────────────────────────────────────────────

bool ModuleController::save(const std::string& path, const std::string& type) {
    if (!m_mod) return false;
    return bb_save_module(m_mod, path, type);
}
