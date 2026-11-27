/*
 * CZ Lua — project model and screen navigation (v1).
 *
 * A project is a folder containing main.lua. Bundled samples live
 * under /usr/share/cz_lua/projects (read-only, owned by root); user
 * projects live under $HOME/cz_lua/projects so the editor can write
 * them. Opening a bundled sample copies it into the user area first
 * (dev memo §3, v1 task requirement 4). The Lua sandbox root for a
 * run is the (user-side) project folder.
 */

#pragma once

#include <string>
#include <vector>

namespace screen {

struct CzProject {
    std::string name;     // folder name, shown in the file list
    std::string dir;      // folder path
    std::string mainPath; // dir + "/main.lua"
    bool bundled = false; // true: read-only sample under /usr/share
};

// Navigation between the CZ Lua screens. Implemented by the app's
// ScreenManager; calls are deferred internally (lv_async_call), so
// screens may invoke them from inside key/timer callbacks safely.
struct CzNavigator {
    virtual ~CzNavigator() = default;
    virtual void openFileList() = 0;
    virtual void openEditor(const CzProject& project) = 0;
    // Run `source` (the editor's current buffer, saved or not) with
    // the project's folder as the sandbox root. `editor_dirty` is the
    // buffer's dirty flag, kept so the edit session can be restored
    // verbatim when the runner exits back to the editor.
    virtual void openRunner(const CzProject& project, const std::string& source,
                            bool editor_dirty) = 0;
};

// $HOME/cz_lua/projects (created on demand). Falls back to /tmp when
// HOME is unset.
std::string userProjectsRoot();
void ensureUserProjectsRoot();

// Bundled samples: /usr/share/cz_lua/projects on device,
// "projects" (relative) when running from the source tree.
std::string bundledProjectsRoot();

// User projects first, then bundled samples that have no user copy
// yet (a copied sample shadows its bundled original).
std::vector<CzProject> scanProjects();

// Create an "untitled" / "untitled2" / ... project with a stub
// main.lua in the user area.
CzProject createNewProject();

// Copy a bundled sample into the user area and return the copy.
// If the copy already exists it is returned unchanged.
CzProject copyBundledToUser(const CzProject& bundled);

std::string readProjectMain(const CzProject& project);
bool writeProjectMain(const CzProject& project, const std::string& text);

} // namespace screen
