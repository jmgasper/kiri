#include "core/Git.h"
#include "core/FileIO.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>
#include <fstream>

namespace kiri {
namespace {
std::string Trim(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    return s;
}
std::string_view Field(std::string_view data, size_t& offset, char separator = '\0') {
    if (offset >= data.size()) return {};
    auto end = data.find(separator, offset);
    if (end == std::string_view::npos) end = data.size();
    auto result = data.substr(offset, end - offset);
    offset = end + 1;
    return result;
}
bool ValidHash(std::string_view hash) {
    return (hash.size() == 40 || hash.size() == 64)
        && std::all_of(hash.begin(), hash.end(), [](unsigned char c) { return std::isxdigit(c); });
}
ProcessResult Failure(std::string error) { ProcessResult r; r.error = std::move(error); return r; }
}
std::vector<GitFile> ParseGitStatus(std::string_view data) {
    std::vector<GitFile> files;
    size_t offset = 0;
    while (offset < data.size()) {
        auto field = Field(data, offset);
        if (field.size() < 4 || field[2] != ' ') continue;
        GitFile f{field[0], field[1], std::string(field.substr(3)), {}};
        if (f.index == 'R' || f.index == 'C' || f.worktree == 'R' || f.worktree == 'C')
            f.originalPath = Field(data, offset);
        files.push_back(std::move(f));
    }
    return files;
}
std::vector<Commit> ParseGitLog(std::string_view data) {
    std::vector<Commit> commits;
    size_t offset = 0;
    while (offset < data.size()) {
        Commit c;
        c.hash = Field(data, offset);
        if (!ValidHash(c.hash)) break;
        auto parents = Field(data, offset);
        size_t p = 0;
        while (p < parents.size()) c.parents.emplace_back(Field(parents, p, ' '));
        c.author = Field(data, offset);
        c.date = Field(data, offset);
        c.subject = Field(data, offset);
        c.refs = Field(data, offset);
        commits.push_back(std::move(c));
    }
    return commits;
}
void CommitGraph::Clear() { fLanes.clear(); fNextColor = 0; }
GraphRow CommitGraph::Append(const Commit& c) {
    auto found = std::find_if(fLanes.begin(), fLanes.end(), [&](const auto& l) { return l.hash == c.hash; });
    if (found == fLanes.end()) {
        fLanes.push_back({c.hash, fNextColor++});
        found = fLanes.end() - 1;
    }
    GraphRow row;
    row.lane = static_cast<int>(found - fLanes.begin());
    row.color = found->color;
    auto before = fLanes;
    fLanes.erase(fLanes.begin() + row.lane);
    int insert = row.lane;
    for (size_t i = 0; i < c.parents.size(); ++i) {
        const auto& parent = c.parents[i];
        auto existing = std::find_if(fLanes.begin(), fLanes.end(), [&](const auto& l) { return l.hash == parent; });
        if (existing == fLanes.end()) {
            fLanes.insert(fLanes.begin() + std::min(insert, static_cast<int>(fLanes.size())),
                {parent, i == 0 ? row.color : fNextColor++});
            ++insert;
        }
    }
    for (size_t i = 0; i < before.size(); ++i) {
        if (static_cast<int>(i) == row.lane) continue;
        auto target = std::find_if(fLanes.begin(), fLanes.end(), [&](const auto& l) { return l.hash == before[i].hash; });
        if (target != fLanes.end()) row.edges.push_back({static_cast<int>(i), static_cast<int>(target - fLanes.begin()), before[i].color});
    }
    for (const auto& parent : c.parents) {
        auto target = std::find_if(fLanes.begin(), fLanes.end(), [&](const auto& l) { return l.hash == parent; });
        if (target != fLanes.end()) row.edges.push_back({row.lane, static_cast<int>(target - fLanes.begin()), target->color});
    }
    row.width = static_cast<int>(std::max(before.size(), fLanes.size()));
    return row;
}
std::optional<std::string> GitHubRepository(std::string remote) {
    remote = Trim(remote);
    std::string path;
    if (remote.rfind("git@github.com:", 0) == 0) path = remote.substr(15);
    else if (remote.rfind("https://github.com/", 0) == 0) path = remote.substr(19);
    else if (remote.rfind("http://github.com/", 0) == 0) path = remote.substr(18);
    else if (remote.rfind("ssh://git@github.com/", 0) == 0) path = remote.substr(21);
    else if (remote.rfind("ssh://git@github.com:22/", 0) == 0) path = remote.substr(24);
    else return {};
    while (!path.empty() && path.back() == '/') path.pop_back();
    if (path.size() > 4 && path.substr(path.size()-4) == ".git") path.resize(path.size()-4);
    auto slash = path.find('/');
    if (slash == 0 || slash == std::string::npos || slash == path.size()-1 || path.find('/',slash+1) != std::string::npos)
        return {};
    for (unsigned char c : path) if (!std::isalnum(c) && c != '/' && c != '.' && c != '-' && c != '_') return {};
    if (path.substr(0,slash) == "." || path.substr(0,slash) == ".." || path.substr(slash+1) == "." || path.substr(slash+1) == "..") return {};
    return "https://github.com/" + path;
}
std::string EncodeURLPath(std::string_view path) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string result;
    for (unsigned char c : path) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
            || c == '-' || c == '_' || c == '.' || c == '~' || c == '/') result += static_cast<char>(c);
        else { result += '%'; result += hex[c >> 4]; result += hex[c & 15]; }
    }
    return result;
}
std::optional<size_t> MapLineToBase(std::string_view diff, size_t currentLine) {
    if (currentLine == 0) return {};
    long long delta = 0;
    size_t at = 0;
    while (at < diff.size()) {
        auto line = Field(diff, at, '\n');
        if (line.rfind("@@ ", 0) != 0) continue;
        long long oldStart=0, oldCount=1, newStart=0, newCount=1;
        std::string hunk(line);
        const char* p = hunk.c_str()+3;
        if (*p++ != '-') continue;
        char* end = nullptr;
        oldStart = strtoll(p, &end, 10); p=end;
        if (*p == ',') { oldCount = strtoll(p+1,&end,10); p=end; }
        if (*p++ != ' ' || *p++ != '+') continue;
        newStart = strtoll(p,&end,10); p=end;
        if (*p == ',') newCount = strtoll(p+1,&end,10);
        (void)oldStart;
        auto n = static_cast<long long>(currentLine);
        if (newCount > 0 && n >= newStart && n < newStart+newCount) return {};
        if (n < newStart || (newCount == 0 && n <= newStart)) break;
        delta += oldCount-newCount;
    }
    auto base = static_cast<long long>(currentLine)+delta;
    return base > 0 ? std::optional<size_t>(base) : std::nullopt;
}
ProcessResult GitRepository::Run(std::vector<std::string> arguments, const std::atomic<bool>* cancel,
    std::string input, size_t limit) const {
    std::vector<std::string> args{"git", "--no-pager", "-c", "color.ui=false", "-c", "core.quotepath=false"};
    args.insert(args.end(), arguments.begin(), arguments.end());
    ProcessOptions options;
    options.directory=fDirectory; options.cancel=cancel; options.input=std::move(input); options.outputLimit=limit;
    options.environment={"GIT_TERMINAL_PROMPT=0", "GIT_LITERAL_PATHSPECS=1", "LC_ALL=C", "GIT_OPTIONAL_LOCKS=0"};
    return RunProcess(args,options);
}
std::string GitRepository::Root() const {
    auto result = Run({"rev-parse", "--show-toplevel"});
    return result.ok() ? Trim(result.output) : "";
}
ProcessResult GitRepository::Status(const std::atomic<bool>* cancel) const {
    return Run({"status", "--porcelain=v1", "-z", "--untracked-files=all"},cancel);
}
ProcessResult GitRepository::History(size_t skip, size_t count, const std::string& path,
    const std::atomic<bool>* cancel) const {
    std::vector<std::string> args{"log", "-z", "--topo-order", "--all", "--date=iso-strict",
        "--format=%H%x00%P%x00%an%x00%aI%x00%s%x00%D", "--skip="+std::to_string(skip), "-n", std::to_string(count)};
    if (!path.empty()) { args.push_back("--follow"); args.push_back("--"); args.push_back(path); }
    return Run(args,cancel);
}
ProcessResult GitRepository::Diff(const std::string& path, bool staged, const std::atomic<bool>* cancel) const {
    std::vector<std::string> args{"diff", "--no-ext-diff", "--no-textconv", "--find-renames", "--unified=4"};
    if (staged) args.push_back("--cached");
    args.push_back("--"); if (!path.empty()) args.push_back(path);
    return Run(args,cancel);
}
ProcessResult GitRepository::CommitDiff(const std::string& hash, const std::atomic<bool>* cancel) const {
    if (!ValidHash(hash)) return Failure("Invalid commit identifier.");
    return Run({"show", "--format=fuller", "--stat", "--patch", "--root", "--diff-merges=first-parent",
        "--no-ext-diff", "--no-textconv", "--find-renames", hash, "--"},cancel);
}
ProcessResult GitRepository::Stage(const std::vector<std::string>& paths) const {
    if (paths.empty()) return Failure("Select a file to stage.");
    std::vector<std::string> args{"add", "--"}; args.insert(args.end(),paths.begin(),paths.end()); return Run(args);
}
ProcessResult GitRepository::Unstage(const std::vector<std::string>& paths) const {
    if (paths.empty()) return Failure("Select a file to unstage.");
    const bool hasHead = Run({"rev-parse", "--verify", "HEAD"}).ok();
    std::vector<std::string> args = hasHead ? std::vector<std::string>{"reset", "--quiet", "HEAD", "--"}
        : std::vector<std::string>{"rm", "--cached", "--",};
    args.insert(args.end(),paths.begin(),paths.end()); return Run(args);
}
ProcessResult GitRepository::CommitIndex(const std::string& message) const {
    if (message.find_first_not_of(" \n\r\t") == std::string::npos) return Failure("Enter a commit message.");
    return Run({"commit", "--file=-"},nullptr,message);
}
ProcessResult GitRepository::Permalink(const std::string& path, size_t first, size_t last,
    const std::atomic<bool>* cancel) const {
    if (!first || last < first || path.empty() || path.front() == '/' || path.rfind("../",0)==0)
        return Failure("Select a saved project file and valid line range.");
    // Scintilla exposes a final empty editing line after a trailing newline;
    // GitHub has no corresponding source line. Validate without loading the file.
    auto fullPath=fDirectory+"/"+path;auto stamp=StatFile(fullPath);
    std::ifstream source(fullPath,std::ios::binary);size_t lines=0;bool tail=false;
    char buffer[65536];
    while(source && lines<last) {
        if(cancel && *cancel) return Failure("Operation cancelled.");
        source.read(buffer,sizeof(buffer));auto count=source.gcount();
        for(std::streamsize i=0;i<count;++i) { if(buffer[i]=='\n') { ++lines;tail=false; } else tail=true; }
    }
    if(tail) ++lines;
    if(lines<last) return Failure("The selection is outside the file's source lines.");
    auto head = Run({"rev-parse", "--verify", "HEAD"},cancel);
    if (!head.ok()) return head;
    auto hash=Trim(head.output);
    if (!ValidHash(hash)) return Failure("Cannot resolve the current commit.");
    auto exists=Run({"cat-file", "-e", hash+":"+path},cancel);
    if (!exists.ok()) return Failure("This file is not present in the current commit. Commit it before copying a permalink.");
    std::optional<std::string> repo;
    for (const auto& name : {"origin", "upstream"}) {
        auto remote=Run({"remote", "get-url", name},cancel);
        if (remote.ok()) repo=GitHubRepository(remote.output);
        if (repo) break;
    }
    if (!repo) return Failure("No GitHub remote found. Configure a GitHub origin or upstream remote.");
    auto diff=Run({"diff", "--no-ext-diff", "--no-textconv", "--unified=0", hash, "--", path},cancel);
    if (!diff.ok()) return diff;
    auto baseFirst=MapLineToBase(diff.output,first), baseLast=MapLineToBase(diff.output,last);
    if (!baseFirst || !baseLast || *baseLast-*baseFirst != last-first)
        return Failure("The selection includes uncommitted changes. Commit those lines before copying a permalink.");
    // Endpoints alone are insufficient for replaced lines in the middle of a selection.
    size_t at=0;
    while(at<diff.output.size()) {
        auto line=Field(diff.output,at,'\n');if(line.rfind("@@ ",0)!=0) continue;
        auto plus=line.find(" +");if(plus==std::string_view::npos) continue;
        std::string range(line.substr(plus+2));char* end=nullptr;size_t start=strtoull(range.c_str(),&end,10),count=1;
        if(*end==',') count=strtoull(end+1,&end,10);
        if(count && start<=last && start+count>first)
            return Failure("The selection includes uncommitted changes. Commit those lines before copying a permalink.");
    }
    if(StatFile(fullPath)!=stamp) return Failure("The file changed while creating the permalink. Try again.");
    ProcessResult result; result.exitCode=0;
    result.output=*repo+"/blob/"+hash+"/"+EncodeURLPath(path)+"#L"+std::to_string(*baseFirst);
    if (*baseLast != *baseFirst) result.output+="-L"+std::to_string(*baseLast);
    return result;
}
}
