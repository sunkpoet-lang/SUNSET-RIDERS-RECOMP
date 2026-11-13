#pragma once
#include <cstdint>
#include <array>
#include "primitive_registry.h"
#include <functional>
#include <memory>
#include <vector>
class StatePcm {
public:
    struct Snapshot {
        PrimitiveRegistry::Snapshot chip;
        std::array<uint64_t,7> host;
        std::array<int32_t,2> last;
        uint64_t checksum,rom_identity;
        unsigned schema=1;
        bool operator==(const Snapshot&)const=default;
    };
    Snapshot snapshot()const;
    void restore_component(const Snapshot&);
    explicit StatePcm(std::vector<uint8_t> rom);
    ~StatePcm();
    void advance_master(uint64_t ticks);
    uint8_t read(unsigned reg);
    void write(unsigned reg,uint8_t value);
    uint8_t main_read(unsigned reg);
    void main_write(unsigned reg,uint8_t value);
    void on_native_sample(std::function<void(int32_t,int32_t)>);
    void on_sh1(std::function<void(bool,uint64_t)> callback);
    uint64_t samples()const;
    uint64_t nonzero_samples()const;
    uint64_t digest()const;
    uint64_t sh1_edges()const;
    int32_t last(unsigned channel)const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
