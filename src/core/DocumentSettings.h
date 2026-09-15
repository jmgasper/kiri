#pragma once
#include <atomic>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "core/FileIO.h"

namespace kiri {
struct Indentation {
    bool tabs=false;
    int tabWidth=4,indentWidth=4;
    void Normalize();
    bool operator==(const Indentation& other) const;
};
struct DocumentOverrides {
    std::optional<Indentation> indentation;
    std::optional<std::vector<int>> guides;
};
struct ConfigValue { std::string value,file;int line=0; };
struct DocumentConfig {
    std::map<std::string,ConfigValue> properties;
    std::vector<std::string> files,warnings;
    std::map<std::string,FileStamp> stamps;
};
struct DocumentStyle {
    Indentation indentation;
    std::vector<int> guides;
    std::string origin,warning,notes;
};
// Filesystem I/O belongs on a worker. The resolver does not alter source files.
DocumentConfig ReadDocumentConfig(const std::string& path,const std::atomic<bool>* cancel=nullptr);
void ReadConfigText(DocumentConfig& result,const std::string& text,const std::string& file,
    const std::string& relativePath,bool& root);
bool MatchConfigGlob(const std::string& pattern,const std::string& relativePath,std::string& error);
DocumentStyle ResolveDocumentStyle(const Indentation& defaults,const std::vector<int>& guides,
    const DocumentConfig& config,const DocumentOverrides& overrides={});
// Empty/"off" means no guides. Columns count from 1; at most eight, up to 1000.
bool ParseGuideColumns(const std::string& text,std::vector<int>& columns,std::string& error);
std::string GuideColumnsText(const std::vector<int>& columns);
}
