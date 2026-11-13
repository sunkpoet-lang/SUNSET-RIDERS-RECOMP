#pragma once
#include "board_bus.h"
#include "primitive_registry.h"
#include <array>
#include <memory>
// Typed component snapshot, not a public save-state file or complete machine.
struct SunsetBoardSnapshot {
    PrimitiveRegistry::Snapshot fields;
    std::map<uint32_t,uint64_t> unmapped_writes;
    std::array<uint64_t,2> rom_identity;
    uint64_t checksum;
    unsigned schema=1;
    bool operator==(const SunsetBoardSnapshot&)const=default;
};
inline std::unique_ptr<PrimitiveRegistry> board_registry(SunsetBus& b){
    auto r=std::make_unique<PrimitiveRegistry>();
#define BIND(x) r->add(b.x,#x)
#define SPAN(x) r->add_span(b.x.data(),b.x.size(),#x)
    SPAN(ram);SPAN(palette);SPAN(auxiliary);BIND(reads);BIND(writes);
    BIND(watchdog.now);BIND(watchdog.deadline);BIND(watchdog.feeds);BIND(watchdog.armed);BIND(watchdog.soft_reset_requested);
    SPAN(tiles.ram);BIND(tiles.irq_control);BIND(tiles.addrmap);BIND(tiles.scrollctrl);BIND(tiles.romsubbank);BIND(tiles.tileflip);
    BIND(tiles.banks);BIND(tiles.banks2);BIND(tiles.irq);BIND(tiles.rmrd);BIND(tiles.writes);
    r->add_enum(b.eeprom.state,SunsetEeprom::Completion,"eeprom.state");SPAN(eeprom.data);
    BIND(eeprom.cs);BIND(eeprom.clk);BIND(eeprom.di);BIND(eeprom.locked);BIND(eeprom.now);BIND(eeprom.cs_rise);BIND(eeprom.busy_until);
    BIND(eeprom.commands);BIND(eeprom.writes);BIND(eeprom.bits);BIND(eeprom.accumulator);BIND(eeprom.address);BIND(eeprom.shift);
    SPAN(priority.registers);BIND(priority.writes);
    SPAN(sprites.shared);SPAN(sprites.ram);SPAN(sprites.buffer);SPAN(sprites.regs);BIND(sprites.rom_bank);
    BIND(sprites.ram_writes);BIND(sprites.register_writes);BIND(sprites.buffer_updates);BIND(sprites.buffer_clears);BIND(sprites.rom_reads);
    SPAN(inputs.held);BIND(inputs.coins);BIND(inputs.services);
    SPAN(sound.ports);BIND(sound.synchronised);BIND(sound.irq_held);BIND(sound.nmi_line);BIND(sound.irq_requests);BIND(sound.nmi_block_until);
    BIND(protection_reads);BIND(protection_writes);BIND(dim_control);BIND(sprite_rom_bank);BIND(eeprom_control_writes);BIND(dim_video);
    BIND(coin_lines);BIND(coin_counts);BIND(board_control_writes);BIND(master_ticks);BIND(vblank_entries);BIND(vblank_exits);BIND(status_reads);BIND(service_test);
#undef BIND
#undef SPAN
    return r;
}
inline std::array<uint64_t,2> board_rom_identity(const SunsetBus& b){
    auto hash=[](const auto& bytes){uint64_t h=14695981039346656037ull;for(auto v:bytes)h=(h^v)*1099511628211ull;return h;};
    return {hash(b.rom),hash(b.sprites.rom)};
}
inline uint64_t board_state_checksum(const SunsetBoardSnapshot& s){
    uint64_t h=PrimitiveRegistry::checksum(s.fields);auto put=[&](uint64_t v){for(unsigned i=0;i<8;++i)h=(h^uint8_t(v>>(8*i)))*1099511628211ull;};
    for(const auto& [a,count]:s.unmapped_writes){put(a);put(count);}return h;
}
inline SunsetBoardSnapshot board_snapshot(SunsetBus& b){
    SunsetBoardSnapshot s{board_registry(b)->capture(),b.unmapped_writes,board_rom_identity(b),0,1};s.checksum=board_state_checksum(s);return s;
}
template<class T>inline T board_state_value(const SunsetBoardSnapshot& s,const std::string& name){
    for(const auto& f:s.fields)if(f.name==name){T v{};size_t pos=0;if(f.bytes.size()!=PrimitiveRegistry::wire_size<T>())throw std::runtime_error("field type mismatch");PrimitiveRegistry::decode(v,f.bytes,pos,true);return v;}
    throw std::runtime_error("missing board field");
}
inline const std::vector<uint8_t>& board_state_bytes(const SunsetBoardSnapshot& s,const std::string& name){for(const auto& f:s.fields)if(f.name==name)return f.bytes;throw std::runtime_error("missing board span");}
inline void board_restore(SunsetBus& b,const SunsetBoardSnapshot& s){
    if(s.schema!=1||s.rom_identity!=board_rom_identity(b)||board_state_checksum(s)!=s.checksum)throw std::runtime_error("invalid board schema/identity/checksum");
    if(b.ram.size()!=0x4000||b.palette.size()!=0x1000||b.auxiliary.size()!=0x80||b.tiles.ram.size()!=0x4000)throw std::runtime_error("invalid live board shape");
    auto r=board_registry(b);r->validate(s.fields);
    auto clock=board_state_value<uint64_t>(s,"master_ticks");
    if(board_state_value<uint64_t>(s,"eeprom.now")!=clock||board_state_value<uint64_t>(s,"watchdog.now")!=clock||board_state_value<uint64_t>(s,"eeprom.cs_rise")>clock)throw std::runtime_error("board clock mismatch");
    if(board_state_value<bool>(s,"watchdog.armed")&&board_state_value<uint64_t>(s,"watchdog.deadline")<=clock)throw std::runtime_error("expired armed watchdog");
    auto es=board_state_value<int>(s,"eeprom.state");
    if((es==SunsetEeprom::Reading||es==SunsetEeprom::Writing)&&board_state_value<unsigned>(s,"eeprom.address")>=128)throw std::runtime_error("invalid EEPROM address");
    if((es==SunsetEeprom::Command&&board_state_value<unsigned>(s,"eeprom.bits")>10)||(es==SunsetEeprom::Writing&&board_state_value<unsigned>(s,"eeprom.bits")>7))throw std::runtime_error("invalid EEPROM phase");
    for(auto v:board_state_bytes(s,"inputs.held"))if(v>63)throw std::runtime_error("invalid player controls");
    if(board_state_value<uint8_t>(s,"inputs.coins")>15||board_state_value<uint8_t>(s,"inputs.services")>15)throw std::runtime_error("invalid coin/service state");
    for(auto v:board_state_bytes(s,"priority.registers"))if(v>63)throw std::runtime_error("invalid priority register");
    auto bank=board_state_value<uint8_t>(s,"sprite_rom_bank");
    if((bank!=0&&bank!=4)||board_state_value<unsigned>(s,"sprites.rom_bank")!=bank)throw std::runtime_error("sprite ROM bank mismatch");
    if(s.unmapped_writes.size()>65536)throw std::runtime_error("too many diagnostic addresses");
    for(const auto& [address,count]:s.unmapped_writes)if(address>0xffffff||(address&1)||!count)throw std::runtime_error("invalid diagnostic address");
    auto prepared=s; // All potentially failing allocations happen before any mutation.
    r->restore(prepared.fields);b.unmapped_writes.swap(prepared.unmapped_writes);
    // ROMs, callbacks, bindings and external CPU/video objects remain untouched.
}
