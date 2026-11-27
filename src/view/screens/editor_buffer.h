/*
 * CZ Lua — editor text buffer (LVGL-free, host-testable).
 *
 * Holds the edited text as a list of lines plus a cursor, and
 * implements the vi-style operations the editor screen needs:
 * insert / newline / backspace / delete, line delete (dd), and a
 * simple snapshot undo (u). All positions are byte offsets; the
 * editor targets ASCII Lua source, so bytes == characters.
 */

#pragma once

#include <string>
#include <vector>

namespace screen {

class EditorBuffer {
public:
    EditorBuffer() = default;

    // Replace the whole content; resets cursor, undo history and the
    // dirty flag (loading a file is not an edit).
    void setText(const std::string& text);
    // Serialise back: lines joined with '\n', with a trailing newline
    // when there is any content (POSIX-style text file).
    std::string text() const;

    const std::vector<std::string>& lines() const { return lines_; }
    int lineCount() const { return static_cast<int>(lines_.size()); }
    int cursorLine() const { return line_; }
    int cursorCol() const { return col_; }
    bool dirty() const { return dirty_; }
    void clearDirty() { dirty_ = false; }
    // Used when restoring an edit session whose content already
    // differs from what is on disk.
    void markDirty() { dirty_ = true; }

    void setCursor(int line, int col);
    void moveLeft();
    void moveRight();
    void moveUp();
    void moveDown();
    void moveLineStart();
    void moveLineEnd();

    // Mutations (each records an undo snapshot, coalescing runs of
    // single-character inserts/deletes into one undo step).
    void insertChar(char c);
    void insertNewline();
    void backspace();
    void deleteForward();
    void deleteCharUnder(); // vi 'x'
    void deleteLine();      // vi 'dd'
    bool undo();

private:
    enum class EditKind { None, InsertRun, BackspaceRun, Other };
    void pushUndo(EditKind kind);
    void clampCursor();

    std::vector<std::string> lines_{std::string()};
    int line_ = 0;
    int col_ = 0;
    int goal_col_ = 0; // remembered column for vertical movement
    bool dirty_ = false;

    struct Snapshot {
        std::vector<std::string> lines;
        int line;
        int col;
    };
    std::vector<Snapshot> undo_;
    EditKind last_edit_ = EditKind::None;
    static constexpr size_t kMaxUndo = 100;
};

} // namespace screen
