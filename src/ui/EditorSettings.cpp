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
    indentation.Normalize();std::vector<int> guides;std::string error;
    if(ParseGuideColumns(GuideColumnsText(guideColumns),guides,error)) guideColumns=std::move(guides);else guideColumns.clear();
    fontSize=std::clamp(fontSize,int32(8),int32(48));
    if(theme<0 || theme>=static_cast<int32>(Theme::Builtins().size())) theme=0;
    if(themeID.empty()) themeID=Theme::Builtins()[theme].definition.id;
    BFont font;
    if(fontFamily.empty() || fontFamily.size()>B_FONT_FAMILY_LENGTH
        || font.SetFamilyAndStyle(fontFamily.c_str(),nullptr)!=B_OK) {
        font_family family;
        be_fixed_font->GetFamilyAndStyle(&family,nullptr);
        fontFamily=family;
    }
}
Theme EditorSettings::Colors() const { return customTheme && customTheme->id==themeID?Theme::FromDefinition(*customTheme):Theme::Builtins()[std::clamp(theme,0,int(Theme::Builtins().size())-1)]; }
void EditorSettings::SetTheme(const ColorTheme& definition) {
    themeID=definition.id;themeStatus.clear();customTheme.reset();theme=0;
    if(themeID.rfind("custom.",0)==0) { customTheme=definition;theme=definition.dark?0:1; }
    else for(size_t i=0;i<Theme::Builtins().size();++i) if(Theme::Builtins()[i].definition.id==themeID) theme=i;
}
void EditorSettings::SelectTheme(const std::string& id,const std::string& settings) {
    auto resolved=ResolveColorTheme(settings,id);SetTheme(resolved.theme);themeID=id;themeStatus=resolved.warning;
}
void EditorSettings::ReadFrom(const BMessage& message,const std::string& settings) {
    const char* family;
    if(message.FindString("editor_font_family",&family)==B_OK) fontFamily=family;
    int32 value;
    if(message.FindInt32("editor_font_size",&value)==B_OK) fontSize=value;
    // Retain the original numeric setting for older installations, while new
    // settings identify themes by name so menu ordering can change safely.
    if(message.FindInt32("theme",&value)==B_OK) { theme=value;themeID.clear();customTheme.reset();themeStatus.clear(); }
    const char* name;
    if(message.FindString("theme_name",&name)==B_OK) { theme=ThemeIndex(name);themeID.clear();customTheme.reset(); }
    Normalize();
    const char* id=nullptr;
    if(message.FindString("theme_id",&id)==B_OK) {
        const char* data=nullptr;
        if(settings.empty() && message.FindString("custom_theme",&data)==B_OK) {
            auto parsed=ParseTheme(data);if(parsed.ok() && parsed.theme.id==id) SetTheme(parsed.theme);else SelectTheme(id,settings);
        }else SelectTheme(id,settings);
    }
    bool enabled=false;if(message.FindBool("semantic_highlighting",&enabled)==B_OK) semanticHighlighting=enabled;
    if(message.FindBool("indent_tabs",&enabled)==B_OK) indentation.tabs=enabled;
    if(message.FindInt32("tab_width",&value)==B_OK) indentation.tabWidth=value;
    if(message.FindInt32("indent_width",&value)==B_OK) indentation.indentWidth=value;
    if(message.FindBool("minimap",&enabled)==B_OK) minimap=enabled;
    const char* guides=nullptr;if(message.FindString("guide_columns",&guides)==B_OK) {std::string error;ParseGuideColumns(guides,guideColumns,error);}
    Normalize();
}
void EditorSettings::WriteTo(BMessage& message) const {
    message.AddString("editor_font_family",fontFamily.c_str());
    message.AddInt32("editor_font_size",fontSize);
    message.AddInt32("theme",theme);
    message.AddString("theme_name",Colors().name.c_str());
    message.AddString("theme_id",(themeID.empty()?Theme::Builtins()[theme].definition.id:themeID).c_str());
    if(customTheme) message.AddString("custom_theme",SerializeTheme(*customTheme).c_str());
    message.AddBool("semantic_highlighting",semanticHighlighting);
    message.AddBool("indent_tabs",indentation.tabs);message.AddInt32("tab_width",indentation.tabWidth);message.AddInt32("indent_width",indentation.indentWidth);
    message.AddBool("minimap",minimap);message.AddString("guide_columns",GuideColumnsText(guideColumns).c_str());
}
bool EditorSettings::operator==(const EditorSettings& other) const {
    return fontFamily==other.fontFamily && fontSize==other.fontSize && theme==other.theme && themeID==other.themeID && customTheme==other.customTheme && semanticHighlighting==other.semanticHighlighting && indentation==other.indentation && guideColumns==other.guideColumns && minimap==other.minimap;
}
void WriteDocumentOverrides(BMessage& message,const DocumentOverrides& overrides) {
    message.AddBool("document_indent_override",bool(overrides.indentation));message.AddBool("document_guides_override",bool(overrides.guides));
    if(overrides.indentation) {message.AddBool("document_tabs",overrides.indentation->tabs);message.AddInt32("document_tab_width",overrides.indentation->tabWidth);message.AddInt32("document_indent_width",overrides.indentation->indentWidth);}
    if(overrides.guides) message.AddString("document_guides",GuideColumnsText(*overrides.guides).c_str());
}
DocumentOverrides ReadDocumentOverrides(const BMessage& message) {
    DocumentOverrides overrides;bool enabled=false;
    if(message.FindBool("document_indent_override",&enabled)==B_OK && enabled) {Indentation indentation;int32 width=0;message.FindBool("document_tabs",&indentation.tabs);if(message.FindInt32("document_tab_width",&width)==B_OK) indentation.tabWidth=width;if(message.FindInt32("document_indent_width",&width)==B_OK) indentation.indentWidth=width;indentation.Normalize();overrides.indentation=indentation;}
    if(message.FindBool("document_guides_override",&enabled)==B_OK && enabled) {const char* text=nullptr;std::vector<int> guides;std::string error;if(message.FindString("document_guides",&text)==B_OK && ParseGuideColumns(text,guides,error)) overrides.guides=std::move(guides);}
    return overrides;
}
}
