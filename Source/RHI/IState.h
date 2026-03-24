#pragma once


class IState {
public:
    virtual ~IState() = default;

    // virtual void SetDebugName(const char* name) = 0;
};

// =========================================================
// =========================================================

class IInputLayout : public IState {
};

class IDepthStencilState : public IState {
};

class IRasterizerState : public IState {
};

class IBlendState : public IState {
};
