// SPDX-License-Identifier: GPL-3.0-or-later
// Native, illustrated multi-page Calculator manual. No Python/ReportLab.
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using std::string;
static string escape(const string& in) {
  string out;
  for(unsigned char c:in) {
    if(c=='('||c==')'||c=='\\')out+='\\';
    if(c>=32&&c<=126)out+=char(c);
    else if(c>=128)out+='?';
  }
  return out;
}
static bool contains(string s,string needle) {
  std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return char(std::tolower(c));});
  std::transform(needle.begin(),needle.end(),needle.begin(),[](unsigned char c){return char(std::tolower(c));});
  return s.find(needle)!=string::npos;
}
struct Page {string body;};
struct Manual {
  std::vector<Page> pages;
  double y=770;
  size_t number=0;
  static string fmt(double x){std::ostringstream o;o<<std::fixed<<std::setprecision(2)<<x;return o.str();}
  void raw(const string& s){pages.back().body+=s+'\n';}
  void text(double x,double pos,double size,const string& s,bool bold=false) {
    raw("0.11 0.17 0.24 rg BT /"+string(bold?"F2":"F1")+" "+fmt(size)+" Tf 1 0 0 1 "+fmt(x)+" "+fmt(pos)+" Tm ("+escape(s)+") Tj ET");
  }
  void page(){
    pages.push_back({});number++;y=770;
    raw("0.07 0.15 0.23 rg 0 810 595 32 re f");
    raw("0.90 0.98 1 rg BT /F2 9 Tf 1 0 0 1 51 825 Tm (CALCULATOR / USER MANUAL) Tj ET");
    text(51,34,8,"INFILTRATOROS | Calculator | User guide");
    text(530,34,8,std::to_string(number));
  }
  Manual(){page();}
  void ensure(double h){if(y-h<65)page();}
  void heading(string s,double font){
    ensure(font+29);y-=font+10;text(51,y,font,s,true);y-=17;
  }
  void paragraph(const string& s,bool code=false){
    std::istringstream words(s);
    string word,line;const size_t limit=code?88:100;
    auto push=[&](){if(line.empty())return;ensure(16);text(51,y,code?8:9.5,line);y-=code?12:14;line.clear();};
    while(words>>word){
      if(line.size()+word.size()+1>limit)push();
      if(!line.empty())line+=' ';
      line+=word;
    }
    push();y-=4;
  }
  void diagram(const string& mode) {
    ensure(165);double bottom=y-150;
    raw("0.91 0.95 0.97 rg 51 "+fmt(bottom)+" 489 157 re f");
    raw("0.09 0.18 0.27 rg 65 "+fmt(bottom+12)+" 210 132 re f");
    text(79,bottom+120,12,mode,true);
    text(145,bottom+92,12,mode=="STANDARD"?"14":mode=="SCIENTIFIC"?"1.41421356237":"0000 00FF",true);
    const std::vector<string> keys={"MC","MR","M+","MS","7","8","9","/","4","5","6","*","1","2","3","-","0",".","%","+","C","CE","<","="};
    for(size_t i=0;i<keys.size();++i){
      double x=72+50*(i%4),p=bottom+75-13*(i/4);
      raw("0.23 0.34 0.44 rg "+fmt(x)+" "+fmt(p)+" 45 11 re f");
      text(x+7,p+2,7,keys[i]);
    }
    text(294,bottom+123,13,mode+" MODE",true);
    const std::vector<string> notes=mode=="STANDARD"?
      std::vector<string>{"Everyday arithmetic","Operator precedence","Finite binary64 values","Memory and history"}:
      mode=="SCIENTIFIC"?
      std::vector<string>{"Functions and variables","Adjustable precision","Real and complex values","Angle controls"}:
      std::vector<string>{"Binary, octal, decimal, hex","Integer widths","Signed / unsigned views","Bitwise and shifts"};
    for(size_t i=0;i<notes.size();++i)text(294,bottom+98-21*i,9,notes[i]);
    text(64,bottom+4,7,"Concept illustration, not a screenshot.");
    y=bottom-15;
  }
  void write(const std::filesystem::path& dest) {
    std::vector<string> objs={
      "<< /Type /Catalog /Pages 2 0 R >>","",
      "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
      "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold >>"
    };
    string kids;
    for(const auto& pg:pages){
      size_t id=objs.size()+1;kids+=std::to_string(id)+" 0 R ";
      objs.push_back("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Resources << /Font << /F1 3 0 R /F2 4 0 R >> >> /Contents "+std::to_string(id+1)+" 0 R >>");
      objs.push_back("<< /Length "+std::to_string(pg.body.size())+" >>\nstream\n"+pg.body+"endstream");
    }
    objs[1]="<< /Type /Pages /Kids ["+kids+"] /Count "+std::to_string(pages.size())+" >>";
    if(dest.has_parent_path())std::filesystem::create_directories(dest.parent_path());
    std::ofstream out(dest,std::ios::binary);
    if(!out)throw std::runtime_error("Cannot write manual");
    out<<"%PDF-1.4\n%\342\343\317\323\n";
    std::vector<std::streamoff> offsets;
    for(size_t i=0;i<objs.size();++i){offsets.push_back(static_cast<std::streamoff>(out.tellp()));out<<i+1<<" 0 obj\n"<<objs[i]<<"\nendobj\n";}
    auto xref=static_cast<std::streamoff>(out.tellp());
    out<<"xref\n0 "<<objs.size()+1<<"\n0000000000 65535 f \n";
    for(auto offset:offsets)out<<std::setw(10)<<std::setfill('0')<<offset<<" 00000 n \n";
    out<<"trailer\n<< /Size "<<objs.size()+1<<" /Root 1 0 R >>\nstartxref\n"<<xref<<"\n%%EOF\n";
    out.flush();
    if(!out||out.tellp()<10000)throw std::runtime_error("Incomplete PDF manual");
    std::cout<<"Illustrated native Calculator PDF: "<<dest<<" ("<<pages.size()<<" pages)\n";
  }
};
int main(int argc,char**argv){
 try{
  if(argc!=3)throw std::runtime_error("usage: calculator-manual SOURCE.md OUTPUT.pdf");
  std::ifstream input(argv[1]);if(!input)throw std::runtime_error("Missing manual manuscript");
  string content{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
  for(const char* key:{"Standard","Programmer","Operating assumptions"})
    if(!contains(content,key))throw std::runtime_error(string("Missing manual section ")+key);
  Manual doc;doc.heading("CALCULATOR",32);
  doc.heading("User Manual and Operating Principles",15);
  doc.paragraph("A practical guide to everyday calculations, scientific work, programmer operations, controls, and assumptions.");
  doc.diagram("STANDARD");doc.page();
  std::istringstream source(content);string line;
  while(std::getline(source,line)){
    if(!line.empty()&&line.back()=='\r')line.pop_back();
    if(line.empty()){doc.y-=6;continue;}
    if(line.rfind("## ",0)==0) {
      doc.heading(line.substr(3),15);
      if(contains(line,"Scientific"))doc.diagram("SCIENTIFIC");
      else if(contains(line,"Programmer"))doc.diagram("PROGRAMMER");
    } else if(line.rfind("# ",0)==0)doc.heading(line.substr(2),19);
    else if(line.rfind("### ",0)==0)doc.heading(line.substr(4),11);
    else if(line.rfind("> ",0)==0)doc.paragraph("NOTE: "+line.substr(2));
    else if(line.rfind("- ",0)==0)doc.paragraph("  - "+line.substr(2));
    else if(line.rfind("<!--",0)==0)continue;
    else doc.paragraph(line,!line.empty()&&std::isspace(static_cast<unsigned char>(line[0])));
  }
  doc.write(argv[2]);return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
