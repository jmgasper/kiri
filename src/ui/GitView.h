#pragma once
#include "core/Git.h"
#include "ui/Theme.h"
#include "ui/Async.h"
#include <View.h>
#include <memory>
class BListView;class BStringView;class BTextControl;class BButton;class BTabView;
namespace kiri {
class Editor;struct EditorSettings;
class GitView:public BView {
public:
    GitView();
    ~GitView() override;
    void AttachedToWindow() override;
    void MessageReceived(BMessage* message) override;
    void SetRepository(std::string root,const std::string& error={});
    void ApplyTheme(const Theme& theme);
    void ApplySettings(const EditorSettings& settings);
    void Refresh();
    void ShowFileHistory(const std::string& path);
    const std::string& Root() const { return fRoot; }
private:
    void LoadHistory(bool more);
    void LoadDiff();
    void Operate(uint32 command);
    std::unique_ptr<AsyncQueue> fJobs;
    std::string fRoot,fHistoryPath;
    BListView* fChanges;
    BListView* fHistory;
    BTextControl* fCommitMessage;
    BStringView* fStatus;
    BStringView* fHistoryTitle;
    BButton* fMore;
    Editor* fDiff;
    std::vector<GitFile> fFiles;
    std::vector<Commit> fCommits;
    CommitGraph fGraph;
    bool fStaged=false,fBusy=false,fHistoryBusy=false;
    Theme fTheme=Theme::Builtins()[0];
    int64 fGeneration=0;
    int64 fDiffRequest=0;
    int64 fHistoryRequest=0,fStatusRequest=0;
};
}
