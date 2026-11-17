// SPDX-License-Identifier: GPL-3.0-only
// Sunset Riders online in its window: the window machine, the seat
// match (src/net/seat_match.h) with rollback for 2-4 players on up to four PCs, this PC's controls
// (one or two players: keyboard layouts P1/P2 and the first gamepads), the names of every seat in its player's colour,
// and the menu in its online mode (no states, cheats, turbo or pause: the game runs on for everyone).
//   Sunset-Riders-Online --host PORT | --join ADDRESS:PORT  [--players N] [--start N] [--late] [--locals N] [--name NAME]
//                 [--delay N] [--log FILE] [--auto SEED] [--frames N] [--capture PREFIX] [--hidden]
// The verified ROM regions arrive on stdin, as the window build takes them. --auto plays made-up input (tests).
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
#include "seat_board.h"
#include "sunset_replay.h"
#include <ctime>

namespace {
struct Rgb { uint8_t r, g, b; };
constexpr Rgb kSeatColour[4] = {{90, 150, 255}, {255, 90, 90}, {110, 220, 120}, {255, 205, 80}};

// This PC's players from the window's controls, without touching the board's own inputs (the match's state is the
// same on every PC: the frame's lanes are applied by the match, at the frame's tick)
void LocalLanes(WindowControls &controls, SunsetBus &board, unsigned locals, std::array<uint16_t, 2> &lanes) {
    const SunsetInputs before = board.inputs;
    controls.latch(board);
    for (unsigned i = 0; i < locals && i < 2; ++i)
        lanes[i] = uint16_t((board.inputs.held[i] & 63) | (((board.inputs.coins >> i) & 1) ? 0x100 : 0));
    board.inputs = before;
}

// Every seated player's name under that player's money, on a dark strip (an empty seat shows nothing)
void DrawNames(SDL_Renderer *r, const std::array<std::string, 4> &names, unsigned seats) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (unsigned seat = 0; seat < seats && seat < 4; ++seat) {
        if (names[seat].empty()) continue;
        std::string label = names[seat]; if (label.size() > 16) label.resize(16);
        const float left = float(seat) * 144.0f + 6.0f, width = float(label.size()) * 8.0f + 8.0f;
        SDL_SetRenderDrawColor(r, 0, 0, 0, 150); SDL_FRect back{left, 76, width, 12}; SDL_RenderFillRect(r, &back);
        const auto &c = kSeatColour[seat]; SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
        SDL_RenderDebugText(r, left + 4, 78, label.c_str());
    }
}
void DrawMessage(SDL_Renderer *r, const std::string &text) {
    if (text.empty()) return;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    const float width = float(std::min<size_t>(text.size(), 64)) * 8.0f + 24.0f;
    SDL_SetRenderDrawColor(r, 0, 0, 0, 190); SDL_FRect box{(576 - width) / 2, 236, width, 28}; SDL_RenderFillRect(r, &box);
    SDL_SetRenderDrawColor(r, 255, 220, 80, 255); SDL_RenderDebugText(r, box.x + 12, 246, text.substr(0, 64).c_str());
}
std::string LocalTime(const char *format) {
    const std::time_t now = std::time(nullptr); std::tm local{}; localtime_s(&local, &now);
    char text[64]; std::strftime(text, sizeof text, format, &local); return text;
}

// Plays a replay of a match: the machine starts as every match does (its state must be the replay's first one, byte
// for byte: no state is loaded from the file), then runs every frame's lanes and compares the state's CRC at each
// checksum. Returns 0 when it plays to its end with every checksum matching, 1 when it stops matching, 6 refused.
int PlayReplay(ExperimentMachine &m, const std::string &rom, const std::string &path, bool headless, bool hidden, const std::string &capture) {
    SunsetReplay replay; std::string error;
    if (!replay.Load(path, error)) { std::fprintf(stderr, "replay: %s\n", error.c_str()); return 6; }
    if (replay.info.rom != rom) { std::fprintf(stderr, "replay: a replay of another ROM set\n"); return 6; }
    if (replay.info.identity != SUNSET_REPLAY_IDENTITY) {
        std::fprintf(stderr, "replay: made with another version of the game's emulation (identity %.12s, this build %.12s)\n", replay.info.identity.c_str(), SUNSET_REPLAY_IDENTITY);
        return 6;
    }
    window_audio_sink(m.sound.get(), [](int16_t, int16_t) {});
    m.run_until(120ull * SunsetBus::frame_ticks);
    { std::vector<uint8_t> own; rollback_save(m, own); if (own != replay.state) { std::fprintf(stderr, "replay: it does not start from this game's own start\n"); return 6; } }
    std::array<std::string, 4> names{}; for (size_t i = 0; i < replay.info.players.size() && i < 4; ++i) names[i] = replay.info.players[i];
    std::map<uint64_t, uint32_t> checks(replay.checks.begin(), replay.checks.end());
    uint64_t matched = 0, frame = 0, stopped = UINT64_MAX;
    if (auto it = checks.find(0); it != checks.end()) { if (uint32_t(crc32(0, replay.state.data(), uInt(replay.state.size()))) == it->second) ++matched; else stopped = 0; }
    SDL_Window *w = nullptr; SDL_Renderer *r = nullptr; SDL_Texture *texture = nullptr; std::unique_ptr<WindowAudio> audio;
    if (!headless) {
        if (hidden) SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
        SDL_SetMainReady(); sdl_require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO), "SDL init");
        w = SDL_CreateWindow("Sunset Riders EAC 4P | replay", 864, 708, hidden ? SDL_WINDOW_HIDDEN : SDL_WINDOW_RESIZABLE); sdl_require(w != nullptr, "window");
        r = SDL_CreateRenderer(w, nullptr); sdl_require(r != nullptr, "renderer");
        SDL_SetRenderLogicalPresentation(r, 576, 472, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        texture = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 288, 224); sdl_require(texture != nullptr, "framebuffer texture");
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        audio = std::make_unique<WindowAudio>(); if (hidden) audio->gain(-1.0f);
        window_audio_sink(m.sound.get(), [&](int16_t left, int16_t right) { audio->push(left, right); });
    }
    uint64_t next = SDL_GetTicksNS(); bool quit = false; std::string message;
    while (!quit) {
        if (w) { SDL_Event e; while (SDL_PollEvent(&e)) if (e.type == SDL_EVENT_QUIT || (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_ESCAPE)) quit = true; }
        if (frame < replay.info.frames && stopped == UINT64_MAX) {
            const auto lanes = replay.At(frame);
            advance(m, std::span<const uint16_t>(lanes.data(), replay.info.seats)); ++frame;
            if (auto it = checks.find(frame); it != checks.end()) {
                std::vector<uint8_t> own; rollback_save(m, own);
                if (uint32_t(crc32(0, own.data(), uInt(own.size()))) == it->second) ++matched; else stopped = frame;
            }
            if (audio) audio->flush();
        } else if (message.empty()) {
            message = stopped != UINT64_MAX ? "Replay stops matching at frame " + std::to_string(stopped) : "End of the replay";
            if (headless || hidden) quit = true;
        }
        if (w) {
            sdl_require(SDL_UpdateTexture(texture, nullptr, window_video_rgb(m.video.get()), 288 * 3), "texture update");
            SDL_SetRenderDrawColor(r, 12, 12, 16, 255); SDL_RenderClear(r);
            SDL_FRect game{0, 24, 576, 448}; SDL_RenderTexture(r, texture, nullptr, &game);
            DrawNames(r, names, replay.info.seats);
            SDL_SetRenderDrawColor(r, 230, 230, 235, 255);
            const std::string title = "Replay | " + replay.info.started + " | frame " + std::to_string(frame) + " of " + std::to_string(replay.info.frames) + " | Esc quits";
            SDL_RenderDebugText(r, 8, 8, title.c_str());
            DrawMessage(r, message);
            if (quit && !capture.empty()) if (auto *shot = SDL_RenderReadPixels(r, nullptr)) { SDL_SaveBMP(shot, (capture + ".bmp").c_str()); SDL_DestroySurface(shot); }
            SDL_RenderPresent(r);
            if (!hidden) { next += 16666667ull; const uint64_t now = SDL_GetTicksNS(); if (next > now) SDL_DelayNS(next - now); else next = now; }
        }
    }
    if (stopped != UINT64_MAX)
        std::fprintf(stderr, "replay: stops matching at frame %llu; %llu of %zu checkpoints matched before\n", static_cast<unsigned long long>(stopped), static_cast<unsigned long long>(matched), checks.size());
    else if (frame >= replay.info.frames)
        std::fprintf(stderr, "replay: played to its end; %llu of %zu checkpoints matched\n", static_cast<unsigned long long>(matched), checks.size());
    if (stopped == UINT64_MAX && frame < replay.info.frames) std::fprintf(stderr, "replay: stopped by the player at frame %llu\n", static_cast<unsigned long long>(frame));
    if (texture) SDL_DestroyTexture(texture); if (r) SDL_DestroyRenderer(r); if (w) SDL_DestroyWindow(w); audio.reset(); if (w) SDL_Quit();
    return stopped != UINT64_MAX ? 1 : 0;
}
}

int main(int argc, char **argv) {
    try {
        recomp::match::Options o;
        o.capacity = 4; o.scope = recomp::net::BindScope::Any; o.delay = 2;
        std::string log, capture, replayFile, replays; uint64_t autoSeed = 0, frames = 0; bool autoPlay = false, hidden = false, mode = false, headless = false, replaysSet = false;
        for (int i = 1; i < argc; ++i) {
            const std::string a = argv[i];
            auto next = [&]() -> std::string { if (i + 1 >= argc) throw std::runtime_error("missing value for " + a); return argv[++i]; };
            if (a == "--host") { o.host = true; o.port = uint16_t(std::stoi(next())); mode = true; }
            else if (a == "--join") { const std::string at = next(); const auto colon = at.rfind(':'); if (colon == std::string::npos) throw std::runtime_error("--join ADDRESS:PORT");
                o.host = false; o.address = at.substr(0, colon); o.port = uint16_t(std::stoi(at.substr(colon + 1))); mode = true; }
            else if (a == "--players") o.capacity = unsigned(std::stoi(next()));
            else if (a == "--start") o.startPlayers = unsigned(std::stoi(next()));
            else if (a == "--late") o.lateJoins = true;
            else if (a == "--locals") o.locals = unsigned(std::stoi(next()));
            else if (a == "--name") o.name = next();
            else if (a == "--delay") o.delay = unsigned(std::stoi(next()));
            else if (a == "--log") log = next();
            else if (a == "--auto") { autoPlay = true; autoSeed = std::stoull(next()); }
            else if (a == "--frames") frames = std::stoull(next());
            else if (a == "--capture") capture = next();
            else if (a == "--hidden") hidden = true;
            else if (a == "--loopback") o.scope = recomp::net::BindScope::Loopback;
            else if (a == "--replay") replayFile = next();
            else if (a == "--replays") { replays = next(); replaysSet = true; }
            else if (a == "--headless") headless = true;
            else throw std::runtime_error("unknown option " + a);
        }
        if (!mode && replayFile.empty()) throw std::runtime_error("usage: Sunset-Riders-Online --host PORT | --join ADDRESS:PORT [--players N] [--start N] [--late] [--locals N] [--name NAME] [--replays FOLDER|none] | --replay FILE [--headless]");
        { std::string name; for (char c : o.name) if (c >= 32 && c < 127 && name.size() < 16) name += c; o.name = name.empty() ? "Player" : name; }
        if (!log.empty()) { o.checkpointLog = std::fopen(log.c_str(), "w"); if (!o.checkpointLog) throw std::runtime_error("the checkpoint log could not be opened"); }

        // (the program's own SHA-256, as the bootstrap identity and the profile take it)
        std::ifstream self(argv[0], std::ios::binary);
        std::vector<uint8_t> exe((std::istreambuf_iterator<char>(self)), std::istreambuf_iterator<char>());
        o.build = state_file::hex(state_file::sha256(exe)); exe.clear();
        sunset_rom_payload_sink = &verified_payload; auto machine = load_window_machine(""); sunset_rom_payload_sink = nullptr;
        auto &m = *machine;
        const std::string rom = state_file::hex(state_file::sha256(verified_payload)); verified_payload.clear();
        if (!replayFile.empty()) return PlayReplay(m, rom, replayFile, headless, hidden || autoPlay, capture);
        // (the match's replay goes to the replays folder beside the program unless --replays says otherwise or none)
        if (!replaysSet) replays = (std::filesystem::path(SDL_GetBasePath() ? SDL_GetBasePath() : "") / "replays").string();
        const std::string profile = "sunset-riders-eac|rom-" + rom + "|lanes-held6-coin|assists-0|turbo-0";
        o.profile.assign(profile.begin(), profile.end());
        o.stateBytes = 1024 * 1024;

        if (hidden || autoPlay) SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
        SDL_SetMainReady(); sdl_require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD), "SDL init");
        struct SdlLifetime { ~SdlLifetime() { SDL_Quit(); } } sdlLifetime;
        auto *w = SDL_CreateWindow("Sunset Riders EAC 4P | online (experimental)", 864, 708, (hidden || autoPlay) ? SDL_WINDOW_HIDDEN : SDL_WINDOW_RESIZABLE);
        sdl_require(w != nullptr, "window");
        auto *r = SDL_CreateRenderer(w, nullptr); sdl_require(r != nullptr, "renderer"); SDL_SetRenderVSync(r, 0);
        SDL_SetRenderLogicalPresentation(r, 576, 472, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        auto *texture = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 288, 224); sdl_require(texture != nullptr, "framebuffer texture");
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        std::string icon = std::string(SDL_GetBasePath()) + "title-icon.bmp"; if (auto *s = SDL_LoadBMP(icon.c_str())) { SDL_SetWindowIcon(w, s); SDL_DestroySurface(s); }

        // The menu in its online mode: states, cheats, turbo and pause are refused by the policy
        sunset_menu::PadMenuInput padMenu; sunset_menu::Adapter overlay; overlay.policy.enter(sunset_menu::Mode::Online);
        overlay.open(); overlay.menu.Note("Online match: states, cheats, turbo and pause are off; the game runs on."); overlay.menu.Close();
        recomp::MenuBar bar; sunset_menu::BarModel barModel(overlay); bar.SetArea(576, 472, 2);
        auto refreshBar = [&] { bar.Configure(barModel.entries(false, false)); }; refreshBar();
        auto *barTexture = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, 576, 472); sdl_require(barTexture != nullptr, "bar texture");
        SDL_SetTextureBlendMode(barTexture, SDL_BLENDMODE_NONE);
        WindowAudio audio; WindowControls controls; controls.window = w;
        { int count = 0; auto *ids = SDL_GetGamepads(&count); for (int i = 0; i < count; ++i) controls.connect(ids[i]); SDL_free(ids); }
        auto bindAudio = [&] { window_audio_sink(m.sound.get(), [&](int16_t left, int16_t right) { audio.push(left, right); }); };
        auto mute = [&] { window_audio_sink(m.sound.get(), [](int16_t, int16_t) {}); };
        mute();
        m.run_until(120ull * SunsetBus::frame_ticks); // (the same short power-on on every PC)
        bindAudio();

        // The match's replay: its start (the state every PC begins from), a checksum every 60 frames, the names seated
        SunsetReplay record; rollback_save(m, record.state); record.info.started = LocalTime("%Y-%m-%dT%H:%M:%S");
        const std::string fileTime = LocalTime("%Y-%m-%d_%H-%M-%S");
        std::map<uint64_t, uint32_t> checks; uint32_t lastCrc = 0; std::array<std::string, 4> seated{};
        recomp::match::SeatMatch match;
        recomp::match::Backend b;
        b.save = [&](std::vector<uint8_t> &state) { rollback_save(m, state); lastCrc = uint32_t(crc32(0, state.data(), uInt(state.size()))); return !state.empty(); };
        // (GekkoNet's frame N is the board after N + 1 frames: its checksum is kept under the frames run)
        b.saved = [&](int frame) { const uint64_t run = match.EpochBase() + uint64_t(frame) + 1; if (run % 60 == 0) checks[run] = lastCrc; return true; };
        b.load = [&](std::span<const uint8_t> state) { rollback_load(m, state); return true; };
        b.frame = [&](std::span<const uint16_t> lanes, bool again) {
            // (a frame played again is neither seen nor heard)
            if (again) { mute(); advance_again(m, lanes); bindAudio(); } else advance(m, lanes);
            return true;
        };
        b.hash = [&] { std::vector<uint8_t> bytes; rollback_save(m, bytes); return uint32_t(crc32(0, bytes.data(), uInt(bytes.size()))); };
        b.hashSaved = true;
        std::string error;
        if (!match.Start(o, std::move(b), error)) throw std::runtime_error(error);

        auto key = [&](SDL_Scancode code) { SDL_Event e{}; e.type = SDL_EVENT_KEY_DOWN; e.key.scancode = code; e.key.down = true; SDL_PushEvent(&e); };
        auto menuButton = [](SDL_Scancode c) { return c == SDL_SCANCODE_UP ? recomp::kMenuUp : c == SDL_SCANCODE_DOWN ? recomp::kMenuDown : c == SDL_SCANCODE_LEFT ? recomp::kMenuLeft
                                                   : c == SDL_SCANCODE_RIGHT ? recomp::kMenuRight : c == SDL_SCANCODE_RETURN || c == SDL_SCANCODE_KP_ENTER ? recomp::kMenuConfirm
                                                   : c == SDL_SCANCODE_BACKSPACE || c == SDL_SCANCODE_ESCAPE ? recomp::kMenuBack : 0u; };
        bool quit = false, fullscreen = false; std::string status = "Online: states, cheats, turbo and pause are off for everyone.";
        auto dispatch = [&](recomp::Menu::Chosen action) {
            if (action.kind == recomp::Menu::Chosen::Quit) quit = true;
            if ((action.kind == recomp::Menu::Chosen::Save || action.kind == recomp::Menu::Chosen::Load)) { status = "States are unavailable online."; overlay.menu.Note(status); }
            if (action.kind == recomp::Menu::Chosen::Action) {
                if (action.id == sunset_menu::Adapter::Fullscreen) { fullscreen = !fullscreen; SDL_SetWindowFullscreen(w, fullscreen); }
                if (action.id == sunset_menu::Adapter::Controls) { status = "This PC: P1 arrows Z/X, coin 1" + std::string(o.locals > 1 ? "; P2 WASD F/G, coin 2" : "") + "; gamepads A/B, Start = coin."; overlay.menu.Note(status); }
                if (action.id == sunset_menu::BarModel::About) { status = "Sunset Riders EAC online: rollback for up to four players (experimental)."; overlay.open(); overlay.menu.Note(status); }
                if (action.id == sunset_menu::BarModel::Shortcuts) { status = "Esc menu; F10 bar; +/- volume; F11 fullscreen. No pause online."; overlay.open(); overlay.menu.Note(status); }
            }
            audio.volume = overlay.muted ? 0.0f : overlay.volume / 100.0f; if (audio.stream) SDL_SetAudioStreamGain(audio.stream, audio.volume);
            refreshBar();
        };

        uint64_t rng = autoSeed * 2654435761u + 1, shown = 0, presented = 0, endedAt = 0, nextFrame = SDL_GetTicksNS();
        auto random = [&] { rng = rng * 6364136223846793005ull + 1442695040888963407ull; return uint32_t(rng >> 33); };
        std::array<uint16_t, 2> lanes{};
        while (!quit) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) quit = true;
                if (e.type == SDL_EVENT_GAMEPAD_ADDED) controls.connect(e.gdevice.which);
                if (e.type == SDL_EVENT_GAMEPAD_REMOVED) { controls.disconnect(e.gdevice.which); padMenu.remove(e.gdevice.which); }
                if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
                    const SDL_JoystickID id = e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ? e.gbutton.which : e.gaxis.which; bool known = false;
                    for (auto *pad : controls.pads) if (pad && SDL_GetGamepadID(pad) == id) known = true;
                    if (known) { auto translated = padMenu.translate(e, overlay.menu.IsOpen() || bar.IsOpen()); if (translated != SDL_SCANCODE_UNKNOWN) key(translated); }
                }
                if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST) { bar.Close(); overlay.menu.Close(); padMenu.clear(); controls.clear(); }
                if (e.type == SDL_EVENT_MOUSE_MOTION && bar.IsOpen() && !overlay.menu.IsOpen()) { float x, y; if (SDL_RenderCoordinatesFromWindow(r, e.motion.x, e.motion.y, &x, &y)) bar.Pointer(x, y); }
                if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !overlay.menu.IsOpen()) {
                    float x, y; if (!SDL_RenderCoordinatesFromWindow(r, e.button.x, e.button.y, &x, &y)) continue;
                    if (e.button.button == SDL_BUTTON_LEFT && (bar.IsOpen() || bar.Hit(x, y))) { if (!bar.IsOpen()) { refreshBar(); controls.clear(); }
                        auto choice = bar.Click(x, y); if (choice.chosen) { dispatch(barModel.choose(choice.id)); bar.Close(); } }
                    else if (e.button.button == SDL_BUTTON_RIGHT) { if (!bar.IsOpen()) { refreshBar(); controls.clear(); } bar.OpenPopup(x, y); }
                }
                if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
                    const auto c = e.key.scancode;
                    if (c == SDL_SCANCODE_F10) { if (bar.IsOpen()) bar.Close(); else { refreshBar(); controls.clear(); bar.OpenMenu(0); } continue; }
                    if (bar.IsOpen()) { if (unsigned bt = menuButton(c)) { auto choice = bar.Buttons(bt); if (choice.chosen) { dispatch(barModel.choose(choice.id)); bar.Close(); } } continue; }
                    if (c == SDL_SCANCODE_ESCAPE) { if (overlay.menu.IsOpen()) overlay.menu.Close(); else { overlay.open(); overlay.menu.Note("Online match: states, cheats, turbo and pause are off; the game runs on."); padMenu.clear(); } controls.clear(); continue; }
                    if (overlay.menu.IsOpen()) { if (unsigned bt = menuButton(c)) dispatch(overlay.press(bt)); continue; }
                    if (c == SDL_SCANCODE_EQUALS || c == SDL_SCANCODE_KP_PLUS || c == SDL_SCANCODE_MINUS || c == SDL_SCANCODE_KP_MINUS) {
                        const bool up = c == SDL_SCANCODE_EQUALS || c == SDL_SCANCODE_KP_PLUS; overlay.volume = std::clamp(overlay.volume + (up ? 5 : -5), 0, 100); overlay.muted = false;
                        audio.volume = overlay.volume / 100.0f; if (audio.stream) SDL_SetAudioStreamGain(audio.stream, audio.volume);
                    }
                    if (c == SDL_SCANCODE_F11) { fullscreen = !fullscreen; SDL_SetWindowFullscreen(w, fullscreen); }
                }
            }
            if (quit) break;

            // This PC's players: made-up (tests), nothing while a menu is open, or the window's controls
            lanes = {};
            if (autoPlay) {
                for (unsigned i = 0; i < o.locals; ++i) {
                    if (random() % 6 == 0) lanes[i] = uint16_t(random() & 63); else lanes[i] = uint16_t(lanes[i] & 63);
                    if ((shown + 40 + 20 * i) % 300 == 0) lanes[i] |= 0x100;
                }
            } else if (!overlay.menu.IsOpen() && !bar.IsOpen()) LocalLanes(controls, m.board, o.locals, lanes);
            const int seen = match.Tick(std::span<const uint16_t>(lanes.data(), o.locals));
            shown += uint64_t(seen);
            if (seen) audio.flush();
            if (frames && shown >= frames) quit = true;
            if (match.GetStatus() == recomp::match::SeatMatch::Status::Ended) {
                if (!endedAt) endedAt = SDL_GetTicksNS();
                if (autoPlay || SDL_GetTicksNS() - endedAt > 4000000000ull) quit = true;
            }

            // The picture, the menu over it, every seat's name, the match's state
            std::vector<uint8_t> menuRgb; const uint8_t *rgb = window_video_rgb(m.video.get());
            if (overlay.menu.IsOpen()) {
                std::vector<uint32_t> pixels(288 * 224);
                for (size_t i = 0; i < pixels.size(); ++i) pixels[i] = rgb[i * 3] | (uint32_t(rgb[i * 3 + 1]) << 8) | (uint32_t(rgb[i * 3 + 2]) << 16) | 0xff000000;
                overlay.menu.Draw(pixels.data(), 288, 224); menuRgb.resize(288 * 224 * 3);
                for (size_t i = 0; i < pixels.size(); ++i) { menuRgb[i * 3] = uint8_t(pixels[i]); menuRgb[i * 3 + 1] = uint8_t(pixels[i] >> 8); menuRgb[i * 3 + 2] = uint8_t(pixels[i] >> 16); }
                rgb = menuRgb.data();
            }
            sdl_require(SDL_UpdateTexture(texture, nullptr, rgb, 288 * 3), "texture update");
            SDL_SetRenderDrawColor(r, 12, 12, 16, 255); SDL_RenderClear(r);
            SDL_FRect game{0, 24, 576, 448}; SDL_RenderTexture(r, texture, nullptr, &game);
            const auto names = match.Names();
            for (unsigned seat = 0; seat < 4; ++seat) if (!names[seat].empty()) seated[seat] = names[seat]; // (for the replay)
            if (!overlay.menu.IsOpen()) DrawNames(r, names, o.capacity);
            DrawMessage(r, match.Message()); // (waiting, connecting, a change of players, the end)
            std::vector<uint32_t> barPixels(576 * 472, 0xff000000); bar.Draw(barPixels.data(), 576, 472);
            sdl_require(SDL_UpdateTexture(barTexture, nullptr, barPixels.data(), 576 * 4), "bar pixels");
            SDL_FRect strip{0, 0, 576, float(bar.Height())}; SDL_RenderTexture(r, barTexture, &strip, &strip);
            for (auto box : bar.Boxes()) { SDL_FRect rect{box.x, box.y, box.w, box.h}; SDL_RenderTexture(r, barTexture, &rect, &rect); }
            if (!capture.empty() && autoPlay && frames && shown + 1 >= frames && seen) {
                if (auto *shot = SDL_RenderReadPixels(r, nullptr)) { SDL_SaveBMP(shot, (capture + ".bmp").c_str()); SDL_DestroySurface(shot); }
            }
            // (a test: the menu open over the running match at frame 600, captured, closed: the match goes on)
            if (!capture.empty() && autoPlay && shown >= 600 && shown < 610 && !overlay.menu.IsOpen()) { overlay.open(); overlay.menu.Note("Online match: states, cheats, turbo and pause are off; the game runs on."); }
            if (!capture.empty() && autoPlay && shown >= 610 && overlay.menu.IsOpen()) {
                if (auto *shot = SDL_RenderReadPixels(r, nullptr)) { SDL_SaveBMP(shot, (capture + "-menu.bmp").c_str()); SDL_DestroySurface(shot); }
                overlay.menu.Close();
            }
            SDL_RenderPresent(r); ++presented;
            // (the board's 60 frames a second; a little slower while this PC is ahead of the others)
            nextFrame += uint64_t(16666667.0 * (match.FramesAhead() > 1.0f ? 1.016 : 1.0));
            const uint64_t now = SDL_GetTicksNS();
            if (nextFrame > now) SDL_DelayNS(nextFrame - now); else if (now - nextFrame > 100000000ull) nextFrame = now;
        }
        std::fprintf(stderr, "sunset-online: %llu frames, global frame %llu, epoch %llu, %s\n", static_cast<unsigned long long>(shown),
                     static_cast<unsigned long long>(match.Frame()), static_cast<unsigned long long>(match.Epoch()), match.Message().c_str());
        const bool failed = match.Failed();
        // The match's replay: every confirmed frame from the start (a minute at least), with its checksums
        if (std::vector<uint16_t> history; replays != "none" && match.ConfirmedHistory(history) && history.size() >= size_t(60) * o.capacity) {
            const uint64_t count = history.size() / o.capacity;
            record.info.identity = SUNSET_REPLAY_IDENTITY; record.info.rom = rom; record.info.version = SUNSET_VERSION;
            record.info.role = o.host ? "host" : "guest"; record.info.seats = o.capacity; record.info.frames = count;
            record.info.players.assign(seated.begin(), seated.begin() + o.capacity);
            SunsetReplay::Lanes last{0xffff, 0xffff, 0xffff, 0xffff};
            for (uint64_t f = 0; f < count; ++f) {
                SunsetReplay::Lanes lanes{}; for (unsigned p = 0; p < o.capacity; ++p) lanes[p] = history[size_t(f) * o.capacity + p];
                if (lanes != last) { record.inputs[f] = lanes; last = lanes; }
            }
            for (const auto &[frame, crc] : checks) if (frame <= count) record.checks.emplace_back(frame, crc);
            std::string names;
            for (const auto &n : record.info.players) { std::string clean; for (char c : n) if (std::isalnum(static_cast<unsigned char>(c))) clean += c; if (!clean.empty()) names += (names.empty() ? "" : "-") + clean; }
            std::error_code ec; std::filesystem::create_directories(replays, ec);
            const std::string path = (std::filesystem::path(replays) / (fileTime + "_ssriders_" + (names.empty() ? std::string("match") : names) + ".fareplay")).string();
            std::string why;
            if (record.Save(path, why)) std::fprintf(stderr, "replay: %s (%llu frames, %zu checksums)\n", path.c_str(), static_cast<unsigned long long>(count), record.checks.size());
            else std::fprintf(stderr, "replay: %s\n", why.c_str());
        }
        match.Stop();
        if (o.checkpointLog) std::fclose(o.checkpointLog);
        SDL_DestroyTexture(barTexture); SDL_DestroyTexture(texture); SDL_DestroyRenderer(r); SDL_DestroyWindow(w);
        return failed ? 1 : 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "sunset-online: %s\n", e.what());
        return 1;
    }
}
