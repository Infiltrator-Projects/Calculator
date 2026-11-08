// SPDX-License-Identifier: GPL-3.0-or-later
// Native C++17 release-asset validator; replaces Python release_assets.py.
#include <openssl/evp.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <unistd.h>
namespace fs=std::filesystem;
static std::string sha256(const fs::path& path) {
    using Digest=std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)>;
    Digest context(EVP_MD_CTX_new(),&EVP_MD_CTX_free);
    if(!context||EVP_DigestInit_ex(context.get(),EVP_sha256(),nullptr)!=1)
        throw std::runtime_error("Cannot initialize SHA-256: "+path.string());
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error("Cannot read asset: "+path.string());
    std::array<char,65536> buffer{};
    while(file) {
        file.read(buffer.data(),std::streamsize(buffer.size()));
        const auto length=file.gcount();
        if(length>0&&EVP_DigestUpdate(context.get(),buffer.data(),std::size_t(length))!=1)
            throw std::runtime_error("SHA-256 update failed: "+path.string());
    }
    if(!file.eof())throw std::runtime_error("Cannot read complete asset: "+path.string());
    unsigned char result[EVP_MAX_MD_SIZE]{};
    unsigned length=0;
    if(EVP_DigestFinal_ex(context.get(),result,&length)!=1||length!=32)
        throw std::runtime_error("SHA-256 finalization failed: "+path.string());
    std::ostringstream hex;
    for(unsigned i=0;i<length;i++)
        hex<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(result[i]);
    return hex.str();
}
static void write_manifest(const fs::path& path,const std::string& content) {
    // Never expose a partially written release manifest.
    const fs::path tmp=path.string()+".tmp";
    {
        std::ofstream output(tmp,std::ios::binary|std::ios::trunc);
        if(!output)throw std::runtime_error("Cannot create "+tmp.string());
        output<<content;
        output.flush();
        if(!output)throw std::runtime_error("Cannot write "+tmp.string());
    }
    std::error_code error;
    fs::rename(tmp,path,error);
    if(error) {
        fs::remove(tmp);
        throw std::runtime_error("Cannot publish "+path.string()+": "+error.message());
    }
}
int main(int argc,char** argv) {
    try {
        std::string version;
        fs::path dist="dist";
        for(int i=1;i<argc;i++) {
            const std::string arg=argv[i];
            if(arg=="--version"&&i+1<argc)version=argv[++i];
            else if(arg=="--dist"&&i+1<argc)dist=argv[++i];
            else throw std::runtime_error("Usage: release-assets --version x.y.z [--dist DIRECTORY]");
        }
        if(!std::regex_match(version,std::regex(R"(^[0-9]+\.[0-9]+\.[0-9]+$)")))
            throw std::runtime_error("Invalid release version: "+version);
        const std::vector<fs::path> assets{
            dist/"linux"/("calculator_"+version+"_amd64.deb"),
            dist/"windows"/("calculator_"+version+"_windows_x64.exe"),
            dist/"ios"/("calculator_"+version+"_ios-device-unsigned.ipa"),
            dist/"source"/("calculator_"+version+"_source.tar.gz"),
            dist/"native"/("calculator-"+version+"-native-installer.run"),
            dist/"LICENSE.txt"
        };
        for(const auto& path:assets) {
            std::error_code error;
            if(!fs::is_regular_file(path,error)||fs::file_size(path,error)==0||error)
                throw std::runtime_error("missing or empty release asset: "+path.string());
        }
        if(access(assets[4].c_str(),X_OK)!=0)
            throw std::runtime_error("native installer is not executable: "+assets[4].string());
        std::string checksums;
        for(const auto& path:assets)
            checksums+=sha256(path)+"  "+path.filename().string()+"\n";
        const fs::path checksum_path=dist/"SHA256SUMS.txt";
        write_manifest(checksum_path,checksums);
        std::string manifest;
        for(const auto& path:assets)manifest+=path.generic_string()+"\n";
        manifest+=checksum_path.generic_string()+"\n";
        write_manifest(dist/"release-assets.txt",manifest);
        std::cout<<"Validated "<<assets.size()<<" release assets plus checksum manifest.\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"release assets: "<<error.what()<<'\n';
        return 1;
    }
}
