#pragma once

#include "Actor/Actor.h"
#include <SimpleMath.h>
#include "JSONManager.h"

// コンポーネント
class Component
{
public:
	Component() {}
	virtual ~Component() {}

	virtual void Serialize(json& outJson) const {}

	virtual void Deserialize(const json& inJson) {}

	// 名前取得
	virtual const char* GetName() const = 0;

	// 開始処理
	virtual void Start() {}

	// 更新処理
	virtual void Update(float dt) {}

	virtual void Render() {}

	// GUI描画
	virtual void OnGUI() {}

	virtual void OnDestroy() {}

	// アクター設定
	void SetActor(std::shared_ptr<Actor> actor) { this->actor = actor; }

	// アクター取得
	std::shared_ptr<Actor> GetActor() { return actor.lock(); }

	virtual std::shared_ptr<Component> Clone() { return nullptr; }

	// 有効/無効
	void SetEnabled(bool v) { enabled = v; }
	bool IsEnabled() const { return enabled; }

protected:
	template<typename T>
	inline T Clamp(const T& v, const T& lo, const T& hi)
	{
		return (v < lo) ? lo : ((v > hi) ? hi : v);
	}

private:
	std::weak_ptr<Actor> actor;
	bool enabled = true;


};
