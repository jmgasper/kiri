#include "ui/DocumentSettingsWindow.h"
#include "ui/Messages.h"
#include <Button.h>
#include <CheckBox.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>
#include <charconv>

namespace kiri {
namespace {constexpr uint32 kChanged='dchg',kApply='dapp';
bool Width(const char* text,int& width) {std::string value=text;auto parsed=std::from_chars(value.data(),value.data()+value.size(),width);return parsed.ec==std::errc() && parsed.ptr==value.data()+value.size() && width>=1 && width<=16;}
}
DocumentSettingsWindow::DocumentSettingsWindow(BMessenger target,const EditorSettings& defaults,const DocumentStyle& style,
    const DocumentOverrides& overrides,BRect parent,int64 document,const std::string& name)
    :BWindow(BRect(0,0,660,480),document<0?"Editing Defaults — Kiri":"Document Settings — Kiri",B_TITLED_WINDOW,
        B_NOT_ZOOMABLE|B_NOT_RESIZABLE|B_AUTO_UPDATE_SIZE_LIMITS|B_ASYNCHRONOUS_CONTROLS|B_CLOSE_ON_ESCAPE),fTarget(target),fDefaults(defaults),fDocument(document) {
    auto indentation=overrides.indentation.value_or(style.indentation);auto guides=overrides.guides.value_or(style.guides);
    fOverrideIndent=document<0?nullptr:new BCheckBox("override indentation","Override indentation for this document",new BMessage(kChanged));if(fOverrideIndent) fOverrideIndent->SetValue(bool(overrides.indentation));
    fOverrideGuides=document<0?nullptr:new BCheckBox("override guides","Override guides for this document",new BMessage(kChanged));if(fOverrideGuides) fOverrideGuides->SetValue(bool(overrides.guides));
    fTabs=new BCheckBox("indent tabs","Use tabs for indentation (otherwise spaces)",new BMessage(kChanged));fTabs->SetValue(indentation.tabs);
    fIndent=new BTextControl("indent width","Indent width:",std::to_string(indentation.indentWidth).c_str(),new BMessage(kChanged));fIndent->SetModificationMessage(new BMessage(kChanged));
    fTabWidth=new BTextControl("tab width","Tab width:",std::to_string(indentation.tabWidth).c_str(),new BMessage(kChanged));fTabWidth->SetModificationMessage(new BMessage(kChanged));
    fGuides=new BTextControl("guide columns","Guide columns:",GuideColumnsText(guides).c_str(),new BMessage(kChanged));fGuides->SetModificationMessage(new BMessage(kChanged));
    fGuides->SetToolTip("For example: 80, 100. Leave empty for no guides. Guides use the width of a space in the editor font; they never reflow text.");
    fMinimap=document<0?new BCheckBox("show minimap","Show document minimap",new BMessage(kChanged)):nullptr;if(fMinimap) fMinimap->SetValue(defaults.minimap);
    fValidation=new BStringView("document settings validation"," ");fValidation->SetExplicitMinSize(BSize(200,22));fValidation->SetTruncation(B_TRUNCATE_END);
    auto* origin=new BTextView("document setting origins");origin->MakeEditable(false);origin->MakeSelectable(true);origin->SetWordWrap(true);
    auto detail=document<0?"These defaults apply when a document has no matching EditorConfig rule or explicit override. Choose OK here, then Apply or OK in Preferences. Existing text and undo history are preserved.":style.origin+(style.warning.empty()?"":"\n\n"+style.warning)+(style.notes.empty()?"":"\n\n"+style.notes);
    origin->SetText(detail.c_str());auto* scroll=new BScrollView("setting origins scroll",origin,0,false,true,B_PLAIN_BORDER);scroll->SetExplicitMinSize(BSize(550,130));
    fApply=new BButton("apply document settings","OK",new BMessage(kApply));auto* panel=new BView("document settings panel",B_WILL_DRAW);
    auto layout=BLayoutBuilder::Group<>(panel,B_VERTICAL,10).SetInsets(16).Add(new BStringView("document settings title",document<0?"Editing defaults":name.c_str()));
    if(document>=0) layout.Add(fOverrideIndent);
    layout.Add(fTabs).AddGroup(B_HORIZONTAL,12).Add(fIndent).Add(fTabWidth).End();
    if(document>=0) layout.Add(fOverrideGuides);
    layout.Add(fGuides).Add(new BStringView("guide explanation","Guides are visual: space-width columns, without reformatting."));
    if(document<0) layout.Add(fMinimap);
    layout.Add(scroll).Add(fValidation).AddGroup(B_HORIZONTAL,8).AddGlue().Add(new BButton("cancel document settings","Cancel",new BMessage(B_QUIT_REQUESTED))).Add(fApply).End();
    BLayoutBuilder::Group<>(this,B_VERTICAL,0).Add(panel);ThemeView(panel,defaults.Colors());
    SetDefaultButton(fApply);MoveTo(parent.left+35,parent.top+35);MoveOnScreen();Validate();
}
bool DocumentSettingsWindow::Validate() {
    bool indent=fDocument<0 || fOverrideIndent->Value(),guides=fDocument<0 || fOverrideGuides->Value();
    fTabs->SetEnabled(indent);fIndent->SetEnabled(indent);fTabWidth->SetEnabled(indent);fGuides->SetEnabled(guides);
    int a=0,b=0;bool validIndent=!indent || (Width(fIndent->Text(),a) && Width(fTabWidth->Text(),b));
    std::vector<int> columns;std::string error;bool validGuides=!guides || ParseGuideColumns(fGuides->Text(),columns,error);
    fIndent->MarkAsInvalid(indent && !Width(fIndent->Text(),a));fTabWidth->MarkAsInvalid(indent && !Width(fTabWidth->Text(),b));fGuides->MarkAsInvalid(!validGuides);
    if(!validIndent) error="Indent width and tab width must be from 1 to 16.";
    fValidation->SetText(error.empty()?"Changes affect future typing and visual guides.":error.c_str());fValidation->SetToolTip(error.c_str());fApply->SetEnabled(validIndent && validGuides);return validIndent && validGuides;
}
void DocumentSettingsWindow::MessageReceived(BMessage* message) {
    if(message->what==kActivateWorkspace) {Activate();return;}
    if(message->what==kChanged) {Validate();return;}
    if(message->what==kApply && Validate()) {
        Indentation indentation;indentation.tabs=fTabs->Value();Width(fIndent->Text(),indentation.indentWidth);Width(fTabWidth->Text(),indentation.tabWidth);
        std::vector<int> guides;std::string error;ParseGuideColumns(fGuides->Text(),guides,error);
        BMessage reply(fDocument<0?kApplyEditingDefaults:kApplyDocumentSettings);
        if(fDocument<0) {fDefaults.indentation=indentation;fDefaults.guideColumns=std::move(guides);fDefaults.minimap=fMinimap->Value();fDefaults.WriteTo(reply);}
        else {reply.AddInt64("document",fDocument);DocumentOverrides overrides;if(fOverrideIndent->Value()) overrides.indentation=indentation;if(fOverrideGuides->Value()) overrides.guides=std::move(guides);WriteDocumentOverrides(reply,overrides);}
        if(fTarget.SendMessage(&reply)==B_OK) PostMessage(B_QUIT_REQUESTED);return;
    }
    if(message->what==kWindowTheme) {ThemeWindow(this,*message);return;}
    BWindow::MessageReceived(message);
}
}
