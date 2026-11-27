/*
 * CZ Lua — editor text buffer (implementation). See editor_buffer.h.
 */

#include "editor_buffer.h"

#include <algorithm>

namespace screen {

void EditorBuffer::setText(const std::string& text) {
    lines_.clear();
    std::string current;
    for (char c : text) {
        if (c == '\n') {
            lines_.push_back(current);
            current.clear();
        } else if (c != '\r') {
            current += c;
        }
    }
    // A trailing newline terminates the last line rather than adding
    // an extra empty one; keep exactly one empty line for empty files.
    if (!current.empty() || lines_.empty()) {
        lines_.push_back(current);
    }
    line_ = 0;
    col_ = 0;
    goal_col_ = 0;
    dirty_ = false;
    undo_.clear();
    last_edit_ = EditKind::None;
}

std::string EditorBuffer::text() const {
    std::string out;
    for (size_t i = 0; i < lines_.size(); ++i) {
        out += lines_[i];
        out += '\n';
    }
    return out;
}

void EditorBuffer::clampCursor() {
    line_ = std::max(0, std::min(line_, static_cast<int>(lines_.size()) - 1));
    col_ = std::max(0, std::min(col_, static_cast<int>(lines_[line_].size())));
}

void EditorBuffer::setCursor(int line, int col) {
    line_ = line;
    col_ = col;
    clampCursor();
    goal_col_ = col_;
    last_edit_ = EditKind::None;
}

void EditorBuffer::moveLeft() {
    if (col_ > 0) {
        --col_;
    } else if (line_ > 0) {
        --line_;
        col_ = static_cast<int>(lines_[line_].size());
    }
    goal_col_ = col_;
    last_edit_ = EditKind::None;
}

void EditorBuffer::moveRight() {
    if (col_ < static_cast<int>(lines_[line_].size())) {
        ++col_;
    } else if (line_ + 1 < static_cast<int>(lines_.size())) {
        ++line_;
        col_ = 0;
    }
    goal_col_ = col_;
    last_edit_ = EditKind::None;
}

void EditorBuffer::moveUp() {
    if (line_ > 0) {
        --line_;
        col_ = std::min(goal_col_, static_cast<int>(lines_[line_].size()));
    }
    last_edit_ = EditKind::None;
}

void EditorBuffer::moveDown() {
    if (line_ + 1 < static_cast<int>(lines_.size())) {
        ++line_;
        col_ = std::min(goal_col_, static_cast<int>(lines_[line_].size()));
    }
    last_edit_ = EditKind::None;
}

void EditorBuffer::moveLineStart() {
    col_ = 0;
    goal_col_ = 0;
    last_edit_ = EditKind::None;
}

void EditorBuffer::moveLineEnd() {
    col_ = static_cast<int>(lines_[line_].size());
    goal_col_ = col_;
    last_edit_ = EditKind::None;
}

void EditorBuffer::pushUndo(EditKind kind) {
    const bool coalesce = (kind == EditKind::InsertRun && last_edit_ == EditKind::InsertRun) ||
                          (kind == EditKind::BackspaceRun && last_edit_ == EditKind::BackspaceRun);
    if (!coalesce) {
        undo_.push_back(Snapshot{lines_, line_, col_});
        if (undo_.size() > kMaxUndo) {
            undo_.erase(undo_.begin());
        }
    }
    last_edit_ = kind;
    dirty_ = true;
}

void EditorBuffer::insertChar(char c) {
    pushUndo(EditKind::InsertRun);
    lines_[line_].insert(lines_[line_].begin() + col_, c);
    ++col_;
    goal_col_ = col_;
}

void EditorBuffer::insertNewline() {
    pushUndo(EditKind::Other);
    std::string tail = lines_[line_].substr(static_cast<size_t>(col_));
    lines_[line_].erase(static_cast<size_t>(col_));
    lines_.insert(lines_.begin() + line_ + 1, tail);
    ++line_;
    col_ = 0;
    goal_col_ = 0;
}

void EditorBuffer::backspace() {
    if (col_ == 0 && line_ == 0) return;
    pushUndo(EditKind::BackspaceRun);
    if (col_ > 0) {
        lines_[line_].erase(static_cast<size_t>(col_ - 1), 1);
        --col_;
    } else {
        col_ = static_cast<int>(lines_[line_ - 1].size());
        lines_[line_ - 1] += lines_[line_];
        lines_.erase(lines_.begin() + line_);
        --line_;
    }
    goal_col_ = col_;
}

void EditorBuffer::deleteForward() {
    if (col_ >= static_cast<int>(lines_[line_].size())) {
        if (line_ + 1 >= static_cast<int>(lines_.size())) return;
        pushUndo(EditKind::Other);
        lines_[line_] += lines_[line_ + 1];
        lines_.erase(lines_.begin() + line_ + 1);
        return;
    }
    pushUndo(EditKind::Other);
    lines_[line_].erase(static_cast<size_t>(col_), 1);
}

void EditorBuffer::deleteCharUnder() {
    if (col_ >= static_cast<int>(lines_[line_].size())) return;
    pushUndo(EditKind::Other);
    lines_[line_].erase(static_cast<size_t>(col_), 1);
    clampCursor();
}

void EditorBuffer::deleteLine() {
    pushUndo(EditKind::Other);
    lines_.erase(lines_.begin() + line_);
    if (lines_.empty()) {
        lines_.push_back(std::string());
    }
    line_ = std::min(line_, static_cast<int>(lines_.size()) - 1);
    col_ = 0;
    goal_col_ = 0;
}

bool EditorBuffer::undo() {
    if (undo_.empty()) return false;
    Snapshot snap = undo_.back();
    undo_.pop_back();
    lines_ = snap.lines;
    line_ = snap.line;
    col_ = snap.col;
    goal_col_ = col_;
    dirty_ = true; // content may again differ from disk either way
    last_edit_ = EditKind::None;
    clampCursor();
    return true;
}

} // namespace screen
