#pragma once

#include"RenderContext/RenderContext.h"
#include "Model/ModelResource.h"

class Shader
{
public:
	Shader() {}
	virtual ~Shader() {};

	//描画開始
	virtual void Begin(const RenderContext& rc) = 0;

	////モデル描画
	//virtual void Draw(const RenderContext& rc, const ModelResource* modelResource) = 0;

	// 個々のメッシュごとに呼ばれる（DrawIndexed は呼ばない！）
	virtual void Update(const RenderContext& rc, const ModelResource::MeshResource& mesh) = 0;

	//描画終了
	virtual void End(const RenderContext& rc) = 0;
};

class EffectShader
{
public:
	EffectShader() {}
	virtual ~EffectShader() {};

	//描画開始
	virtual void Begin(const RenderContext& rc) = 0;

	////モデル描画
	virtual void Draw(const RenderContext& rc, const ModelResource* modelResource) = 0;

	//描画終了
	virtual void End(const RenderContext& rc) = 0;
};


class SpriteShader
{
public:
	SpriteShader() {}
	virtual ~SpriteShader() {}

	// 描画開始
	virtual void Begin(const RenderContext& rc) = 0;

	// 描画
	virtual void Draw(const RenderContext& rc, const Sprite* sprite) = 0;

	// 描画終了
	virtual void End(const RenderContext& rc) = 0;
};
