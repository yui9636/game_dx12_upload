#pragma once


// RHI が保持するステートオブジェクトの共通基底。
class IState {
public:
    virtual ~IState() = default;
};
// 頂点入力レイアウトの RHI 共通ハンドル。
class IInputLayout : public IState {
};

// 深度ステンシル設定の RHI 共通ハンドル。
class IDepthStencilState : public IState {
};

// ラスタライザ設定の RHI 共通ハンドル。
class IRasterizerState : public IState {
};

// ブレンド設定の RHI 共通ハンドル。
class IBlendState : public IState {
};
