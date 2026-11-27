/*
 * CZ Lua — project model (implementation). See cz_project.h.
 */

#include "cz_project.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace screen {
namespace fs = std::filesystem;
namespace {

constexpr const char* kStubMain = "-- CZ Lua project\n"
                                  "function load()\n"
                                  "end\n"
                                  "\n"
                                  "function update(dt)\n"
                                  "end\n"
                                  "\n"
                                  "function draw()\n"
                                  "  czlua.graphics.clear(0.02, 0.02, 0.05)\n"
                                  "  czlua.graphics.setColor(1, 1, 1)\n"
                                  "  czlua.graphics.print(\"HELLO\", 8, 8)\n"
                                  "end\n";

std::vector<CzProject> scanRoot(const std::string& root, bool bundled) {
    std::vector<CzProject> out;
    std::error_code ec;
    if (!fs::is_directory(root, ec)) return out;
    for (const auto& entry : fs::directory_iterator(root, ec)) {
        if (!entry.is_directory()) continue;
        fs::path main = entry.path() / "main.lua";
        if (!fs::is_regular_file(main)) continue;
        CzProject p;
        p.name = entry.path().filename().string();
        p.dir = entry.path().string();
        p.mainPath = main.string();
        p.bundled = bundled;
        out.push_back(p);
    }
    std::sort(out.begin(), out.end(),
              [](const CzProject& a, const CzProject& b) { return a.name < b.name; });
    return out;
}

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return "";
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool writeFile(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << text;
    return static_cast<bool>(out);
}

} // namespace

std::string userProjectsRoot() {
    const char* home = std::getenv("HOME");
    fs::path base = (home && home[0]) ? fs::path(home) : fs::path("/tmp");
    return (base / "cz_lua" / "projects").string();
}

void ensureUserProjectsRoot() {
    std::error_code ec;
    fs::create_directories(userProjectsRoot(), ec);
}

std::string bundledProjectsRoot() {
    std::error_code ec;
    if (fs::is_directory("/usr/share/cz_lua/projects", ec)) {
        return "/usr/share/cz_lua/projects";
    }
    return "projects";
}

std::vector<CzProject> scanProjects() {
    ensureUserProjectsRoot();
    std::vector<CzProject> out = scanRoot(userProjectsRoot(), false);
    for (const auto& sample : scanRoot(bundledProjectsRoot(), true)) {
        const bool shadowed = std::any_of(out.begin(), out.end(), [&](const CzProject& p) {
            return p.name == sample.name;
        });
        if (!shadowed) out.push_back(sample);
    }
    return out;
}

CzProject createNewProject() {
    ensureUserProjectsRoot();
    const fs::path root = userProjectsRoot();
    for (int i = 1;; ++i) {
        const std::string name = (i == 1) ? "untitled" : "untitled" + std::to_string(i);
        const fs::path dir = root / name;
        std::error_code ec;
        if (!fs::exists(dir, ec)) {
            fs::create_directories(dir, ec);
            writeFile((dir / "main.lua").string(), kStubMain);
            CzProject p;
            p.name = name;
            p.dir = dir.string();
            p.mainPath = (dir / "main.lua").string();
            p.bundled = false;
            return p;
        }
    }
}

CzProject copyBundledToUser(const CzProject& bundled) {
    ensureUserProjectsRoot();
    const fs::path dir = fs::path(userProjectsRoot()) / bundled.name;
    CzProject copy;
    copy.name = bundled.name;
    copy.dir = dir.string();
    copy.mainPath = (dir / "main.lua").string();
    copy.bundled = false;
    std::error_code ec;
    if (!fs::is_regular_file(copy.mainPath, ec)) {
        fs::create_directories(dir, ec);
        writeFile(copy.mainPath, readFile(bundled.mainPath));
    }
    return copy;
}

std::string readProjectMain(const CzProject& project) {
    return readFile(project.mainPath);
}

bool writeProjectMain(const CzProject& project, const std::string& text) {
    return writeFile(project.mainPath, text);
}

} // namespace screen
