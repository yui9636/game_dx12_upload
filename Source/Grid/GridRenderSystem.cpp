#include "GridRenderSystem.h"
#include "Component/GridComponent.h"
#include "Component/TransformComponent.h"
#include "Graphics.h"
#include "PrimitiveRenderer.h"
#include <System\Query.h>
#include "RHI/ICommandList.h"

void GridRenderSystem::Render(Registry& registry, RenderContext& rc) {
    auto pr = Graphics::Instance().GetPrimitiveRenderer();
    if (!pr) return;

    bool hasAnyGrid = false;
    Query<GridComponent, TransformComponent> query(registry);

    query.ForEach([&](GridComponent& grid, TransformComponent& trans) {
        if (!grid.enabled) return;

        // –{—ˆ‚Í PrimitiveRenderer::DrawGrid “à‚Å white ŒÅ’è‚Å‚·‚ª
        // ‚±‚±‚Å grid.color ‚ð”½‰f‚³‚¹‚é‚æ‚¤‚ÉŠg’£‚·‚é‚±‚Æ‚à‰Â”\‚Å‚·B
        // Œ»ó‚Ì PrimitiveRenderer ‚ð‚»‚Ì‚Ü‚ÜŽg‚¤ê‡‚ÍˆÈ‰º‚Ì’Ê‚èF
        pr->DrawGrid(grid.subdivisions, grid.scale);
        hasAnyGrid = true;
        });

    if (hasAnyGrid) {
        // •`‰æŽÀs
        pr->Render(
            rc.commandList->GetNativeContext(),
            rc.viewMatrix,
            rc.projectionMatrix,
            D3D11_PRIMITIVE_TOPOLOGY_LINELIST
        );
    }
}