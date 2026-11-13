#pragma once
#include <cstdint>
#include <vector>
#include <string>
struct DualSound;
DualSound* dual_create(std::vector<uint8_t>,std::vector<uint8_t>);
void dual_destroy(DualSound*);
void dual_advance(DualSound*,uint64_t);
uint8_t dual_read(DualSound*,unsigned);
void dual_write(DualSound*,unsigned,uint8_t);
void dual_irq(DualSound*);
std::string dual_report(DualSound*);
void dual_test();

void dual_write_wav(DualSound*,const std::string&,uint64_t);
