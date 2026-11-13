#pragma once
#include "state_wire.h"
#include <filesystem>
#include <fstream>
#include <atomic>
#ifndef _WIN32
#error "Private laboratory atomic state I/O currently targets Windows"
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#ifndef SUNSET_STATE_ENGINE_ID
#error "Supply the pinned state-engine identity"
#endif
namespace state_file {
using Bytes=std::vector<uint8_t>;
constexpr uint32_t header_size=152;
inline std::array<uint8_t,32> sha256(std::span<const uint8_t> bytes){
    struct Handles {BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;~Handles(){if(hash)BCryptDestroyHash(hash);if(alg)BCryptCloseAlgorithmProvider(alg,0);}} h;
    if(BCryptOpenAlgorithmProvider(&h.alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("cannot open SHA256");
    if(BCryptCreateHash(h.alg,&h.hash,nullptr,0,nullptr,0,0)<0)throw std::runtime_error("cannot create SHA256");
    if(!bytes.empty()&&BCryptHashData(h.hash,const_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(bytes.size()),0)<0)throw std::runtime_error("cannot hash state");
    std::array<uint8_t,32> out{};if(BCryptFinishHash(h.hash,out.data(),out.size(),0)<0)throw std::runtime_error("cannot finish SHA256");return out;
}
inline std::string hex(std::span<const uint8_t> bytes){std::string s;s.reserve(2*bytes.size());for(auto b:bytes){s+="0123456789abcdef"[b>>4];s+="0123456789abcdef"[b&15];}return s;}
inline std::array<uint8_t,64> engine_id(){
    constexpr char id[]=SUNSET_STATE_ENGINE_ID;static_assert(sizeof(id)==65);std::array<uint8_t,64> out{};
    for(unsigned i=0;i<64;++i){char c=id[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))throw std::runtime_error("invalid compiled engine ID");out[i]=uint8_t(c);}return out;
}
inline std::array<uint8_t,32> set_id(){std::array<uint8_t,32> out{};constexpr char id[]="ssriders-eac-4p";std::copy(id,id+sizeof(id)-1,out.begin());return out;}
constexpr std::array<std::array<uint8_t,4>,5> tags={{{'C','P','U','6'},{'B','R','D','1'},{'S','N','D','1'},{'V','I','D','1'},{'C','L','K','1'}}};
inline Bytes envelope(const std::array<Bytes,5>& sections){
    state_wire::Writer payload;
    for(size_t i=0;i<sections.size();++i){state_wire::put(payload,tags[i]);state_wire::put(payload,uint32_t(1));state_wire::put(payload,uint64_t(sections[i].size()));payload.append(sections[i]);}
    state_wire::Writer out;constexpr std::array<uint8_t,8> magic={'S','S','R','S','T','A','T','E'};
    state_wire::put(out,magic);state_wire::put(out,uint32_t(1));state_wire::put(out,header_size);state_wire::put(out,engine_id());state_wire::put(out,set_id());
    state_wire::put(out,uint64_t(payload.bytes.size()));state_wire::put(out,sha256(payload.bytes));
    if(out.bytes.size()!=header_size)throw std::runtime_error("state header layout mismatch");out.append(payload.bytes);return std::move(out.bytes);
}
inline std::array<std::span<const uint8_t>,5> unpack(std::span<const uint8_t> bytes){
    state_wire::Reader r(bytes);std::array<uint8_t,8> magic{};uint32_t version=0,size=0;std::array<uint8_t,64> engine{};std::array<uint8_t,32> set{},hash{};uint64_t n=0;
    state_wire::get(r,magic);state_wire::get(r,version);state_wire::get(r,size);state_wire::get(r,engine);state_wire::get(r,set);state_wire::get(r,n);state_wire::get(r,hash);
    constexpr std::array<uint8_t,8> wanted={'S','S','R','S','T','A','T','E'};
    if(magic!=wanted||version!=1||size!=header_size)throw std::runtime_error("unsupported state signature/version/header");
    if(engine!=engine_id()||set!=set_id())throw std::runtime_error("incompatible state engine/set");
    if(n!=r.remaining())throw std::runtime_error("state payload length mismatch");auto payload=r.take(size_t(n));r.finish();
    if(sha256(payload)!=hash)throw std::runtime_error("state payload checksum mismatch");
    state_wire::Reader p(payload);std::array<std::span<const uint8_t>,5> sections{};
    for(size_t i=0;i<5;++i){std::array<uint8_t,4> tag{};uint32_t v=0;uint64_t length=0;state_wire::get(p,tag);state_wire::get(p,v);state_wire::get(p,length);
        if(tag!=tags[i]||v!=1)throw std::runtime_error("invalid state section order/tag/version");if(length>p.remaining())throw std::runtime_error("truncated state section");sections[i]=p.take(size_t(length));}
    p.finish();return sections;
}
inline Bytes read(const std::filesystem::path& path){
    std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("cannot open state file");auto size=f.tellg();
    if(size<0||uint64_t(size)>state_wire::max_file)throw std::runtime_error("oversized state file");Bytes data(size_t(size),uint8_t(0));f.seekg(0);if(!data.empty())f.read(reinterpret_cast<char*>(data.data()),data.size());if(!f)throw std::runtime_error("truncated state file read");return data;
}
inline void write_atomic(const std::filesystem::path& path,std::span<const uint8_t> bytes,bool fail_before_commit=false){
    if(path.extension()!=L".ssrstate")throw std::runtime_error("diagnostic state extension required");unpack(bytes);
    static std::atomic<unsigned> sequence=0;std::filesystem::path temporary;HANDLE h=INVALID_HANDLE_VALUE;
    for(unsigned tries=0;tries<128;++tries){temporary=path;temporary+=L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(++sequence);h=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(h!=INVALID_HANDLE_VALUE)break;if(GetLastError()!=ERROR_FILE_EXISTS)throw std::runtime_error("cannot create state temporary");}
    if(h==INVALID_HANDLE_VALUE)throw std::runtime_error("state temporary collision limit");
    struct OwnedTemporary {HANDLE& h;std::filesystem::path path;bool committed=false;~OwnedTemporary(){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);if(!committed)DeleteFileW(path.c_str());}} owned{h,temporary};
    size_t pos=0;while(pos<bytes.size()){DWORD done=0;DWORD want=static_cast<DWORD>(std::min<size_t>(bytes.size()-pos,1u<<20));if(!WriteFile(h,bytes.data()+pos,want,&done,nullptr)||!done)throw std::runtime_error("state write failed");pos+=done;}
    if(!FlushFileBuffers(h))throw std::runtime_error("state flush failed");CloseHandle(h);h=INVALID_HANDLE_VALUE;
    if(fail_before_commit)throw std::runtime_error("injected state interruption");
    if(std::filesystem::exists(path)){auto backup=path;backup+=L".bak";if(!CopyFileW(path.c_str(),backup.c_str(),FALSE))throw std::runtime_error("state backup failed");}
    if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("state atomic replacement failed");owned.committed=true;
}
}
