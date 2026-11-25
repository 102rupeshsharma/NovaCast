#include "sdl_window.h"

#include "common/log.h"

#include <SDL.h>

#include <map>
#include <memory>

namespace nc::app {

namespace {

// RAII owner of SDL's global state.
class SdlSession {
public:
    explicit SdlSession(Uint32 flags) : ok_(SDL_Init(flags) == 0) {}
    ~SdlSession() {
        if (ok_)
            SDL_Quit();
    }
    SdlSession(const SdlSession&) = delete;
    SdlSession& operator=(const SdlSession&) = delete;

    bool ok() const { return ok_; }

private:
    bool ok_;
};

struct WindowDeleter {
    void operator()(SDL_Window* w) const { SDL_DestroyWindow(w); }
};
struct RendererDeleter {
    void operator()(SDL_Renderer* r) const { SDL_DestroyRenderer(r); }
};
struct ControllerDeleter {
    void operator()(SDL_GameController* c) const { SDL_GameControllerClose(c); }
};

using WindowPtr = std::unique_ptr<SDL_Window, WindowDeleter>;
using RendererPtr = std::unique_ptr<SDL_Renderer, RendererDeleter>;
using ControllerPtr = std::unique_ptr<SDL_GameController, ControllerDeleter>;

Error sdl_error(const char* what) {
    return make_error(ErrorCode::InitFailed, std::string(what) + ": " + SDL_GetError());
}

void open_controller(int device_index, std::map<SDL_JoystickID, ControllerPtr>& controllers) {
    if (!SDL_IsGameController(device_index))
        return;
    ControllerPtr controller(SDL_GameControllerOpen(device_index));
    if (!controller) {
        LOG_WARN("input", "cannot open controller %d: %s", device_index, SDL_GetError());
        return;
    }
    SDL_Joystick* joystick = SDL_GameControllerGetJoystick(controller.get());
    const SDL_JoystickID id = SDL_JoystickInstanceID(joystick);
    const char* name = SDL_GameControllerName(controller.get());
    LOG_INFO("input", "controller connected: %s (id %d)", name ? name : "unknown", static_cast<int>(id));
    controllers[id] = std::move(controller);
}

} // namespace

Result<WindowRunStats> run_window(const WindowOptions& options) {
    SdlSession sdl(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS);
    if (!sdl.ok())
        return sdl_error("SDL_Init failed");

    SDL_version linked;
    SDL_GetVersion(&linked);
    LOG_INFO("app", "SDL %d.%d.%d, video driver '%s'", linked.major, linked.minor, linked.patch,
             SDL_GetCurrentVideoDriver());

    Uint32 window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    window_flags |= options.hidden ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN;
    WindowPtr window(SDL_CreateWindow(options.title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                      options.width, options.height, window_flags));
    if (!window)
        return sdl_error("SDL_CreateWindow failed");

    Uint32 renderer_flags = SDL_RENDERER_ACCELERATED;
    if (options.vsync)
        renderer_flags |= SDL_RENDERER_PRESENTVSYNC;
    RendererPtr renderer(SDL_CreateRenderer(window.get(), -1, renderer_flags));
    if (!renderer) {
        LOG_WARN("app", "accelerated renderer unavailable (%s); using software", SDL_GetError());
        renderer.reset(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_SOFTWARE));
        if (!renderer)
            return sdl_error("SDL_CreateRenderer failed");
    }

    WindowRunStats stats;
    SDL_RendererInfo info{};
    if (SDL_GetRendererInfo(renderer.get(), &info) == 0 && info.name)
        stats.renderer_name = info.name;
    LOG_INFO("app", "window renderer: %s", stats.renderer_name.c_str());

    // Controllers present at start-up arrive as SDL_CONTROLLERDEVICEADDED events.
    std::map<SDL_JoystickID, ControllerPtr> controllers;

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE)
                    running = false;
                break;
            case SDL_CONTROLLERDEVICEADDED:
                open_controller(event.cdevice.which, controllers);
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                if (controllers.erase(event.cdevice.which) != 0)
                    LOG_INFO("input", "controller disconnected (id %d)", static_cast<int>(event.cdevice.which));
                break;
            default:
                break;
            }
        }

        SDL_SetRenderDrawColor(renderer.get(), 18, 20, 28, 255);
        SDL_RenderClear(renderer.get());
        SDL_RenderPresent(renderer.get());
        ++stats.frames;

        if (options.max_frames && stats.frames >= *options.max_frames)
            running = false;
        if (!options.vsync)
            SDL_Delay(1);
    }

    // Close controllers before SDL_Quit runs in ~SdlSession.
    controllers.clear();
    return stats;
}

} // namespace nc::app
