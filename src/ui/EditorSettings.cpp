#include "ui/EditorSettings.h"
#include "ui/Theme.h"
#include <Font.h>
#include <Message.h>
#include <algorithm>

namespace kiri {
EditorSettings::EditorSettings() {
    font_family family;
    be_fixed_font->GetFamilyAndStyle(&family,nullptr);
    fontFamily=family;
}
void EditorSettings::Normalize() {
    fontSize=std::clamp(fontSize,int32(8),int32(48));
    if(theme<0 || theme>=static_cast<int32>(Theme::Builtins().size())) theme=0;
    BFont font;
    if(fontFamily.empty() || fontFamily.size()>B_FONT_FAMILY_LENGTH
        || font.SetFamilyAndStyle(fontFamily.c_str(),nullptr)!=B_OK) {
        font_family family;
        be_fixed_font->GetFamilyAndStyle(&family,nullptr);
        fontFamily=family;
    }
}
void EditorSettings::ReadFrom(const BMessage& message) {
    const char* family;
    if(message.FindString("editor_font_family",&family)==B_OK) fontFamily=family;
    int32 value;
    if(message.FindInt32("editor_font_size",&value)==B_OK) fontSize=value;
    // Retain the original numeric setting for older installations, while new
    // settings identify themes by name so menu ordering can change safely.
    if(message.FindInt32("theme",&value)==B_OK) theme=value;
    const char* name;
    if(message.FindString("theme_name",&name)==B_OK) theme=ThemeIndex(name);
    Normalize();
}
void EditorSettings::WriteTo(BMessage& message) const {
    message.AddString("editor_font_family",fontFamily.c_str());
    message.AddInt32("editor_font_size",fontSize);
    message.AddInt32("theme",theme);
    message.AddString("theme_name",Theme::Builtins()[theme].name.c_str());
}
bool EditorSettings::operator==(const EditorSettings& other) const {
    return fontFamily==other.fontFamily && fontSize==other.fontSize && theme==other.theme;
}
}
