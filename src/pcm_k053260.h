#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
class SunsetPcm {
public:
    explicit SunsetPcm(std::vector<uint8_t> rom);
    ~SunsetPcm();
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
