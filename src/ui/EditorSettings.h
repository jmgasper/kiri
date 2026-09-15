#pragma once
#include <SupportDefs.h>
#include <string>
#include "ui/Theme.h"
#include <optional>

class BMessage;
namespace kiri {
struct EditorSettings {
    EditorSettings();
    std::string fontFamily;
    int32 fontSize=13;
    int32 theme=0;
    std::string themeID,themeStatus;
    std::optional<ColorTheme> customTheme;
    bool semanticHighlighting=true;
    Theme Colors() const;
    void SelectTheme(const std::string& id,const std::string& settings);
    void SetTheme(const ColorTheme& definition);
    void Normalize();
    void ReadFrom(const BMessage& message,const std::string& settings={});
    void WriteTo(BMessage& message) const;
    bool operator==(const EditorSettings& other) const;
    bool operator!=(const EditorSettings& other) const { return !(*this==other); }
};
}
