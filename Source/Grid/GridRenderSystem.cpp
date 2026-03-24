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

        pr->DrawGrid(grid.subdivisions, grid.scale);
        hasAnyGrid = true;
        });

    if (hasAnyGrid) {
        pr->Render(
            rc.commandList->GetNativeContext(),
            rc.viewMatrix,
            rc.projectionMatrix,
            D3D11_PRIMITIVE_TOPOLOGY_LINELIST
        );
    }
}
