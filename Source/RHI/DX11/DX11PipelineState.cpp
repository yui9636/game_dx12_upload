#include "DX11PipelineState.h"

DX11PipelineState::DX11PipelineState(const PipelineStateDesc& desc)
    : m_desc(desc)
{
    // DX11においては、DX12のような「PSOの事前コンパイル」という概念が存在しないため、
    // ここでは渡された記述子(Desc)をそのままコピーして保持するだけにとどめます。

    // 【将来の拡張ポイント】
    // もしエンジン側で IShader や IState の寿命管理を厳密に行う（参照カウンタ方式など）場合、
    // ここで desc 内の各ポインタに対して AddRef() 的な処理を行う必要があります。
}

DX11PipelineState::~DX11PipelineState()
{
    // コンストラクタで AddRef() を行った場合は、ここで Release() を行います。
    // 現在は生ポインタを保持しているだけなので、特別な破棄処理は不要です。
}

const PipelineStateDesc& DX11PipelineState::GetDesc() const
{
    return m_desc;
}