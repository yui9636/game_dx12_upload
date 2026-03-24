//#include "Stage.h"
//#include "Graphics.h"
//#include "Actor/Actor.h"
//#include "Collision/ColliderComponent.h"
//
//
//Stage* Stage::instance = nullptr;
//
//Stage& Stage::Instance()
//{
//	if (!instance) instance = new Stage();
//	return *instance;
//}
//
//Stage::Stage()
//{
//	instance = this;
//}
//
//Stage::~Stage()
//{
//	Clear();
//}
//
//// -----------------------------------------------------------
//// -----------------------------------------------------------
//bool Stage::LoadLevel(const std::string& filename)
//{
//	Clear();
//
//	ID3D11Device* device = Graphics::Instance().GetDevice();
//
//	//bool result = LevelLoader::LoadLevel(device, filename, false);
//
//	//return result;
//}
//
//void Stage::Clear()
//{
//	ActorManager::Instance().Clear();
//}
//
//// -----------------------------------------------------------
//// -----------------------------------------------------------
//void Stage::Update(float dt)
//{
//	//ActorManager::Instance().Update(dt);
//
//	//ActorManager::Instance().UpdateTransform();
//}
//
//// -----------------------------------------------------------
//// -----------------------------------------------------------
//void Stage::Render(const RenderContext& rc, ModelRenderer* renderer)
//{
//	ActorManager::Instance().Render(rc, renderer);
//}
//
//// -----------------------------------------------------------
//// -----------------------------------------------------------
//bool Stage::RayCast(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, HitResult& hit)
//{
//	bool hasHit = false;
//	float minDistance = FLT_MAX;
//
//	const auto& actors = ActorManager::Instance().GetActors();
//
//
//	for (const auto& actor : actors)
//	{
//		auto collider = actor->GetComponent<ColliderComponent>();
//		if (!collider || !collider->IsEnabled()) continue;
//
//
//
//		Model* model = actor->GetModelRaw();
//		if (model)
//		{
//			HitResult tempHit;
//			if (Collision::IntersectRayVsModel(start, end, model, actor->GetTransform(), tempHit))
//			{
//				if (tempHit.distance < minDistance)
//				{
//					minDistance = tempHit.distance;
//					hit = tempHit;
//					hasHit = true;
//				}
//			}
//		}
//		*/
//	}
//
//	return hasHit;
//}
//
