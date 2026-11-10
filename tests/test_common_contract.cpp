// SPDX-License-Identifier: GPL-3.0-or-later
// Calculator's pinned Common integration and iPhone source boundary, in C++17.
#include <array>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace fs=std::filesystem;
static void check(bool ok,const std::string &why) {
    if(!ok) throw std::runtime_error(why);
}
static std::string read(const fs::path &p) {
    std::ifstream in(p,std::ios::binary);
    check(bool(in),"Cannot read source: "+p.string());
    return std::string(std::istreambuf_iterator<char>(in),{});
}
static std::string trim(std::string s) {
    auto first=s.find_first_not_of("\r\n \t");
    if(first==std::string::npos) return {};
    auto last=s.find_last_not_of("\r\n \t");
    return s.substr(first,last-first+1);
}
static void contains(const std::string &source,const std::string &token,
                     const std::string &context) {
    check(source.find(token)!=std::string::npos,
          context+" missing contract: "+token);
}
static std::string command(const std::vector<std::string> &args) {
    int pipes[2];
    check(::pipe(pipes)==0,"pipe failed");
    pid_t child=::fork();
    check(child>=0,"fork failed");
    if(child==0) {
        ::close(pipes[0]);
        if(::dup2(pipes[1],STDOUT_FILENO)<0) ::_exit(126);
        ::close(pipes[1]);
        std::vector<char *> argv;
        for(const auto &v:args) argv.push_back(const_cast<char *>(v.c_str()));
        argv.push_back(nullptr);
        ::execvp(argv[0],argv.data());
        ::_exit(127);
    }
    ::close(pipes[1]);
    std::string output;
    char data[4096];
    for(;;) {
        ssize_t n=::read(pipes[0],data,sizeof(data));
        if(n==0) break;
        if(n<0 && errno==EINTR) continue;
        check(n>0,"subprocess read failed");
        output.append(data,static_cast<std::size_t>(n));
    }
    ::close(pipes[0]);
    int status=0;
    while(::waitpid(child,&status,0)<0) {
        if(errno!=EINTR) throw std::runtime_error("waitpid failed");
    }
    check(WIFEXITED(status) && WEXITSTATUS(status)==0,
          "Pinned Git revision command failed");
    return trim(output);
}
static std::string object_section(const std::string &data,const std::string &name) {
    const std::string key="\""+name+"\"";
    auto pos=data.find(key);
    check(pos!=std::string::npos,"Common design JSON missing key: "+name);
    pos=data.find(':',pos+key.size());
    check(pos!=std::string::npos,"Malformed Common design JSON key: "+name);
    pos=data.find('{',pos);
    check(pos!=std::string::npos,"Malformed Common design object: "+name);
    size_t start=pos;
    unsigned depth=0;
    bool quoted=false,escaped=false;
    for(;pos<data.size();++pos) {
        char c=data[pos];
        if(quoted) {
            if(escaped) escaped=false;
            else if(c=='\\') escaped=true;
            else if(c=='"') quoted=false;
        } else {
            if(c=='"') quoted=true;
            else if(c=='{') ++depth;
            else if(c=='}' && --depth==0) return data.substr(start,pos-start+1);
        }
    }
    throw std::runtime_error("Unclosed Common JSON object: "+name);
}
static bool source_file(const fs::path &p) {
    static const std::set<std::string> extensions=
        {".c",".cc",".cpp",".h",".hpp",".m",".mm"};
    return extensions.count(p.extension().string())!=0;
}
int main() {
    try {
        const fs::path root=fs::current_path();
        const fs::path common=root/"src/infiltratr-common";
        const std::string expected_version="1.19.57";
        const std::string expected_commit=
            "0cfa4a36cf84034fd77d5c17a59990f068d5c1fb";
        check(trim(read(common/"VERSION"))==expected_version,
              "Common version must match exact pinned contract");
        const std::string cmake=read(root/"CMakeLists.txt");
        contains(cmake,"set(INFILTRATR_COMMON_EXPECTED_VERSION \""+
                       expected_version+"\")","CMake");
        contains(cmake,"set(INFILTRATR_COMMON_EXPECTED_COMMIT \""+
                       expected_commit+"\")","CMake");
        if(fs::exists(root/".git")) {
            check(command({"git","-C",common.string(),"rev-parse","HEAD"})==
                    expected_commit,"Common checkout differs from pinned commit");
            check(command({"git","-C",root.string(),"rev-parse",
                           "HEAD:src/infiltratr-common"})==expected_commit,
                  "Common Gitlink differs from pinned commit");
        }
        std::string public_headers;
        for(const auto &entry:fs::directory_iterator(common/"include/infiltratr")) {
            if(entry.is_regular_file() && entry.path().extension()==".h")
                public_headers+=read(entry.path())+"\n";
        }
        std::set<std::string> calls;
        const std::regex call_re(R"(\b(infiltratr_[a-z0-9_]+)\s*\()");
        for(const fs::path &base:{root/"src",root/"ios/Bridge"}) {
            check(fs::is_directory(base),"Missing Calculator source root");
            for(const auto &entry:fs::recursive_directory_iterator(base)) {
                if(!entry.is_regular_file() || !source_file(entry.path()))
                    continue;
                if(entry.path().string().rfind(common.string(),0)==0)
                    continue;
                const std::string src=read(entry.path());
                for(const char *private_name:{"ascii_internal.h","posix_read_internal.h"})
                    check(src.find(private_name)==std::string::npos,
                          "Private Common header imported: "+entry.path().string());
                for(auto it=std::sregex_iterator(src.begin(),src.end(),call_re);
                         it!=std::sregex_iterator();++it)
                    calls.insert((*it)[1].str());
            }
        }
        check(!calls.empty(),"Calculator does not call any Common public API");
        for(const auto &fn:calls)
            check(std::regex_search(public_headers,
                std::regex("\\b"+fn+"\\s*\\(")),
                "Private or missing Common API called: "+fn);
        const std::string scientific=read(root/"src/core/scientific.cpp");
        check(scientific.find("std::stoi(")==std::string::npos,
              "Private std::stoi parsing must not replace Common's checked parse");
        contains(scientific,"infiltratr_parse_i64(","scientific");
        const std::string design_header=read(common/"include/infiltratr/design.h");
        for(const char *role:{"titlebar_rgb","heading_rgb","summary_rgb","kicker_rgb",
              "detail_label_rgb","note_rgb","status_border_rgb",
              "accent_foreground_rgb","accent_hover_rgb","selected_summary_rgb",
              "warning_muted_rgb","warning_border_rgb","success_border_rgb"})
            contains(design_header,role,"Common semantic design header");
        for(const char *api:{"infiltratr_parse_double_token","infiltratr_parse_double",
              "infiltratr_parse_u64","infiltratr_parse_i64",
              "infiltratr_parse_u64_range","infiltratr_parse_double_range",
              "infiltratr_i64_add_checked","infiltratr_u64_multiply_checked",
              "infiltratr_ascii_is_space","infiltratr_ascii_is_alpha",
              "infiltratr_ascii_is_alnum","infiltratr_ascii_is_digit",
              "infiltratr_ascii_to_lower","infiltratr_ascii_contains_ci",
              "infiltratr_ascii_equal_ci","infiltratr_utf8_encode_codepoint",
              "infiltratr_theme_mode_key","infiltratr_theme_mode_parse",
              "infiltratr_mkdir_parents","infiltratr_atomic_file_write_bytes",
              "infiltratr_read_text_file_alloc","infiltratr_design_metrics",
              "infiltratr_typography"})
            check(calls.count(api)!=0,"Calculator no longer exercises Common API: "+std::string(api));
        const std::string installer=read(common/"cmake/InfiltratrEnsureExtendedFonts.sh");
        std::smatch revision_match;
        check(std::regex_search(installer,revision_match,
                  std::regex(R"(SOURCE_COMMIT="([a-f0-9]{40})")")),
              "Extended typography source must be pinned to exact commit");
        const std::string revision=revision_match[1].str();
        contains(cmake,"set(CALCULATOR_EXTENDED_FONTS_COMMIT \""+revision+"\")",
                 "Calculator fonts");
        contains(cmake,"EXPECTED_HASH","CMake fonts");
        contains(cmake,"CALCULATOR_FONT_MANIFEST_COUNT EQUAL 23","CMake fonts");
        const std::string ios_font=read(root/"ios/bundle-fonts.sh");
        contains(ios_font,"REVISION=\""+revision+"\"","iPhone font source");
        contains(ios_font,"shasum -a 256","iPhone font source integrity");
        contains(ios_font,"wc -l < \"${MANIFEST}\"","iPhone font count");
        const std::string project=read(root/"ios/project.yml");
        const std::string typography=read(root/"ios/Sources/CalculatorTypography.swift");
        for(const char *font:{"mb_corpo_s_text_regular.ttf",
             "mb_corpo_s_text_bold.ttf","mb_corpo_a_title_cond_regular.ttf",
             "mb_corpo_s_text_cond_bold.ttf",
             "mb_corpo_a_screen_2_cond_regular.ttf"}) {
            contains(cmake,font,"Windows font roles");
            contains(ios_font,font,"iPhone font packaging");
            contains(project,font,"iPhone bundle");
            contains(typography,fs::path(font).stem().string(),
                     "iPhone runtime font registration");
        }
        const std::string design=read(common/"design/infiltrator-design-v1.json");
        const std::string palettes=object_section(object_section(design,"theme"),"palettes");
        for(const char *mode:{"day","night"}) {
            const std::string colors=object_section(palettes,mode);
            for(const char *role:{"titlebar","heading","summary","kicker",
                "detail_label","note","status_border","accent_hover",
                "selected_summary","warning_muted","warning_border","success_border"})
                contains(colors,"\""+std::string(role)+"\"",
                         std::string("Common ")+mode+" design palette");
        }
        const std::string common_cmake=read(common/"CMakeLists.txt");
        const std::string start="set(INFILTRATR_COMMON_PORTABLE_SOURCES";
        auto i=common_cmake.find(start);
        check(i!=std::string::npos,"Missing Common Portable sources in its CMake");
        const auto close=common_cmake.find(')',i+start.size());
        check(close!=std::string::npos,"Unclosed Common Portable source list");
        const std::string portable=common_cmake.substr(i,close-i);
        const std::regex source_re(R"(src/[A-Za-z0-9_.-]+\.c)");
        size_t count=0;
        for(auto it=std::sregex_iterator(portable.begin(),portable.end(),source_re);
                 it!=std::sregex_iterator();++it) {
            const std::string file=it->str();
            contains(project,"../src/infiltratr-common/"+file,
                     "iPhone Common portable source list");
            ++count;
        }
        check(count>0,"Common portable source list is empty");
        std::cout<<"Pinned Common "<<expected_version<<" integration contract passed at "
                 <<expected_commit.substr(0,12)<<'\n';
    } catch(const std::exception &e) {
        std::cerr<<"Common contract validation failed: "<<e.what()<<'\n';
        return 1;
    }
    return 0;
}
