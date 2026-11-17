// SPDX-License-Identifier: GPL-3.0-only
// A replay of a Sunset Riders match, .fareplay format 2: what the match was (a JSON header), the machine's state when
// it began, every player's confirmed inputs frame by frame, and state checksums every 60 frames, so that playback can
// tell when it no longer matches. Little-endian:
//   "FAREPLAY", format (uint16, 2), backend (uint16, 0: named in the header)
//   header: length (uint32), UTF-8 JSON: format, backend "konami-ssriders", game, identity (the gameplay identity:
//           a fingerprint of the sources that decide the game's state), rom (the verified regions' SHA-256), version,
//           inputs {players, bytes}, mode, role, players (names, P1 first), started, frames
//   state: raw length, compressed length (uint32 each), zlib (the machine's state at the first frame)
//   inputs: raw length, compressed length, zlib of: players (uint8), then each change: frame delta (varint), each
//           player's input (bytes each, little-endian; Sunset: 2, the six held bits and bit 8 the coin)
//   checksums: count (uint32), then each: frame (uint64), CRC-32 of the machine's state after that many frames
#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <zlib.h>

struct SunsetReplay {
    static constexpr uint16_t kFormat = 2;
    static constexpr const char *kBackend = "konami-ssriders";
    static constexpr const char *kGame = "ssriders-eac";
    using Lanes = std::array<uint16_t, 4>;
    struct Info {
        std::string backend = kBackend, game = kGame, identity, rom, version, mode = "online", role, started;
        std::vector<std::string> players;
        unsigned seats = 4, bytes = 2;
        uint64_t frames = 0;
    };
    Info info;
    std::vector<uint8_t> state;
    std::map<uint64_t, Lanes> inputs;                    // frame -> every lane from that frame on
    std::vector<std::pair<uint64_t, uint32_t>> checks;   // frames run -> the state's CRC then

    // ---- the header's JSON (written here; read with a small reader for these fields only)
    static std::string Quote(const std::string &s) {
        std::string o = "\"";
        for (unsigned char c : s) {
            if (c == '"' || c == '\\') { o += '\\'; o += char(c); }
            else if (c < 32) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; }
            else o += char(c);
        }
        return o + "\"";
    }
    std::string HeaderJson() const {
        std::string j = "{\"format\":2,\"backend\":" + Quote(info.backend) + ",\"game\":" + Quote(info.game) + ",\"identity\":" + Quote(info.identity) +
                        ",\"rom\":" + Quote(info.rom) + ",\"version\":" + Quote(info.version) + ",\"inputs\":{\"players\":" + std::to_string(info.seats) +
                        ",\"bytes\":" + std::to_string(info.bytes) + "},\"mode\":" + Quote(info.mode) + ",\"role\":" + Quote(info.role) + ",\"players\":[";
        for (size_t i = 0; i < info.players.size(); ++i) j += (i ? "," : "") + Quote(info.players[i]);
        return j + "],\"started\":" + Quote(info.started) + ",\"frames\":" + std::to_string(info.frames) + "}";
    }
    struct Json {
        const std::string &s; size_t at = 0; bool ok = true;
        void Space() { while (at < s.size() && (s[at] == ' ' || s[at] == '\n' || s[at] == '\r' || s[at] == '\t')) ++at; }
        bool Eat(char c) { Space(); if (at < s.size() && s[at] == c) { ++at; return true; } return false; }
        std::string String() {
            std::string o; if (!Eat('"')) { ok = false; return o; }
            while (at < s.size() && s[at] != '"') {
                char c = s[at++];
                if (c == '\\' && at < s.size()) {
                    char e = s[at++];
                    if (e == 'u' && at + 4 <= s.size()) { o += char(std::strtol(s.substr(at, 4).c_str(), nullptr, 16)); at += 4; }
                    else o += e == 'n' ? '\n' : e == 't' ? '\t' : e;
                } else o += c;
            }
            if (at >= s.size()) ok = false; else ++at;
            return o;
        }
        uint64_t Number() { Space(); uint64_t v = 0; size_t from = at; while (at < s.size() && s[at] >= '0' && s[at] <= '9') v = v * 10 + uint64_t(s[at++] - '0'); if (at == from) ok = false; return v; }
        void Skip() {
            Space(); if (at >= s.size()) { ok = false; return; }
            if (s[at] == '"') { String(); return; }
            if (s[at] == '{' || s[at] == '[') { const char close = s[at] == '{' ? '}' : ']'; ++at;
                if (Eat(close)) return;
                do { if (close == '}') { String(); if (!Eat(':')) { ok = false; return; } } Skip(); } while (ok && Eat(','));
                if (!Eat(close)) ok = false; return; }
            while (at < s.size() && s[at] != ',' && s[at] != '}' && s[at] != ']') ++at;
        }
    };
    static bool ParseHeader(const std::string &text, Info &info, uint64_t &format) {
        Json j{text}; format = 0;
        if (!j.Eat('{')) return false;
        if (j.Eat('}')) return false;
        do {
            const std::string key = j.String(); if (!j.Eat(':')) return false;
            if (key == "format") format = j.Number();
            else if (key == "backend") info.backend = j.String();
            else if (key == "game") info.game = j.String();
            else if (key == "identity") info.identity = j.String();
            else if (key == "rom") info.rom = j.String();
            else if (key == "version") info.version = j.String();
            else if (key == "mode") info.mode = j.String();
            else if (key == "role") info.role = j.String();
            else if (key == "started") info.started = j.String();
            else if (key == "frames") info.frames = j.Number();
            else if (key == "players") { info.players.clear(); if (!j.Eat('[')) return false;
                if (!j.Eat(']')) { do info.players.push_back(j.String()); while (j.ok && j.Eat(',')); if (!j.Eat(']')) return false; } }
            else if (key == "inputs") { if (!j.Eat('{')) return false;
                do { const std::string k = j.String(); if (!j.Eat(':')) return false;
                     if (k == "players") info.seats = unsigned(j.Number()); else if (k == "bytes") info.bytes = unsigned(j.Number()); else j.Skip(); } while (j.ok && j.Eat(','));
                if (!j.Eat('}')) return false; }
            else j.Skip();
        } while (j.ok && j.Eat(','));
        return j.ok && j.Eat('}');
    }

    // ---- the file
    static void U16(std::vector<uint8_t> &o, uint16_t v) { o.push_back(uint8_t(v)); o.push_back(uint8_t(v >> 8)); }
    static void U32(std::vector<uint8_t> &o, uint32_t v) { for (int i = 0; i < 4; ++i) o.push_back(uint8_t(v >> (8 * i))); }
    static void U64(std::vector<uint8_t> &o, uint64_t v) { for (int i = 0; i < 8; ++i) o.push_back(uint8_t(v >> (8 * i))); }
    static bool Pack(const std::vector<uint8_t> &raw, std::vector<uint8_t> &o) {
        uLongf size = compressBound(uLong(raw.size())); std::vector<uint8_t> z(size);
        if (compress2(z.data(), &size, raw.data(), uLong(raw.size()), 6) != Z_OK) return false;
        U32(o, uint32_t(raw.size())); U32(o, uint32_t(size)); o.insert(o.end(), z.begin(), z.begin() + size); return true;
    }
    std::vector<uint8_t> Bytes() const {
        std::vector<uint8_t> o = {'F', 'A', 'R', 'E', 'P', 'L', 'A', 'Y'};
        U16(o, kFormat); U16(o, 0);
        const std::string header = HeaderJson(); U32(o, uint32_t(header.size())); o.insert(o.end(), header.begin(), header.end());
        if (!Pack(state, o)) return {};
        std::vector<uint8_t> in = {uint8_t(info.seats)}; uint64_t last = 0;
        for (const auto &[frame, lanes] : inputs) {
            uint64_t delta = frame - last; last = frame;
            do { uint8_t b = delta & 127; delta >>= 7; in.push_back(uint8_t(b | (delta ? 128 : 0))); } while (delta);
            for (unsigned p = 0; p < info.seats; ++p) U16(in, lanes[p]);
        }
        if (!Pack(in, o)) return {};
        U32(o, uint32_t(checks.size())); for (const auto &[frame, crc] : checks) { U64(o, frame); U32(o, crc); }
        return o;
    }
    bool Read(const std::vector<uint8_t> &b, std::string &error) {
        size_t at = 0;
        auto need = [&](size_t n) { if (n > b.size() - at) { error = "the replay is cut short"; return false; } return true; };
        auto u16 = [&] { uint16_t v = uint16_t(b[at] | b[at + 1] << 8); at += 2; return v; };
        auto u32 = [&] { uint32_t v = 0; for (int i = 0; i < 4; ++i) v |= uint32_t(b[at + i]) << (8 * i); at += 4; return v; };
        auto u64 = [&] { uint64_t v = 0; for (int i = 0; i < 8; ++i) v |= uint64_t(b[at + i]) << (8 * i); at += 8; return v; };
        if (!need(12) || std::memcmp(b.data(), "FAREPLAY", 8)) { error = "not a replay"; return false; }
        at = 8; const uint16_t format = u16(), backend = u16();
        if (format != kFormat || backend != 0) { error = format > kFormat ? "a replay of a newer version (format " + std::to_string(format) + ")" : "not a Sunset Riders replay (format " + std::to_string(format) + ")"; return false; }
        if (!need(4)) return false; const uint32_t length = u32(); if (length > (1u << 20) || !need(length)) { error = "the replay's header can't be read"; return false; }
        uint64_t said = 0;
        if (!ParseHeader(std::string(b.begin() + at, b.begin() + at + length), info, said) || said != kFormat) { error = "the replay's header can't be read"; return false; }
        at += length;
        if (info.backend != kBackend || info.game != kGame) { error = "a replay of another game (" + info.backend + ", " + info.game + ")"; return false; }
        if (info.seats < 1 || info.seats > 4 || info.bytes != 2) { error = "the replay's inputs are not Sunset Riders' (players 1-4, 2 bytes each)"; return false; }
        auto unpack = [&](std::vector<uint8_t> &out, uint32_t most) {
            if (!need(8)) return false; const uint32_t raw = u32(), packed = u32();
            if (raw > most || !need(packed)) { error = "the replay is damaged"; return false; }
            out.resize(raw); uLongf got = raw;
            if (raw && uncompress(out.data(), &got, b.data() + at, packed) != Z_OK) { error = "the replay is damaged"; return false; }
            if (got != raw) { error = "the replay is damaged"; return false; }
            at += packed; return true;
        };
        std::vector<uint8_t> in;
        if (!unpack(state, 16u << 20) || !unpack(in, 256u << 20)) return false;
        inputs.clear(); size_t i = 0; uint64_t frame = 0;
        if (in.empty() || in[i++] != info.seats) { error = "the replay's inputs are damaged"; return false; }
        while (i < in.size()) {
            uint64_t delta = 0; int shift = 0;
            while (true) { if (i >= in.size() || shift > 56) { error = "the replay's inputs are damaged"; return false; }
                const uint8_t c = in[i++]; delta |= uint64_t(c & 127) << shift; shift += 7; if (!(c & 128)) break; }
            frame += delta; Lanes lanes{};
            if (in.size() - i < size_t(info.seats) * 2) { error = "the replay's inputs are damaged"; return false; }
            for (unsigned p = 0; p < info.seats; ++p) { lanes[p] = uint16_t(in[i] | in[i + 1] << 8); i += 2; }
            inputs[frame] = lanes;
        }
        if (!need(4)) return false; const uint32_t count = u32();
        if (count > (1u << 24) || !need(size_t(count) * 12)) { error = "the replay is damaged"; return false; }
        checks.clear(); for (uint32_t k = 0; k < count; ++k) { const uint64_t f = u64(); checks.emplace_back(f, u32()); }
        if (at != b.size()) { error = "the replay has trailing bytes"; return false; }
        return true;
    }
    bool Save(const std::string &path, std::string &error) const {
        const auto bytes = Bytes(); if (bytes.empty()) { error = "the replay could not be packed"; return false; }
        const std::string partial = path + ".part";
        { std::ofstream f(partial, std::ios::binary); if (!f.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()))) { error = "the replay could not be written"; return false; } }
        std::remove(path.c_str());
        if (std::rename(partial.c_str(), path.c_str())) { error = "the replay could not be written"; return false; }
        return true;
    }
    bool Load(const std::string &path, std::string &error) {
        std::ifstream f(path, std::ios::binary); if (!f) { error = "the replay can't be opened"; return false; }
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        return Read(bytes, error);
    }
    // The lanes of a frame (the last change at or before it)
    Lanes At(uint64_t frame) const {
        auto it = inputs.upper_bound(frame); if (it == inputs.begin()) return {};
        return std::prev(it)->second;
    }
};
