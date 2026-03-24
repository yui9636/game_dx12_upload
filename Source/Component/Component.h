#pragma once

#include "Actor/Actor.h"
#include <SimpleMath.h>
#include "JSONManager.h"

class Component
{
public:
	Component() {}
	virtual ~Component() {}

	virtual void Serialize(json& outJson) const {}

	virtual void Deserialize(const json& inJson) {}

	virtual const char* GetName() const = 0;

	virtual void Start() {}

	virtual void Update(float dt) {}

	virtual void Render() {}

	virtual void OnGUI() {}

	virtual void OnDestroy() {}

	void SetActor(std::shared_ptr<Actor> actor) { this->actor = actor; }

	std::shared_ptr<Actor> GetActor() { return actor.lock(); }

	virtual std::shared_ptr<Component> Clone() { return nullptr; }

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
