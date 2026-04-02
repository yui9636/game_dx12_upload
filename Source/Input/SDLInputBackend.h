#pragma once
#include "IInputBackend.h"
#include <unordered_map>

struct SDL_Window;
struct SDL_Gamepad;

class SDLInputBackend : public IInputBackend {
public:
    SDLInputBackend() = default;
    ~SDLInputBackend() override;

    bool Initialize(void* hwnd) override;
    void Shutdown() override;

    void BeginFrame() override;
    void PollEvents(InputEventQueue& queue) override;
    void EndFrame() override;

    std::vector<InputDeviceInfo> GetConnectedDevices() const override;

    void SetVibration(uint32_t deviceId, float left, float right) override;
    void StopVibration(uint32_t deviceId) override;

    void StartTextInput() override;
    void StopTextInput() override;
    bool IsTextInputActive() const override;

private:
    SDL_Window* m_sdlWindow = nullptr;
    std::unordered_map<uint32_t, SDL_Gamepad*> m_gamepads;
    bool m_textInputActive = false;
    bool m_initialized = false;
};
