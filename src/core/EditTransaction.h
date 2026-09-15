#pragma once
#include "core/Project.h"
#include <functional>

namespace kiri {
struct EditSnapshot {
    std::string path,text;
    FileStamp stamp;
    bool bom=false,open=false,readOnly=false;
    int64_t document=0,revision=0;
    int version=0;
    std::string owner;
};
struct SelectedEdit { TextEdit edit;bool selected=true; };
struct FileEdit {
    EditSnapshot before;
    std::vector<SelectedEdit> edits;
    std::string after,status="Ready",error;
    FileStamp writtenStamp;
    int64_t appliedRevision=0;
    bool applied=false,restored=false,pending=false,checkVersion=false;
    std::vector<TextEdit> Selected() const;
    void Prepare();
};
struct EditPlan {
    std::string title,root,summary,journal;
    bool rename=false;
    std::vector<FileEdit> files;
};
using SnapshotMap=std::map<std::string,EditSnapshot>; // canonical absolute paths
EditSnapshot DiskSnapshot(const std::string& path,const std::atomic<bool>* cancel=nullptr);
EditPlan ReplacementPlan(const std::string& root,const ProjectIndex& index,const ProjectSearchOptions& options,
    const std::string& replacement,const SnapshotMap& open,const std::atomic<bool>* cancel=nullptr);
std::string PathFromURI(const std::string& uri);
std::string ValidateRenameName(const std::string& name,const std::string& language={});
// Every URI and range is validated, including operations the client cannot apply.
// The resolver must return the snapshot sent to the server or a checked disk snapshot.
EditPlan WorkspaceEditPlan(const Json& edit,PositionEncoding encoding,
    const std::function<EditSnapshot(const std::string&)>& snapshot);
std::string ValidateEdit(const FileEdit& file,const EditSnapshot& current,bool restore=false);
std::string ApplyDiskEdit(FileEdit& file,bool restore=false);
std::string WriteEditJournal(const EditPlan& plan);
EditPlan ReadEditJournal(const std::string& path);
}
