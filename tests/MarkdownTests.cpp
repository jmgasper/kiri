#include "core/Markdown.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace kiri;
static int checks=0;
#define CHECK(x) do {++checks;if(!(x)) throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+" " #x);} while(0)
static std::string Text(const MarkdownDocument& d) {std::string result;for(auto& b:d.blocks) for(auto& r:b.runs) result+=r.text;return result;}
int main() {
    try {
        auto d=ParseMarkdown("# Café &amp; 日本語\n\nA **bold** and *italic* [link](other.md#part) with ![photo](img/a%20b.png).\n\n- first\n- [x] done\n\n> quote\n\n```cpp\nint x = 1;\n```\n\n| Name | Count |\n| :--- | ---: |\n| A | 2 |\n\n<script>alert('x')</script>\n");
        CHECK(d.error.empty());CHECK(d.blocks.front().type==MD_BLOCK_DOC);CHECK(d.blocks[1].anchor=="café--日本語");CHECK(d.blocks[1].firstLine==0);
        bool heading=false,bold=false,italic=false,link=false,image=false,code=false,table=false,task=false,html=false;
        for(auto& b:d.blocks) {
            heading|=b.type==MD_BLOCK_H;code|=b.type==MD_BLOCK_CODE;table|=b.type==MD_BLOCK_TABLE;task|=b.task=='x';html|=b.type==MD_BLOCK_HTML;
            CHECK(b.firstLine<=b.lastLine);CHECK(b.lastLine<d.lines);
            for(auto& r:b.runs) {bold|=r.style&MarkdownBold;italic|=r.style&MarkdownItalic;link|=r.link=="other.md#part";image|=r.image=="img/a%20b.png";}
        }
        CHECK(heading && bold && italic && link && image && code && table && task && html);CHECK(Text(d).find("<script>alert('x')</script>")!=std::string::npos);
        auto refs=ParseMarkdown("[reference][id] &copy; &#x1F642; &#0; &unknown;\n\n[id]: ../README.md\n\n# Repeat\n# Repeat\n");
        CHECK(Text(refs).find("© 🙂 � &unknown;")!=std::string::npos);CHECK(refs.blocks[1].runs[0].link=="../README.md");CHECK(refs.blocks.back().anchor=="repeat-1");
        auto collision=ParseMarkdown("# Title\n# Title-1\n# Title\n");CHECK(collision.blocks.back().anchor=="title-2");
        auto referenceImage=ParseMarkdown("![alt][image]\n\nParagraph\n\n[image]: local.png\n");
        CHECK(referenceImage.blocks[1].runs[0].line==0 && referenceImage.blocks[1].lastLine==0);
        auto cr=ParseMarkdown("# one\r\n\r\nsecond\rthird\n");CHECK(cr.lines==5);CHECK(cr.blocks[2].firstLine==2);
        for(const auto& malformed:{"", "**unfinished", "[bad](", "```cpp\nunfinished", "![", "<script>\nalert(1)", "- - -", "| a |\n|--|\n| b |"}) {auto m=ParseMarkdown(malformed);CHECK(m.error.empty());}
        CHECK(!ParseMarkdown(std::string(kMarkdownBytes+1,'a')).error.empty());CHECK(!ParseMarkdown(std::string(kMarkdownLines,'\n')).error.empty());
        std::atomic<bool> cancel{true};CHECK(!ParseMarkdown("hello",&cancel).error.empty());
        auto local=ResolveMarkdownTarget("../images/a%20b.png","/work/docs/readme.md",true);CHECK(local.kind==MarkdownTargetKind::Local && local.path=="/work/images/a b.png");
        auto anchor=ResolveMarkdownTarget("#caf%C3%A9","/work/README.md");CHECK(anchor.kind==MarkdownTargetKind::Anchor && anchor.fragment=="café");
        auto relative=ResolveMarkdownTarget("file.md#some-heading","/work/README.md");CHECK(relative.path=="/work/file.md" && relative.fragment=="some-heading");
        CHECK(ResolveMarkdownTarget("https://example.com/a?q=1","",false).kind==MarkdownTargetKind::Web);
        CHECK(ResolveMarkdownTarget("HTTPS://example.com","",false).kind==MarkdownTargetKind::Web);
        CHECK(ResolveMarkdownTarget("https://example.com/image.png","",true).kind==MarkdownTargetKind::Blocked);
        for(auto s:{"javascript:alert(1)","data:text/html,test","file:///tmp/a","mailto:a@b.com","//example.com/a","%2f%2fexample.com/a","a%00b","a%0Ab","bad%ZZ","javascript%3Aalert(1)","a\\b"}) CHECK(ResolveMarkdownTarget(s,"/work/README.md").kind==MarkdownTargetKind::Blocked);
        CHECK(ResolveMarkdownTarget("relative.png","",true).kind==MarkdownTargetKind::Blocked);
        CHECK(ResolveMarkdownTarget("/absolute.png","",true).kind==MarkdownTargetKind::Local);
        // Representative long README, and repeated adversarial delimiters.
        std::string longText;for(int i=0;i<2000;++i) longText+="## Section "+std::to_string(i)+"\n\nText with **emphasis**, [link](a.md), and `code`.\n\n";
        auto start=std::chrono::steady_clock::now();auto large=ParseMarkdown(longText);CHECK(large.error.empty());CHECK(large.blocks.size()>4000);
        auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
        auto adversarial=ParseMarkdown(std::string(20000,'['));CHECK(adversarial.error.empty());CHECK(Text(adversarial).size()==20000);
        std::cout<<"Passed "<<checks<<" Markdown checks; "<<longText.size()<<"-byte README parsed in "<<elapsed<<" ms.\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
