#pragma once
#include "InputEvent.h"
#include "InputEventQueue.h"
#include <vector>

class IInputBackend {
public:
    virtual ~IInputBackend() = default;

    virtual bool Initialize(void* hwnd) = 0;
    virtual void Shutdown() = 0;

    virtual void BeginFrame() = 0;
    virtual void PollEvents(InputEventQueue& queue) = 0;
    virtual void EndFrame() = 0;

    virtual std::vector<InputDeviceInfo> GetConnectedDevices() const = 0;

    virtual void SetVibration(uint32_t deviceId, float left, float right) = 0;
    virtual void StopVibration(uint32_t deviceId) = 0;

    virtual void StartTextInput() = 0;
    virtual void StopTextInput() = 0;
    virtual bool IsTextInputActive() const = 0;
};
