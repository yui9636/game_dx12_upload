#include "UIManager.h"
#include "RenderContext/RenderContext.h"
#include "RHI/ICommandList.h"

// 指定 UI 要素を管理リストから取り除く。
void UIManager::RemoveElement(std::shared_ptr<UIElement> element)
{
    auto it = std::remove(elements.begin(), elements.end(), element);
    if (it != elements.end())
    {
        elements.erase(it, elements.end());
    }

}

void UIManager::Clear()
{
    elements.clear();
}

// 登録済み UI 要素をすべて更新する。
void UIManager::Update(float dt)
{
    for (auto& e : elements)
    {
        e->Update(dt);
    }
}

// アクティブな UI 要素だけを現在の RenderContext で描画する。
void UIManager::Render(const RenderContext& rc)
{
    if (elements.empty()) return;

    const RenderState* rs = rc.renderState;

    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

   

    for (auto& e : elements)
    {
        if (e->IsActive())
        {
            e->Render(rc);
        }
    }


}
