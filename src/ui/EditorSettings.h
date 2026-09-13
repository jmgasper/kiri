#pragma once
#include <SupportDefs.h>
#include <string>

class BMessage;
namespace kiri {
struct EditorSettings {
    EditorSettings();
    std::string fontFamily;
    int32 fontSize=13;
    int32 theme=0;
    void Normalize();
    void ReadFrom(const BMessage& message);
    void WriteTo(BMessage& message) const;
    bool operator==(const EditorSettings& other) const;
    bool operator!=(const EditorSettings& other) const { return !(*this==other); }
};
}
