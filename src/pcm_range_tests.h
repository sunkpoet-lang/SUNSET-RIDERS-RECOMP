#pragma once
#include "pcm_range_guard.h"
namespace pcm_guard {
template<class T>void set(StatePcm::Snapshot&s,const std::string&name,const T&value){
    auto it=std::find_if(s.chip.begin(),s.chip.end(),[&](const auto&f){return f.name==name;});if(it==s.chip.end())throw std::logic_error("unknown PCM fixture field");
    it->bytes.clear();PrimitiveRegistry::encode(value,it->bytes);s.checksum=PrimitiveRegistry::checksum(s.chip);
}
inline unsigned invalid_tests(StatePcm&live){
    auto saved=live.snapshot();live.validate_component(saved);unsigned checks=0;
    auto reject=[&](const StatePcm::Snapshot&bad){bool caught=false;try{live.restore_component(bad);}catch(const std::runtime_error&){caught=true;}
        if(!caught||live.snapshot()!=saved)throw std::runtime_error("invalid PCM range mutated live chip");++checks;};
    for(unsigned i=0;i<4;++i){auto suffix="/"+std::to_string(i);auto bad=saved;
        set(bad,"m_pitch"+suffix,uint16_t(4096));reject(bad);bad=saved;set(bad,"m_start"+suffix,uint32_t(0x200000));reject(bad);
        bad=saved;set(bad,"m_pan"+suffix,uint8_t(8));reject(bad);bad=saved;set(bad,"m_volume"+suffix,uint8_t(128));reject(bad);
        bad=saved;set(bad,"m_counter"+suffix,uint16_t(0x1040));reject(bad);bad=saved;set(bad,"m_position"+suffix,uint32_t(0x20001));reject(bad);
        bad=saved;int32_t gain[2]={0x7fffffff,0x7fffffff};set(bad,"m_pan_volume"+suffix,gain);reject(bad);
        for(uint16_t pitch:{uint16_t(0),uint16_t(4095)}){auto valid=saved;set(valid,"m_pitch"+suffix,pitch);live.validate_component(valid);}
    }
    auto bad=saved;set(bad,"m_timer_state",uint32_t(4));reject(bad);bad=saved;set(bad,"m_tim2_count",uint32_t(112));reject(bad);
    bad=saved;set(bad,"m_timer_state",uint32_t((field<uint32_t>(saved,"m_timer_state")+1)%4));reject(bad);
    bad=saved;set(bad,"m_tim2_count",uint32_t((field<uint32_t>(saved,"m_tim2_count")+1)%112));reject(bad);
    for(unsigned index:{0u,1u,2u,3u,6u}){bad=saved;if(index==0)bad.host[0]=301ull*3579545+1;else ++bad.host[index];reject(bad);}
    bad=saved;bad.last[0]=32768;reject(bad);return checks;
}
}
