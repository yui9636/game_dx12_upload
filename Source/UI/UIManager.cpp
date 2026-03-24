#include "UIManager.h"
#include "RenderContext/RenderContext.h"
#include "RHI/ICommandList.h"

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

void UIManager::Update(float dt)
{
    for (auto& e : elements)
    {
        e->Update(dt);
    }
}

void UIManager::Render(const RenderContext& rc)
{
    if (elements.empty()) return;

    const RenderState* rs = rc.renderState;

    // --------------------------------------------------------
    // --------------------------------------------------------

    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    //dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), blendFactor, 0xFFFFFFFF);
    //dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
    //dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullNone));

   

    for (auto& e : elements)
    {
        if (e->IsActive())
        {
            e->Render(rc);
        }
    }


}
