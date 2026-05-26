#pragma once
#include "PatternEditor.h"
#include <cstdint>
#include <deque>
#include <vector>

class ModuleController;

struct CellEdit {
    int32_t     pat, row, ch;
    PatternCell before, after;
};

using UndoEntry = std::vector<CellEdit>;

class UndoManager {
public:
    void push(UndoEntry e);
    void undo(ModuleController& ctrl);
    void redo(ModuleController& ctrl);
    void clear();
    bool canUndo() const { return !m_undoStack.empty(); }
    bool canRedo() const { return !m_redoStack.empty(); }

private:
    std::deque<UndoEntry> m_undoStack;
    std::deque<UndoEntry> m_redoStack;
    static constexpr int  kMaxUndo = 64;
};
