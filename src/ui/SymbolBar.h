#pragma once
#include "core/LanguageProtocol.h"
#include "ui/Theme.h"
#include <View.h>

namespace kiri {
class SymbolBar:public BView {
public:
    SymbolBar();
    void ApplyTheme(const Theme& theme);
    void SetSymbols(std::vector<DocumentSymbol> symbols,int64 document,int64 version);
    void SetStatus(const std::string& status,const std::string& detail={});
    void SetPosition(size_t offset);
    void Draw(BRect update) override;
    void MouseDown(BPoint point) override;
    void KeyDown(const char* bytes,int32 count) override;
    void Browse();
private:
    Theme fTheme=Theme::Builtins()[0];
    std::vector<DocumentSymbol> fSymbols;
    std::string fStatus="Open a source file",fDetail;
    int64 fDocument=0,fVersion=0;
    int fCurrent=-1;
};
}
