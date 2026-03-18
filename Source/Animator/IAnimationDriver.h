#pragma once

class IAnimationDriver
{
public:
    virtual ~IAnimationDriver() = default;

    virtual float GetTime() const = 0;

    virtual bool AllowInternalUpdate() const = 0;
    virtual int GetOverrideAnimationIndex() const = 0;

    virtual bool IsLoop() const = 0;
};