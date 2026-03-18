#pragma once


class IState {
public:
    virtual ~IState() = default;

    // ※将来的にデバッグ機能などを共通化する場合はここに追加します
    // virtual void SetDebugName(const char* name) = 0;
};

// =========================================================
// 各種ステート（共通基底を継承するだけの純粋な箱）
// =========================================================

class IInputLayout : public IState {
};

class IDepthStencilState : public IState {
};

class IRasterizerState : public IState {
};

class IBlendState : public IState {
};