#include "sunset_session_profile.h"
#pragma once
#include "cpu_dispatch_guard.h"
namespace cpu_boundary {
inline constexpr std::array<int,8> scratch_values={0,1,2,3,4,8,16,2048};
template<class T>void validate_main(const T&s){
    sunset_guard::validate_main(s);
    if((std::get<8>(s)!=15&&std::get<8>(s)!=16)||std::get<12>(s)>16||std::get<9>(s)!=0||std::get<10>(s)<0||std::get<10>(s)>4096||std::get<40>(s)!=0||std::get<41>(s)!=0||std::get<42>(s)!=0)throw std::runtime_error("invalid completed 68000 slice/index/debt");
    if(std::find(scratch_values.begin(),scratch_values.end(),std::get<11>(s))==scratch_values.end())throw std::runtime_error("unreachable pinned 68000 temporary");
}
template<class T>void validate_sound(const T&s){
    sunset_guard::sound(s);const auto&t=s.second;
    if(std::get<33>(t)>0||std::get<33>(t)<-64||std::get<34>(t)!=4||std::get<35>(t)!=3||std::get<36>(t)!=4||s.first>sunset_session::admission_seconds*8000000)throw std::runtime_error("invalid fixed-profile Z80 slice/timing");
}
}
