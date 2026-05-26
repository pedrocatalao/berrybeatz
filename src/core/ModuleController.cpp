#include "ModuleController.h"
#include "OpenMPTInternal.h"
#include <libopenmpt/libopenmpt_ext.hpp>

ModuleController::ModuleController(ModuleState& state) : m_state(state) {}

void ModuleController::setModule(openmpt::module_ext* mod) { m_mod = mod; }
void ModuleController::clearModule()                        { m_mod = nullptr; }

// ── Transport parameters ───────────────────────────────────────────────────────

void ModuleController::setTempo(int bpm) {
    if (!m_mod) return;
    auto* ia3 = static_cast<openmpt::ext::interactive3*>(
        m_mod->get_interface(openmpt::ext::interactive3_id));
    if (ia3) ia3->set_current_tempo2(bpm);
}

void ModuleController::setSpeed(int speed) {
    if (!m_mod) return;
    auto* ia = static_cast<openmpt::ext::interactive*>(
        m_mod->get_interface(openmpt::ext::interactive_id));
    if (ia) ia->set_current_speed(speed);
}

void ModuleController::setGlobalVolume(double factor) {
    if (!m_mod) return;
    auto* ia = static_cast<openmpt::ext::interactive*>(
        m_mod->get_interface(openmpt::ext::interactive_id));
    if (ia) ia->set_global_volume(factor);
}

void ModuleController::setTempoFactor(double factor) {
    if (!m_mod) return;
    auto* ia = static_cast<openmpt::ext::interactive*>(
        m_mod->get_interface(openmpt::ext::interactive_id));
    if (ia) ia->set_tempo_factor(factor);
}

void ModuleController::setPitchFactor(double factor) {
    if (!m_mod) return;
    auto* ia = static_cast<openmpt::ext::interactive*>(
        m_mod->get_interface(openmpt::ext::interactive_id));
    if (ia) ia->set_pitch_factor(factor);
}

// ── Render parameters ──────────────────────────────────────────────────────────

void ModuleController::setMasterGain(int gain) {
    if (!m_mod) return;
    m_mod->set_render_param(openmpt::module::RENDER_MASTERGAIN_MILLIBEL, gain);
}

void ModuleController::setStereoSeparation(int sep) {
    if (!m_mod) return;
    m_mod->set_render_param(openmpt::module::RENDER_STEREOSEPARATION_PERCENT, sep);
}

void ModuleController::setInterpolation(int mode) {
    if (!m_mod) return;
    m_mod->set_render_param(openmpt::module::RENDER_INTERPOLATIONFILTER_LENGTH, mode);
}

void ModuleController::setVolumeRamping(int mode) {
    if (!m_mod) return;
    m_mod->set_render_param(openmpt::module::RENDER_VOLUMERAMPING_STRENGTH, mode);
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
    auto* ia = static_cast<openmpt::ext::interactive*>(
        m_mod->get_interface(openmpt::ext::interactive_id));
    if (ia) ia->play_note(channel, note, volume, panning);
    (void)instrument;
}

void ModuleController::stopNote(int32_t channel) {
    if (!m_mod) return;
    auto* ia = static_cast<openmpt::ext::interactive*>(
        m_mod->get_interface(openmpt::ext::interactive_id));
    if (ia) ia->stop_note(channel);
}

void ModuleController::setNotePitch(int32_t channel, double semitones) {
    if (!m_mod) return;
    auto* ia2 = static_cast<openmpt::ext::interactive2*>(
        m_mod->get_interface(openmpt::ext::interactive2_id));
    if (ia2) ia2->set_note_finetune(channel, semitones);
}

int32_t ModuleController::playInstrumentNote(int32_t instrument, int note, double volume, double panning) {
    if (!m_mod) return -1;
    auto* ia = static_cast<openmpt::ext::interactive*>(
        m_mod->get_interface(openmpt::ext::interactive_id));
    if (!ia) return -1;
    return ia->play_note(instrument, note, volume, panning);
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
