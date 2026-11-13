#pragma once
#include "fm_guard_adapter.h"
namespace fm_guard {
inline unsigned invalid_tests(SunsetFm&live){
    auto saved=live.snapshot();live.validate_component(saved);unsigned n=0;
    auto reject=[&](SunsetFm::Snapshot bad){bad.checksum=SunsetFm::state_checksum(bad.chip_bytes);bool caught=false;
        try{live.restore_component(bad);}catch(const std::runtime_error&){caught=true;}if(!caught||live.snapshot()!=saved)throw std::runtime_error("invalid YM2151 range mutated live chip");++n;};
    for(unsigned op=0;op<32;++op){size_t p=operators_begin+10*op;auto bad=saved;bad.chip_bytes[p+4]=0;bad.chip_bytes[p+5]=4;reject(bad);
        for(unsigned state:{0u,5u}){bad=saved;bad.chip_bytes[p+6]=state;reject(bad);}
        bad=saved;bad.chip_bytes[p+7]=1;reject(bad);bad=saved;bad.chip_bytes[p+8]=2;reject(bad);bad=saved;bad.chip_bytes[p+9]=2;reject(bad);
    }
    for(auto change:std::array<std::pair<unsigned,uint8_t>,14>{{{4,255},{5,0},{5,3},{6,0},{7,2},{8,2},{9,2},{10,uint8_t(saved.chip_bytes[10]^1)},{20,32},{21,2},{647,uint8_t(saved.chip_bytes[647]^1)},{7,uint8_t(saved.chip_bytes[7]^1)},{8,uint8_t(saved.chip_bytes[8]^1)},{9,uint8_t(saved.chip_bytes[9]^1)}}}){
        auto bad=saved;bad.chip_bytes[change.first]=change.second;reject(bad);
    }
    auto bad=saved;std::get<0>(bad.host)=301ull*3579545+1;reject(bad);
    bad=saved;std::get<1>(bad.host)=std::get<0>(saved.host)+65;reject(bad);
    bad=saved;++std::get<2>(bad.host);reject(bad);bad=saved;++std::get<3>(bad.host);reject(bad);bad=saved;++std::get<8>(bad.host);reject(bad);
    bad=saved;std::get<9>(bad.host)[0]=saved.chip_bytes[8]?SunsetFm::never:std::get<0>(saved.host)+64;reject(bad);
    bad=saved;std::get<14>(bad.host).clear();std::get<14>(bad.host).push_back({std::get<0>(saved.host),0,0});reject(bad);
    bad=saved;++std::get<13>(bad.host)[0];reject(bad);return n;
}
}
