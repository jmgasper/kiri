#pragma once
#include "core/LanguageProtocol.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace kiri {
struct ColorTheme {
    std::string id,name,base;
    bool dark=true;
    std::map<std::string,uint32_t> colors;
    bool operator==(const ColorTheme& other) const;
};
struct ThemeResult {
    ColorTheme theme;
    std::string error,warning;
    bool ok() const { return error.empty(); }
};
const std::vector<ColorTheme>& BuiltinColorThemes();
const std::vector<std::string>& ColorRoles();
std::string NewThemeID();
std::string HexColor(uint32_t color);
bool ParseColor(const std::string& text,uint32_t& color);
ThemeResult ParseTheme(const std::string& bytes);
std::string SerializeTheme(const ColorTheme& theme);
ThemeResult ReadThemeFile(const std::string& path);
std::vector<ColorTheme> LoadColorThemes(const std::string& settings,std::string* warning=nullptr);
std::string SaveColorTheme(const std::string& settings,const ColorTheme& theme);
ThemeResult ResolveColorTheme(const std::string& settings,const std::string& id);
std::string ThemeContrastWarning(const ColorTheme& theme);
}
