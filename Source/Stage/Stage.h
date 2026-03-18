//#pragma once
//
//#include <string>
//#include <DirectXMath.h>
//#include <memory>
//#include <vector> 
//
//
//
//// 前方宣言
//struct RenderContext;
//class ModelRenderer;
//struct HitResult; // レイキャスト結果用
//class Stage
//{
//public:
//	static Stage& Instance();
//
//	Stage();
//	~Stage();
//
//	// ★変更: JSONレベルデータの読み込み
//	// 内部で LevelLoader::LoadLevel を呼びます
//	bool LoadLevel(const std::string& filename);
//
//	// ステージの破棄
//	void Clear();
//
//	// 更新処理 (ActorManagerの更新を呼ぶ)
//	void Update(float dt);
//
//	// 描画処理 (ActorManagerの描画を呼ぶ)
//	void Render(const RenderContext& rc, ModelRenderer* renderer);
//
//	// レイキャスト (全アクターのコライダーと判定)
//	bool RayCast(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, HitResult& hit);
//
//private:
//	static Stage* instance;
//};