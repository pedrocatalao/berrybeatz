#include "UndoManager.h"
#include "ModuleController.h"

void UndoManager::push(UndoEntry e) {
    m_redoStack.clear();
    m_undoStack.push_back(std::move(e));
    if ((int)m_undoStack.size() > kMaxUndo)
        m_undoStack.pop_front();
}

void UndoManager::undo(ModuleController& ctrl) {
    if (m_undoStack.empty()) return;
    UndoEntry e = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    for (auto& c : e)
        ctrl.setPatternCell(c.pat, c.row, c.ch, c.before);
    m_redoStack.push_back(std::move(e));
}

void UndoManager::redo(ModuleController& ctrl) {
    if (m_redoStack.empty()) return;
    UndoEntry e = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    for (auto& c : e)
        ctrl.setPatternCell(c.pat, c.row, c.ch, c.after);
    m_undoStack.push_back(std::move(e));
}

void UndoManager::clear() {
    m_undoStack.clear();
    m_redoStack.clear();
}
