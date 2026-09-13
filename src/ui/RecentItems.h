#pragma once
#include <string>
#include <vector>
#include <utility>

namespace kiri {
struct RecentItem { std::string path;bool folder=false; };
std::string SettingsDirectory();
class RecentItems {
public:
    static constexpr size_t Limit=24;
    explicit RecentItems(std::string directory):fDirectory(std::move(directory)) {}
    std::vector<RecentItem> Load() const;
    bool Remember(const std::string& path,bool folder) const;
    bool Remove(const std::string& path) const;
private:
    std::vector<RecentItem> Read() const;
    bool Write(const std::vector<RecentItem>& items) const;
    std::string fDirectory;
};
}
