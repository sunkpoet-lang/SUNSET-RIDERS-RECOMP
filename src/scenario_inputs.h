#pragma once
#include "inputs_four.h"
#include <fstream>
#include <sstream>
#include <vector>
// Bounded diagnostic bridge, not the public JSONL reference/replay format.
struct SunsetInputEvent { uint64_t frame;std::array<uint8_t,4> held;uint8_t coins,services; };
inline std::vector<SunsetInputEvent> read_scenario(const std::string& path,uint64_t frames){
    std::ifstream f(path);if(!f)throw std::runtime_error("cannot open scenario");
    std::vector<SunsetInputEvent> out;std::string line;
    while(std::getline(f,line)){
        if(line.empty())continue;
        if(line.size()>256||out.size()>=65536)throw std::runtime_error("scenario exceeds diagnostic limits");
        std::istringstream row(line);long long values[7];
        for(auto& v:values)if(!(row>>v)||v<0)throw std::runtime_error("invalid scenario row");
        std::string extra;if(row>>extra)throw std::runtime_error("extra scenario column");
        if(uint64_t(values[0])>=frames||(!out.empty()&&uint64_t(values[0])<=out.back().frame))throw std::runtime_error("scenario frames must increase within run");
        for(unsigned i=1;i<=4;++i)if(values[i]>63)throw std::runtime_error("invalid EAC controls");
        if(values[5]>15||values[6]>15)throw std::runtime_error("invalid EAC coins/services");
        out.push_back({uint64_t(values[0]),{uint8_t(values[1]),uint8_t(values[2]),uint8_t(values[3]),uint8_t(values[4])},uint8_t(values[5]),uint8_t(values[6])});
    }
    if(f.bad())throw std::runtime_error("scenario read failure");
    return out;
}
