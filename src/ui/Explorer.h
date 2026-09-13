#pragma once
#include <OutlineListView.h>
#include "core/Project.h"
#include "ui/Theme.h"
#include <functional>
#include <set>
namespace kiri {
class FileItem:public BListItem {
public:
    FileItem(DirectoryEntry value,uint32 level=0,bool placeholder=false);
    void DrawItem(BView* owner,BRect rect,bool complete=false) override;
    void Update(BView* owner,const BFont* font) override;
    DirectoryEntry entry;
    bool loaded=false,placeholder=false;
};
class Explorer:public BOutlineListView {
public:
    Explorer();
    void MouseDown(BPoint where) override;
    void KeyDown(const char* bytes,int32 count) override;
    void MessageReceived(BMessage* message) override;
    void ApplyTheme(const Theme& theme) { fTheme=theme;SetViewColor(theme.panel);SetLowColor(theme.panel);Invalidate(); }
    const Theme& Colors() const { return fTheme; }
    void Clear();
    void SetRoot(const std::string& path,const DirectoryResult& listing);
    void AddListing(const std::string& parent,const DirectoryResult& listing);
    void CheckExpanded();
    std::function<void(std::string)> requestDirectory;
private:
    void OpenSelected();
    void AddEntries(const DirectoryResult& listing,FileItem* parent);
    Theme fTheme=Theme::Builtins()[0];
};
}
