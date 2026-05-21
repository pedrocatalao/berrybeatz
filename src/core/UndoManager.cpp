#include "UndoManager.h"
#include "PatternEditor.h"
#include <libopenmpt/libopenmpt_ext.hpp>

void UndoManager::push(UndoEntry e) {
    m_redoStack.clear();
    m_undoStack.push_back(std::move(e));
    if ((int)m_undoStack.size() > kMaxUndo)
        m_undoStack.pop_front();
}

void UndoManager::undo(openmpt::module_ext* mod) {
    if (m_undoStack.empty() || !mod) return;
    UndoEntry e = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    for (auto& c : e)
        pe_set_cell(mod, c.pat, c.row, c.ch, c.before);
    m_redoStack.push_back(std::move(e));
}

void UndoManager::redo(openmpt::module_ext* mod) {
    if (m_redoStack.empty() || !mod) return;
    UndoEntry e = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    for (auto& c : e)
        pe_set_cell(mod, c.pat, c.row, c.ch, c.after);
    m_undoStack.push_back(std::move(e));
}

void UndoManager::clear() {
    m_undoStack.clear();
    m_redoStack.clear();
}
