// SPDX-License-Identifier: GPL-3.0-only
// One PC of a Sunset Riders seat match: the actual EAC machine on the
// backend-neutral seat layer (src/net/seat_match.h): host-star room, 2-4 players with rollback, PCs
// joining and leaving mid-game. Headless; the verified ROM regions arrive on stdin, as the window build takes them.
//   sunset-seat-peer host|join PORT FRAMES SEED LOG [late] [start N] [locals N]
// Each lane: bits 0-5 held, bit 8 the coin. A peer plays made-up input (SEED), pressing coin and start early.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <filesystem>
#include <thread>
#include <vector>
#include <cstdint>
static std::vector<uint8_t> verified_payload;
#define SUNSET_EMBEDDED_MACHINE
#include "window_machine.cpp"
#include "seat_match.h"
#include <zlib.h>

#include "seat_board.h"

int main(int argc, char **argv) {
    try {
        if (argc < 6) throw std::runtime_error("usage: sunset-seat-peer host|join PORT FRAMES SEED LOG [late] [start N] [locals N]");
        recomp::match::Options o;
        o.host = !std::strcmp(argv[1], "host"); o.port = uint16_t(std::atoi(argv[2])); o.address = "127.0.0.1";
        const uint64_t frames = std::strtoull(argv[3], nullptr, 10);
        uint64_t seed = std::strtoull(argv[4], nullptr, 10);
        o.checkpointLog = std::fopen(argv[5], "w");
        o.capacity = 4;
        for (int i = 6; i < argc; ++i) {
            if (!std::strcmp(argv[i], "late")) o.lateJoins = true;
            else if (!std::strcmp(argv[i], "start") && i + 1 < argc) o.startPlayers = unsigned(std::atoi(argv[++i]));
            else if (!std::strcmp(argv[i], "locals") && i + 1 < argc) o.locals = unsigned(std::atoi(argv[++i]));
        }
        o.name = o.host ? "Host" : "Guest " + std::to_string(seed);
        // (the program's own SHA-256, as the bootstrap identity and the profile take it)
        std::ifstream self(argv[0], std::ios::binary);
        std::vector<uint8_t> exe((std::istreambuf_iterator<char>(self)), std::istreambuf_iterator<char>());
        o.build = state_file::hex(state_file::sha256(exe)); exe.clear();
        sunset_rom_payload_sink = &verified_payload; auto machine = load_window_machine(""); sunset_rom_payload_sink = nullptr;
        auto &m = *machine;
        const std::string rom = state_file::hex(state_file::sha256(verified_payload)); verified_payload.clear();
        const std::string profile = "sunset-riders-eac|rom-" + rom + "|lanes-held6-coin|assists-0|turbo-0";
        o.profile.assign(profile.begin(), profile.end());
        o.stateBytes = 1024 * 1024;
        o.scope = recomp::net::BindScope::Loopback;
        window_audio_sink(m.sound.get(), [](int16_t, int16_t) {});
        m.run_until(120ull * SunsetBus::frame_ticks); // (the same short power-on on every PC)

        // (time spent in the board: frames, saves, hashes)
        double frameMs = 0, saveMs = 0, hashMs = 0; uint64_t frameCount = 0, againCount = 0, saveCount = 0;
        auto ms = [](auto from) { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from).count(); };
        if (std::getenv("SUNSET_TRACE")) {
            // (the state's CRC every 60 frames over FRAMES frames of made-up input: run with and without SUNSET_LOCKSTEP)
            auto rng = [&] { seed = seed * 6364136223846793005ull + 1442695040888963407ull; return uint32_t(seed >> 33); };
            uLong audioCrc = 0; window_audio_sink(m.sound.get(), [&audioCrc](int16_t l, int16_t r) { int16_t v[2]{l, r}; audioCrc = crc32(audioCrc, (const Bytef *)v, 4); });
            std::array<uint16_t, 4> lanes{}; const auto t = std::chrono::steady_clock::now();
            // (SUNSET_ROLL: every 7th frame goes back 6 frames and plays 5 again, as a rollback does, and each state it
            // comes to must be the very bytes it saved the first time)
            const bool roll = std::getenv("SUNSET_ROLL") != nullptr; uint64_t rolls = 0, compared = 0;
            std::vector<std::vector<uint8_t>> saves(frames + 1); std::vector<std::array<uint16_t, 4>> played(frames + 1);
            if (roll) rollback_save(m, saves[0]);
            for (uint64_t f = 1; f <= frames; ++f) {
                for (int p = 0; p < 4; ++p) { if (rng() % 6 == 0) lanes[p] = uint16_t(rng() & 63); lanes[p] = uint16_t((lanes[p] & 63) | ((f % 300 == uint64_t(40 + 20 * p)) ? 0x100 : 0)); }
                if (roll && f % 7 == 0 && f > 6) {
                    rollback_load(m, saves[f - 6]); window_audio_sink(m.sound.get(), [](int16_t, int16_t) {});
                    std::vector<uint8_t> again;
                    for (uint64_t k = f - 5; k < f; ++k) {
                        advance_again(m, std::span<const uint16_t>(played[k])); rollback_save(m, again); ++compared;
                        if (again != saves[k]) throw std::runtime_error("rollback state differs at frame " + std::to_string(k));
                    }
                    ++rolls;
                    window_audio_sink(m.sound.get(), [&audioCrc](int16_t l, int16_t r) { int16_t v[2]{l, r}; audioCrc = crc32(audioCrc, (const Bytef *)v, 4); });
                }
                played[f] = lanes;
                ::advance(m, std::span<const uint16_t>(lanes));
                if (roll) { rollback_save(m, saves[f]); if (f > 8) std::vector<uint8_t>().swap(saves[f - 8]); }
                if (f % 60 == 0) { auto c = m.checkpoint(); auto bytes = fast_pack(c); std::fprintf(o.checkpointLog, "%llu cpu %08x board %08x sound %08x video %08x host %llu %llu %llu ram %08x ", static_cast<unsigned long long>(f), unsigned(crc32(0, (const Bytef*)state_wire::encode(c.cpu).data(), uInt(state_wire::encode(c.cpu).size()))), unsigned(crc32(0, (const Bytef*)state_wire::encode(c.board).data(), uInt(state_wire::encode(c.board).size()))), unsigned(crc32(0, sound_checkpoint_pack(c.sound.get()).data(), uInt(sound_checkpoint_pack(c.sound.get()).size()))), unsigned(crc32(0, video_checkpoint_pack(c.video.get()).data(), uInt(video_checkpoint_pack(c.video.get()).size()))), (unsigned long long)c.host[0], (unsigned long long)c.host[1], (unsigned long long)c.host[2], unsigned(crc32(0, m.board.ram.data(), uInt(m.board.ram.size())))); { uLong c = 0; for (const auto &x : board_fields(m.board)) if (std::strcmp(x.name, "master_ticks") && std::strcmp(x.name, "eeprom.now") && std::strcmp(x.name, "watchdog.now")) c = crc32(c, (const Bytef *)x.at, uInt(x.size)); std::fprintf(o.checkpointLog, "fields %08lx ", c); } std::fprintf(o.checkpointLog, "audio %08x rgb %08x %s ", unsigned(audioCrc), unsigned(crc32(0, window_video_rgb(m.video.get()), 288 * 224 * 3)), dual_report(m.sound.get()).c_str()); std::fprintf(o.checkpointLog, "%llu %08x\n", static_cast<unsigned long long>(f), unsigned(crc32(0, bytes.data(), uInt(bytes.size())))); }
            }
            std::fprintf(stderr, "trace: %.2f ms a frame\n", ms(t) / double(frames));
            if (roll) std::fprintf(stderr, "rolls: %llu, %llu states played again and equal\n", (unsigned long long)rolls, (unsigned long long)compared);
            std::fclose(o.checkpointLog);
            return 0;
        }
        recomp::match::Backend b;
        b.save = [&](std::vector<uint8_t> &state) { auto t = std::chrono::steady_clock::now(); rollback_save(m, state); saveMs += ms(t); ++saveCount; return !state.empty(); };
        b.load = [&](std::span<const uint8_t> state) {
            rollback_load(m, state); window_audio_sink(m.sound.get(), [](int16_t, int16_t) {}); return true;
        };
        b.frame = [&](std::span<const uint16_t> lanes, bool again) { auto t = std::chrono::steady_clock::now(); if (again) advance_again(m, lanes); else advance(m, lanes); frameMs += ms(t); ++frameCount; againCount += again; return true; };
        b.hash = [&] { auto t = std::chrono::steady_clock::now(); std::vector<uint8_t> bytes; rollback_save(m, bytes); auto h = uint32_t(crc32(0, bytes.data(), uInt(bytes.size()))); hashMs += ms(t); return h; };

        b.hashSaved = true;
        recomp::match::SeatMatch match;
        std::string error;
        if (!match.Start(o, b, error)) throw std::runtime_error("start: " + error);
        auto rng = [&] { seed = seed * 6364136223846793005ull + 1442695040888963407ull; return uint32_t(seed >> 33); };
        uint64_t shown = 0, played = 0; std::array<uint16_t, 2> held{};
        const auto begun = std::chrono::steady_clock::now();
        auto last = begun;
        while (shown < frames && match.GetStatus() != recomp::match::SeatMatch::Status::Ended) {
            if (std::chrono::steady_clock::now() - begun > std::chrono::seconds(400)) throw std::runtime_error("timeout");
            for (unsigned i = 0; i < o.locals; ++i) {
                if (rng() % 6 == 0) held[i] = uint16_t(rng() & 63);
                // (a coin now and then early on, so every lane plays)
                held[i] = uint16_t((held[i] & 63) | ((played % 300 == 40 + 20 * i) ? 0x100 : 0));
            }
            const int seen = match.Tick(std::span<const uint16_t>(held.data(), o.locals));
            shown += uint64_t(seen); played += uint64_t(seen);
            // (paced at the board's 60 frames a second)
            last += std::chrono::microseconds(16667);
            std::this_thread::sleep_until(last);
        }
        std::fprintf(stderr, "sunset-seat-peer: %llu frames, global frame %llu, epoch %llu, %s\n", static_cast<unsigned long long>(shown),
                     static_cast<unsigned long long>(match.Frame()), static_cast<unsigned long long>(match.Epoch()), match.Message().c_str());
        std::fprintf(stderr, "sunset-seat-peer: board time: %llu frames (%llu again) %.2f ms each, %llu saves %.2f ms each, hashes %.0f ms in all\n",
                     static_cast<unsigned long long>(frameCount), static_cast<unsigned long long>(againCount), frameCount ? frameMs / frameCount : 0.0,
                     static_cast<unsigned long long>(saveCount), saveCount ? saveMs / saveCount : 0.0, hashMs);
        std::fclose(o.checkpointLog);
        return match.Failed() ? 1 : 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "sunset-seat-peer: %s\n", e.what());
        return 1;
    }
}
