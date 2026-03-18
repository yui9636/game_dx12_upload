//#include "Stage.h"
//#include "Graphics.h"
////#include "LevelLoader.h"       // ★追加: レベル読み込み用
//#include "Actor/Actor.h"
//#include "Collision/ColliderComponent.h"
//#include "Collision/Collision.h" // IntersectRayVsModel等がここにある前提
//#include "Model/Model.h"          // Modelクラスの定義（GetMeshes等を使うため）
//
//
//Stage* Stage::instance = nullptr;
//
//Stage& Stage::Instance()
//{
//	// シングルトンインスタンスがなければ生成（安全策）
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
//// レベル読み込み
//// -----------------------------------------------------------
//bool Stage::LoadLevel(const std::string& filename)
//{
//	// 既存のアクターをクリア
//	Clear();
//
//	// デバイスを取得
//	ID3D11Device* device = Graphics::Instance().GetDevice();
//
//	// ★修正: 第1引数に device を渡す
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
//// 更新
//// -----------------------------------------------------------
//void Stage::Update(float dt)
//{
//	//// 全アクターの更新
//	//ActorManager::Instance().Update(dt);
//
//	//// 行列の更新 (親子関係含む)
//	//ActorManager::Instance().UpdateTransform();
//}
//
//// -----------------------------------------------------------
//// 描画
//// -----------------------------------------------------------
//void Stage::Render(const RenderContext& rc, ModelRenderer* renderer)
//{
//	// ActorManagerに追加した Render 関数を使って一括描画
//	ActorManager::Instance().Render(rc, renderer);
//}
//
//// -----------------------------------------------------------
//// レイキャスト (当たり判定の橋渡し)
//// -----------------------------------------------------------
//bool Stage::RayCast(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, HitResult& hit)
//{
//	bool hasHit = false;
//	float minDistance = FLT_MAX;
//
//	// 全アクターを取得
//	const auto& actors = ActorManager::Instance().GetActors();
//
//	// ※Collision::IntersectRayVsSphere などの実装に合わせて調整が必要ですが、
//	// 基本的なループ構造は以下のようになります。
//
//	for (const auto& actor : actors)
//	{
//		// コライダーコンポーネントを持っているか？
//		auto collider = actor->GetComponent<ColliderComponent>();
//		if (!collider || !collider->IsEnabled()) continue;
//
//		// アクターごとの判定
//		// (ColliderComponent側に RayCast 関数を実装するか、
//		//  ここで形状データを取り出して Collision::Intersect... を呼ぶ)
//
//		// 例: ColliderComponent 内の形状リストを回して判定
//		// ここでは簡易的に「モデルのバウンディングボックス」で判定する例を書きます
//		// 本来は collider->RayCast(...) を実装して呼ぶのがベストです。
//
//		/* // --- 簡易実装例 (ModelのBoundsを使用) ---
//		Model* model = actor->GetModelRaw();
//		if (model)
//		{
//			HitResult tempHit;
//			// ワールド行列を加味して判定
//			if (Collision::IntersectRayVsModel(start, end, model, actor->GetTransform(), tempHit))
//			{
//				if (tempHit.distance < minDistance)
//				{
//					minDistance = tempHit.distance;
//					hit = tempHit;
//					hit.actor = actor.get(); // ヒットしたアクターを記録しておくと便利
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
