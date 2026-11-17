// SPDX-License-Identifier: GPL-3.0-only
// Sunset Riders board for the seat match, shared by the headless peer and the
// online window: rollback states (SFR2), one frame with four lanes, a frame played again. Included after window_machine.cpp
// (with SUNSET_EMBEDDED_MACHINE).
#pragma once
#include <cstring>
#include <span>
#include <zlib.h>

// Rollback's own in-memory states: the machine's five parts with their lengths, without the state file's SHA-256 (a
// file's integrity check; these never leave this process). The machine's restore validates them as for a file.
static std::vector<uint8_t> fast_pack(const MachineCheckpoint &s) {
    state_wire::Writer clock; state_wire::put(clock, s.schema); state_wire::put(clock, s.scenario_identity); state_wire::put(clock, s.host);
    std::array<std::vector<uint8_t>, 5> parts{state_wire::encode(s.cpu), state_wire::encode(s.board), sound_checkpoint_pack(s.sound.get()),
                                              video_checkpoint_pack(s.video.get()), std::move(clock.bytes)};
    size_t total = 4; for (auto &p : parts) total += 8 + p.size();
    std::vector<uint8_t> out; out.reserve(total);
    auto put = [&](uint64_t v, int n) { for (int i = 0; i < n; ++i) out.push_back(uint8_t(v >> (8 * i))); };
    put(0x31504653u, 4); // "SFP1"
    for (auto &p : parts) { put(p.size(), 8); out.insert(out.end(), p.begin(), p.end()); }
    return out;
}
static MachineCheckpoint fast_unpack(std::span<const uint8_t> b) {
    size_t at = 4;
    auto get = [&](int n) { if (at + n > b.size()) throw std::runtime_error("truncated rollback state"); uint64_t v = 0; for (int i = 0; i < n; ++i) v |= uint64_t(b[at + i]) << (8 * i); at += n; return v; };
    if (b.size() < 4 || b[0] != 'S' || b[1] != 'F' || b[2] != 'P' || b[3] != '1') throw std::runtime_error("not a rollback state");
    std::array<std::vector<uint8_t>, 5> parts;
    for (auto &p : parts) { const uint64_t n = get(8); if (n > b.size() - at) throw std::runtime_error("truncated rollback state"); p.assign(b.begin() + at, b.begin() + at + n); at += n; }
    if (at != b.size()) throw std::runtime_error("trailing rollback state");
    state_wire::Reader clock{parts[4]}; unsigned schema; uint64_t profile; std::array<uint64_t, 8> host;
    state_wire::get(clock, schema); state_wire::get(clock, profile); state_wire::get(clock, host); clock.finish();
    if (schema != 1) throw std::runtime_error("invalid machine schema");
    return {state_wire::decode<decltype(std::declval<ProbeCpu &>().snapshot())>(parts[0]), state_wire::decode<SunsetBoardSnapshot>(parts[1]),
            SoundState(sound_checkpoint_unpack(parts[2]), sound_checkpoint_delete), VideoState(video_checkpoint_unpack(parts[3]), video_checkpoint_delete), host, profile, schema};
}

// Rollback's states (SFR2): what the game is, in raw bytes, saved and loaded every frame. The main CPU, the board's
// fields (the board registry's own list), the sound (its component checkpoint without ROM fingerprints, restored in
// place) and the machine's clock. Not the video: the game never reads it, and each frame draws from the board at its
// own edges. These never leave this process (a PC joining replays the inputs), so no file checks.
SoundCheckpoint *sound_fast_checkpoint(DualSound *);
void sound_fast_restore(DualSound *, const SoundCheckpoint *);
struct RawField { void *at; size_t size; const char *name; };
static std::vector<RawField> board_fields(SunsetBus &b) {
    std::vector<RawField> r; r.reserve(96);
#define BIND(x) r.push_back({static_cast<void *>(&b.x), sizeof(b.x), #x})
#define SPAN(x) r.push_back({static_cast<void *>(b.x.data()), b.x.size() * sizeof(b.x[0]), #x})
    SPAN(ram);SPAN(palette);SPAN(auxiliary);BIND(reads);BIND(writes);
    BIND(watchdog.now);BIND(watchdog.deadline);BIND(watchdog.feeds);BIND(watchdog.armed);BIND(watchdog.soft_reset_requested);
    SPAN(tiles.ram);BIND(tiles.irq_control);BIND(tiles.addrmap);BIND(tiles.scrollctrl);BIND(tiles.romsubbank);BIND(tiles.tileflip);
    BIND(tiles.banks);BIND(tiles.banks2);BIND(tiles.irq);BIND(tiles.rmrd);BIND(tiles.writes);
    BIND(eeprom.state);SPAN(eeprom.data);
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
static void rollback_save(ExperimentMachine &m, std::vector<uint8_t> &out) {
    out.clear(); out.reserve(160 * 1024);
    auto put = [&](const void *p, size_t n) { auto c = static_cast<const uint8_t *>(p); out.insert(out.end(), c, c + n); };
    put("SFR2", 4);
    const auto cpu = state_wire::encode(m.cpu.snapshot()); uint64_t n = cpu.size(); put(&n, 8); put(cpu.data(), cpu.size());
    for (const auto &f : board_fields(m.board)) put(f.at, f.size);
    n = m.board.unmapped_writes.size(); put(&n, 8);
    for (const auto &[address, count] : m.board.unmapped_writes) { put(&address, sizeof address); put(&count, sizeof count); }
    SoundCheckpoint *sound = sound_fast_checkpoint(m.sound.get()); const auto bytes = sound_checkpoint_pack(sound); sound_checkpoint_delete(sound);
    n = bytes.size(); put(&n, 8); put(bytes.data(), bytes.size());
    const uint64_t host[8]{m.elapsed, m.instructions, m.max_main_lead, m.next_frame_tick, m.frame, m.event, m.drawn, uint64_t(m.irq_line)};
    put(host, sizeof host);
}
static void rollback_load(ExperimentMachine &m, std::span<const uint8_t> b) {
    size_t at = 0;
    auto get = [&](void *p, size_t n) { if (n > b.size() - at) throw std::runtime_error("truncated rollback state"); std::memcpy(p, b.data() + at, n); at += n; };
    char magic[4]; get(magic, 4);
    if (std::memcmp(magic, "SFR2", 4)) throw std::runtime_error("not a rollback state");
    uint64_t n; get(&n, 8); if (n > b.size() - at) throw std::runtime_error("truncated rollback state");
    auto cpu = state_wire::decode<decltype(std::declval<ProbeCpu &>().snapshot())>(b.subspan(at, n)); at += n;
    for (const auto &f : board_fields(m.board)) get(f.at, f.size);
    get(&n, 8); m.board.unmapped_writes.clear();
    for (uint64_t i = 0; i < n; ++i) { uint32_t address; uint64_t count; get(&address, sizeof address); get(&count, sizeof count); m.board.unmapped_writes[address] = count; }
    get(&n, 8); if (n > b.size() - at) throw std::runtime_error("truncated rollback state");
    SoundCheckpoint *sound = sound_checkpoint_unpack(b.subspan(at, n)); at += n;
    try { sound_fast_restore(m.sound.get(), sound); } catch (...) { sound_checkpoint_delete(sound); throw; }
    sound_checkpoint_delete(sound);
    uint64_t host[8]; get(host, sizeof host);
    if (at != b.size()) throw std::runtime_error("trailing rollback state");
    m.cpu.restore_component(cpu);
    m.elapsed = host[0]; m.instructions = host[1]; m.max_main_lead = host[2]; m.next_frame_tick = host[3]; m.frame = host[4]; m.event = host[5]; m.drawn = host[6]; m.irq_line = host[7] != 0;
}

static void advance(ExperimentMachine &m, std::span<const uint16_t> lanes) {
    std::array<uint8_t, 4> held{}; uint8_t coins = 0;
    for (size_t p = 0; p < lanes.size() && p < 4; ++p) { held[p] = uint8_t(lanes[p] & 63); coins |= uint8_t(((lanes[p] >> 8) & 1) << p); }
    m.live_input = [&m, held, coins](uint64_t) { m.board.inputs.frame(held, coins, 0); };
    m.run_until((m.elapsed / SunsetBus::frame_ticks + 1) * SunsetBus::frame_ticks);
    m.live_input = {};
}
// (a frame rollback plays again: no drawing, as nobody sees it; the frame after it draws from its own edges)
static void advance_again(ExperimentMachine &m, std::span<const uint16_t> lanes) {
    auto draw = std::move(m.board.video_edge_callback);
    m.board.video_edge_callback = [&m](bool entering) { if (entering) ++m.drawn; };
    try { advance(m, lanes); } catch (...) { m.board.video_edge_callback = std::move(draw); throw; }
    m.board.video_edge_callback = std::move(draw);
}

