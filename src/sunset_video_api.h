#pragma once
#include "board_bus.h"
#include <string>
struct SunsetVideo;
bool video_bus_size_matches(size_t);
SunsetVideo* video_create(std::vector<uint8_t>,std::vector<uint8_t>);
void video_destroy(SunsetVideo*);
void video_edge(SunsetVideo*,const SunsetBus&,bool);
void video_write_ppm(SunsetVideo*,const std::string&);
std::string video_report(SunsetVideo*);
void video_test();
