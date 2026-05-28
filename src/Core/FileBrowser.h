//
// Created by acroy on 2/17/2026.
//

#ifndef FILEBROWSER_H
#define FILEBROWSER_H

#include <filesystem>

struct FileBrowser {
    bool open = false;
    std::string currentPath;
    std::string selectedFile;
    std::vector<std::string> files;
    std::vector<std::string> folders;
    std::string filter; // e.g. ".obj"
    std::function<void(std::string)> onSelect;

    void openAt(const std::string& path, const std::string& ext, std::function<void(std::string)> callback) {
        currentPath = path;
        filter = ext;
        onSelect = callback;
        selectedFile = "";
        refresh();
        open = true;
    }

    void refresh() {
        files.clear();
        folders.clear();
        for (const auto& entry : std::filesystem::directory_iterator(currentPath)) {
            if (entry.is_directory()) {
                folders.push_back(entry.path().filename().string());
            } else if (filter.empty() || entry.path().extension() == filter) {
                files.push_back(entry.path().filename().string());
            }
        }
        std::sort(files.begin(), files.end());
        std::sort(folders.begin(), folders.end());
    }
};

#endif //FILEBROWSER_H
