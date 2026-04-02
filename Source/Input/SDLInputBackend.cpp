#include "SDLInputBackend.h"
#include "Console/Logger.h"
#include <SDL3/SDL.h>
#include <cstring>

SDLInputBackend::~SDLInputBackend() {
    Shutdown();
}

bool SDLInputBackend::Initialize(void* hwnd) {
    if (m_initialized) return true;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
        LOG_ERROR("[SDLInputBackend] SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, hwnd);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_EXTERNAL_GRAPHICS_CONTEXT_BOOLEAN, true);
    m_sdlWindow = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);

    if (!m_sdlWindow) {
        LOG_ERROR("[SDLInputBackend] SDL_CreateWindowWithProperties failed: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    m_initialized = true;
    LOG_INFO("[SDLInputBackend] Initialized.");
    return true;
}

void SDLInputBackend::Shutdown() {
    if (!m_initialized) return;

    for (auto& [id, pad] : m_gamepads) {
        if (pad) SDL_CloseGamepad(pad);
    }
    m_gamepads.clear();

    if (m_sdlWindow) {
        SDL_DestroyWindow(m_sdlWindow);
        m_sdlWindow = nullptr;
    }

    SDL_Quit();
    m_initialized = false;
}

void SDLInputBackend::BeginFrame() {
    // Nothing needed before polling
}

void SDLInputBackend::PollEvents(InputEventQueue& queue) {
    SDL_Event sdlEvent;
    while (SDL_PollEvent(&sdlEvent)) {
        InputEvent ev;
        ev.timestamp = sdlEvent.common.timestamp;

        switch (sdlEvent.type) {
        case SDL_EVENT_KEY_DOWN:
            ev.type = InputEventType::KeyDown;
            ev.key.scancode = static_cast<uint32_t>(sdlEvent.key.scancode);
            ev.key.keycode = static_cast<uint32_t>(sdlEvent.key.key);
            ev.key.repeat = sdlEvent.key.repeat;
            queue.Push(ev);
            break;

        case SDL_EVENT_KEY_UP:
            ev.type = InputEventType::KeyUp;
            ev.key.scancode = static_cast<uint32_t>(sdlEvent.key.scancode);
            ev.key.keycode = static_cast<uint32_t>(sdlEvent.key.key);
            ev.key.repeat = false;
            queue.Push(ev);
            break;

        case SDL_EVENT_MOUSE_MOTION:
            ev.type = InputEventType::MouseMove;
            ev.mouseMove.x = sdlEvent.motion.x;
            ev.mouseMove.y = sdlEvent.motion.y;
            ev.mouseMove.dx = sdlEvent.motion.xrel;
            ev.mouseMove.dy = sdlEvent.motion.yrel;
            queue.Push(ev);
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            ev.type = InputEventType::MouseButtonDown;
            ev.mouseButton.button = sdlEvent.button.button;
            ev.mouseButton.x = sdlEvent.button.x;
            ev.mouseButton.y = sdlEvent.button.y;
            queue.Push(ev);
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            ev.type = InputEventType::MouseButtonUp;
            ev.mouseButton.button = sdlEvent.button.button;
            ev.mouseButton.x = sdlEvent.button.x;
            ev.mouseButton.y = sdlEvent.button.y;
            queue.Push(ev);
            break;

        case SDL_EVENT_MOUSE_WHEEL:
            ev.type = InputEventType::MouseWheel;
            ev.mouseWheel.scrollX = sdlEvent.wheel.x;
            ev.mouseWheel.scrollY = sdlEvent.wheel.y;
            queue.Push(ev);
            break;

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            ev.type = InputEventType::GamepadButtonDown;
            ev.deviceId = sdlEvent.gbutton.which;
            ev.gamepadButton.button = sdlEvent.gbutton.button;
            queue.Push(ev);
            break;

        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            ev.type = InputEventType::GamepadButtonUp;
            ev.deviceId = sdlEvent.gbutton.which;
            ev.gamepadButton.button = sdlEvent.gbutton.button;
            queue.Push(ev);
            break;

        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            ev.type = InputEventType::GamepadAxis;
            ev.deviceId = sdlEvent.gaxis.which;
            ev.gamepadAxis.axis = static_cast<uint8_t>(sdlEvent.gaxis.axis);
            ev.gamepadAxis.value = sdlEvent.gaxis.value / 32767.0f;
            queue.Push(ev);
            break;

        case SDL_EVENT_GAMEPAD_ADDED: {
            ev.type = InputEventType::DeviceAdded;
            ev.device.deviceId = sdlEvent.gdevice.which;
            ev.device.type = InputDeviceType::Gamepad;
            SDL_Gamepad* pad = SDL_OpenGamepad(sdlEvent.gdevice.which);
            if (pad) {
                m_gamepads[sdlEvent.gdevice.which] = pad;
                LOG_INFO("[SDLInputBackend] Gamepad added: %s", SDL_GetGamepadName(pad));
            }
            queue.Push(ev);
            break;
        }

        case SDL_EVENT_GAMEPAD_REMOVED: {
            ev.type = InputEventType::DeviceRemoved;
            ev.device.deviceId = sdlEvent.gdevice.which;
            ev.device.type = InputDeviceType::Gamepad;
            auto it = m_gamepads.find(sdlEvent.gdevice.which);
            if (it != m_gamepads.end()) {
                SDL_CloseGamepad(it->second);
                m_gamepads.erase(it);
            }
            queue.Push(ev);
            break;
        }

        case SDL_EVENT_TEXT_INPUT:
            ev.type = InputEventType::TextInput;
            strncpy(ev.textInput.text, sdlEvent.text.text, sizeof(ev.textInput.text) - 1);
            ev.textInput.text[sizeof(ev.textInput.text) - 1] = '\0';
            queue.Push(ev);
            break;

        case SDL_EVENT_TEXT_EDITING:
            ev.type = InputEventType::TextComposition;
            strncpy(ev.textComposition.text, sdlEvent.edit.text, sizeof(ev.textComposition.text) - 1);
            ev.textComposition.text[sizeof(ev.textComposition.text) - 1] = '\0';
            ev.textComposition.cursor = sdlEvent.edit.start;
            ev.textComposition.selectionLen = sdlEvent.edit.length;
            queue.Push(ev);
            break;

        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            ev.type = InputEventType::WindowFocusGained;
            queue.Push(ev);
            break;

        case SDL_EVENT_WINDOW_FOCUS_LOST:
            ev.type = InputEventType::WindowFocusLost;
            queue.Push(ev);
            break;

        default:
            break;
        }
    }
}

void SDLInputBackend::EndFrame() {
    // Nothing needed after polling
}

std::vector<InputDeviceInfo> SDLInputBackend::GetConnectedDevices() const {
    std::vector<InputDeviceInfo> devices;

    // Always report keyboard and mouse
    {
        InputDeviceInfo kb;
        kb.deviceId = 0;
        kb.type = InputDeviceType::Keyboard;
        strncpy(kb.name, "Keyboard", sizeof(kb.name));
        kb.connected = true;
        devices.push_back(kb);
    }
    {
        InputDeviceInfo mouse;
        mouse.deviceId = 1;
        mouse.type = InputDeviceType::Mouse;
        strncpy(mouse.name, "Mouse", sizeof(mouse.name));
        mouse.connected = true;
        devices.push_back(mouse);
    }

    for (auto& [id, pad] : m_gamepads) {
        InputDeviceInfo info;
        info.deviceId = id;
        info.type = InputDeviceType::Gamepad;
        const char* name = pad ? SDL_GetGamepadName(pad) : "Unknown Gamepad";
        strncpy(info.name, name ? name : "Gamepad", sizeof(info.name) - 1);
        info.connected = true;
        devices.push_back(info);
    }

    return devices;
}

void SDLInputBackend::SetVibration(uint32_t deviceId, float left, float right) {
    auto it = m_gamepads.find(deviceId);
    if (it != m_gamepads.end() && it->second) {
        uint16_t lo = static_cast<uint16_t>(left * 65535.0f);
        uint16_t hi = static_cast<uint16_t>(right * 65535.0f);
        SDL_RumbleGamepad(it->second, lo, hi, 100);
    }
}

void SDLInputBackend::StopVibration(uint32_t deviceId) {
    SetVibration(deviceId, 0.0f, 0.0f);
}

void SDLInputBackend::StartTextInput() {
    if (!m_textInputActive && m_sdlWindow) {
        SDL_StartTextInput(m_sdlWindow);
        m_textInputActive = true;
    }
}

void SDLInputBackend::StopTextInput() {
    if (m_textInputActive && m_sdlWindow) {
        SDL_StopTextInput(m_sdlWindow);
        m_textInputActive = false;
    }
}

bool SDLInputBackend::IsTextInputActive() const {
    return m_textInputActive;
}
