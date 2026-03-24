#pragma once

#include"RenderContext/RenderContext.h"
#include "Model/ModelResource.h"

class Shader
{
public:
	Shader() {}
	virtual ~Shader() {};

	virtual void Begin(const RenderContext& rc) = 0;

	//virtual void Draw(const RenderContext& rc, const ModelResource* modelResource) = 0;

	virtual void Update(const RenderContext& rc, const ModelResource::MeshResource& mesh) = 0;

	virtual void BeginInstanced(const RenderContext& rc) { Begin(rc); }
	virtual bool SupportsInstancing(const ModelResource::MeshResource& mesh) const
	{
		(void)mesh;
		return false;
	}

	virtual void End(const RenderContext& rc) = 0;
};

class EffectShader
{
public:
	EffectShader() {}
	virtual ~EffectShader() {};

	virtual void Begin(const RenderContext& rc) = 0;

	virtual void Draw(const RenderContext& rc, const ModelResource* modelResource) = 0;

	virtual void End(const RenderContext& rc) = 0;
};


class SpriteShader
{
public:
	SpriteShader() {}
	virtual ~SpriteShader() {}

	virtual void Begin(const RenderContext& rc) = 0;

	virtual void Draw(const RenderContext& rc, const Sprite* sprite) = 0;

	virtual void End(const RenderContext& rc) = 0;
};
