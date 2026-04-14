#include "EditorLayerInternal.h"

void EditorLayer::DrawSceneGridOverlay(const DirectX::XMFLOAT4& viewRect,
                                       const DirectX::XMFLOAT4X4& view,
                                       const DirectX::XMFLOAT4X4& projection) const
{
    if (viewRect.z <= 1.0f || viewRect.w <= 1.0f) {
        return;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (m_sceneViewMode == SceneViewMode::Mode2D) {
        const float aspect = viewRect.z / (std::max)(viewRect.w, 1.0f);
        const float worldHalfHeight = m_editor2DZoom;
        const float worldHalfWidth = m_editor2DZoom * aspect;
        const float left = m_editor2DCenter.x - worldHalfWidth;
        const float right = m_editor2DCenter.x + worldHalfWidth;
        const float bottom = m_editor2DCenter.y - worldHalfHeight;
        const float top = m_editor2DCenter.y + worldHalfHeight;

        const float approxStep = (std::max)(1.0f, m_editor2DZoom / 4.0f);
        const float stepBase = std::pow(10.0f, std::floor(std::log10(approxStep)));
        float step = stepBase;
        if (approxStep / stepBase >= 5.0f) step = stepBase * 5.0f;
        else if (approxStep / stepBase >= 2.0f) step = stepBase * 2.0f;

        auto worldToScreen = [&](float x, float y) {
            const float sx = viewRect.x + ((x - left) / (std::max)(right - left, 0.001f)) * viewRect.z;
            const float sy = viewRect.y + (1.0f - ((y - bottom) / (std::max)(top - bottom, 0.001f))) * viewRect.w;
            return ImVec2(sx, sy);
        };

        const ImU32 minorColor = IM_COL32(90, 98, 112, 110);
        const ImU32 axisColor = IM_COL32(140, 170, 220, 180);
        const float startX = std::floor(left / step) * step;
        const float startY = std::floor(bottom / step) * step;
        for (float x = startX; x <= right; x += step) {
            const ImU32 color = std::fabs(x) < 0.001f ? axisColor : minorColor;
            drawList->AddLine(worldToScreen(x, bottom), worldToScreen(x, top), color, std::fabs(x) < 0.001f ? 2.0f : 1.0f);
        }
        for (float y = startY; y <= top; y += step) {
            const ImU32 color = std::fabs(y) < 0.001f ? axisColor : minorColor;
            drawList->AddLine(worldToScreen(left, y), worldToScreen(right, y), color, std::fabs(y) < 0.001f ? 2.0f : 1.0f);
        }
        return;
    }

    const float step = 10.0f;
    const ImU32 minorColor = IM_COL32(90, 98, 112, 96);
    const ImU32 axisXColor = IM_COL32(210, 80, 80, 180);
    const ImU32 axisZColor = IM_COL32(80, 160, 220, 180);
    float minX = 0.0f;
    float maxX = 0.0f;
    float minZ = 0.0f;
    float maxZ = 0.0f;
    bool hasPlaneCoverage = false;
    const std::array<ImVec2, 5> samplePoints = {
        ImVec2(viewRect.x, viewRect.y),
        ImVec2(viewRect.x + viewRect.z, viewRect.y),
        ImVec2(viewRect.x + viewRect.z, viewRect.y + viewRect.w),
        ImVec2(viewRect.x, viewRect.y + viewRect.w),
        ImVec2(viewRect.x + viewRect.z * 0.5f, viewRect.y + viewRect.w * 0.5f)
    };

    for (const ImVec2& samplePoint : samplePoints) {
        DirectX::XMFLOAT3 rayOrigin{};
        DirectX::XMFLOAT3 rayDirection{};
        DirectX::XMFLOAT3 planePoint{};
        if (!BuildWorldRay(viewRect, view, projection, samplePoint, rayOrigin, rayDirection) ||
            !IntersectRayWithGroundPlane(rayOrigin, rayDirection, planePoint)) {
            continue;
        }

        if (!hasPlaneCoverage) {
            minX = maxX = planePoint.x;
            minZ = maxZ = planePoint.z;
            hasPlaneCoverage = true;
        } else {
            minX = (std::min)(minX, planePoint.x);
            maxX = (std::max)(maxX, planePoint.x);
            minZ = (std::min)(minZ, planePoint.z);
            maxZ = (std::max)(maxZ, planePoint.z);
        }
    }

    if (!hasPlaneCoverage) {
        const float fallbackExtent = (std::max)(200.0f, std::fabs(m_editorCameraPosition.y) * 8.0f);
        minX = -fallbackExtent;
        maxX = fallbackExtent;
        minZ = -fallbackExtent;
        maxZ = fallbackExtent;
    } else {
        const float margin = step * 4.0f;
        minX -= margin;
        maxX += margin;
        minZ -= margin;
        maxZ += margin;
        const float minExtent = 120.0f;
        minX = (std::min)(minX, -minExtent);
        maxX = (std::max)(maxX, minExtent);
        minZ = (std::min)(minZ, -minExtent);
        maxZ = (std::max)(maxZ, minExtent);
    }

    minX = std::floor(minX / step) * step;
    maxX = std::ceil(maxX / step) * step;
    minZ = std::floor(minZ / step) * step;
    maxZ = std::ceil(maxZ / step) * step;

    Registry& registry = m_gameLayer->GetRegistry();
    std::vector<GridOccluder> occluders;
    occluders.reserve(32);
    for (Archetype* archetype : registry.GetAllArchetypes()) {
        const auto& signature = archetype->GetSignature();
        if (!signature.test(TypeManager::GetComponentTypeID<MeshComponent>()) ||
            !signature.test(TypeManager::GetComponentTypeID<TransformComponent>())) {
            continue;
        }

        auto* meshColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<MeshComponent>());
        auto* transformColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
        for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
            const auto* mesh = static_cast<MeshComponent*>(meshColumn->Get(i));
            const auto* transform = static_cast<TransformComponent*>(transformColumn->Get(i));
            if (!mesh || !mesh->model || !mesh->isVisible || !transform) {
                continue;
            }

            DirectX::BoundingBox bounds{};
            if (!TryGetLiveMeshWorldBounds(*mesh, transform, bounds)) {
                continue;
            }

            DirectX::XMFLOAT3 corners[8]{};
            BuildBoundingBoxCorners(bounds, corners);

            ImVec2 minCorner(viewRect.x + viewRect.z, viewRect.y + viewRect.w);
            ImVec2 maxCorner(viewRect.x, viewRect.y);
            float minDepth = 1.0f;
            bool anyProjected = false;
            for (const DirectX::XMFLOAT3& corner : corners) {
                ImVec2 screen{};
                float depth = 1.0f;
                if (!ProjectWorldToSceneScreenDepth(viewRect, view, projection, corner, screen, depth)) {
                    continue;
                }
                anyProjected = true;
                minCorner.x = (std::min)(minCorner.x, screen.x);
                minCorner.y = (std::min)(minCorner.y, screen.y);
                maxCorner.x = (std::max)(maxCorner.x, screen.x);
                maxCorner.y = (std::max)(maxCorner.y, screen.y);
                minDepth = (std::min)(minDepth, depth);
            }

            if (!anyProjected) {
                continue;
            }

            minCorner.x = (std::max)(minCorner.x, viewRect.x);
            minCorner.y = (std::max)(minCorner.y, viewRect.y);
            maxCorner.x = (std::min)(maxCorner.x, viewRect.x + viewRect.z);
            maxCorner.y = (std::min)(maxCorner.y, viewRect.y + viewRect.w);
            if (maxCorner.x <= minCorner.x || maxCorner.y <= minCorner.y) {
                continue;
            }

            occluders.push_back(GridOccluder{ minCorner, maxCorner, minDepth });
        }
    }

    auto drawWorldSegment = [&](const DirectX::XMFLOAT3& a,
                                const DirectX::XMFLOAT3& b,
                                const DirectX::XMFLOAT3& samplePoint,
                                ImU32 color,
                                float thickness) {
        ImVec2 screenA{};
        ImVec2 screenB{};
        ImVec2 sampleScreen{};
        float sampleDepth = 1.0f;
        if (!ProjectWorldToSceneScreen(viewRect, view, projection, a, screenA) ||
            !ProjectWorldToSceneScreen(viewRect, view, projection, b, screenB) ||
            !ProjectWorldToSceneScreenDepth(viewRect, view, projection, samplePoint, sampleScreen, sampleDepth)) {
            return;
        }

        if (IsPointInsideOccluder(sampleScreen, sampleDepth, occluders)) {
            return;
        }

        drawList->AddLine(screenA, screenB, color, thickness);
    };

    for (float x = minX; x <= maxX; x += step) {
        const ImU32 color = std::fabs(x) < 0.001f ? axisXColor : minorColor;
        const float thickness = std::fabs(x) < 0.001f ? 2.0f : 1.0f;
        for (float z = minZ; z < maxZ; z += step) {
            const float nextZ = (std::min)(z + step, maxZ);
            drawWorldSegment({ x, 0.0f, z }, { x, 0.0f, nextZ }, { x, 0.0f, (z + nextZ) * 0.5f }, color, thickness);
        }
    }
    for (float z = minZ; z <= maxZ; z += step) {
        const ImU32 color = std::fabs(z) < 0.001f ? axisZColor : minorColor;
        const float thickness = std::fabs(z) < 0.001f ? 2.0f : 1.0f;
        for (float x = minX; x < maxX; x += step) {
            const float nextX = (std::min)(x + step, maxX);
            drawWorldSegment({ x, 0.0f, z }, { nextX, 0.0f, z }, { (x + nextX) * 0.5f, 0.0f, z }, color, thickness);
        }
    }
}

void EditorLayer::DrawSelectionOutlineOverlay(const DirectX::XMFLOAT4& viewRect,
                                              const DirectX::XMFLOAT4X4& view,
                                              const DirectX::XMFLOAT4X4& projection) const
{
    if (!m_gameLayer || m_sceneViewMode == SceneViewMode::Mode2D) {
        return;
    }

    Registry& registry = m_gameLayer->GetRegistry();
    const EntityID entity = EditorSelection::Instance().GetPrimaryEntity();
    if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
        return;
    }

    auto* mesh = registry.GetComponent<MeshComponent>(entity);
    auto* transform = registry.GetComponent<TransformComponent>(entity);
    if (!mesh || !mesh->model || !transform) {
        return;
    }

    DirectX::BoundingBox bounds{};
    if (!TryGetLiveMeshWorldBounds(*mesh, transform, bounds)) {
        return;
    }
    DirectX::XMFLOAT3 corners[8]{};
    BuildBoundingBoxCorners(bounds, corners);
    static constexpr int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImU32 color = IM_COL32(80, 180, 255, 255);
    for (const auto& edge : edges) {
        ImVec2 a{};
        ImVec2 b{};
        if (ProjectWorldToSceneScreen(viewRect, view, projection, corners[edge[0]], a) &&
            ProjectWorldToSceneScreen(viewRect, view, projection, corners[edge[1]], b)) {
            drawList->AddLine(a, b, color, 2.0f);
        }
    }
}

void EditorLayer::DrawSceneIconOverlay(const DirectX::XMFLOAT4& viewRect,
                                       const DirectX::XMFLOAT4X4& view,
                                       const DirectX::XMFLOAT4X4& projection) const
{
    if (!m_gameLayer || m_sceneViewMode == SceneViewMode::Mode2D) {
        return;
    }

    Registry& registry = m_gameLayer->GetRegistry();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const auto drawIcon = [&](const DirectX::XMFLOAT3& worldPos, ImU32 color, const char* text, const std::string& label) {
        ImVec2 screen{};
        if (!ProjectWorldToSceneScreen(viewRect, view, projection, worldPos, screen)) {
            return;
        }
        drawList->AddCircleFilled(screen, 7.0f, color, 12);
        drawList->AddCircle(screen, 7.0f, IM_COL32(12, 16, 24, 220), 12, 1.5f);
        drawList->AddText(ImVec2(screen.x + 12.0f, screen.y - 8.0f), IM_COL32(240, 244, 248, 255), label.c_str());
        drawList->AddText(ImVec2(screen.x - 3.0f, screen.y - 6.0f), IM_COL32(10, 12, 16, 255), text);
    };

    for (Archetype* archetype : registry.GetAllArchetypes()) {
        const auto& signature = archetype->GetSignature();
        auto* transformColumn = signature.test(TypeManager::GetComponentTypeID<TransformComponent>())
            ? archetype->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>())
            : nullptr;
        auto* nameColumn = signature.test(TypeManager::GetComponentTypeID<NameComponent>())
            ? archetype->GetColumn(TypeManager::GetComponentTypeID<NameComponent>())
            : nullptr;
        const auto& entities = archetype->GetEntities();

        if (m_showSceneLightIcons && transformColumn && signature.test(TypeManager::GetComponentTypeID<LightComponent>())) {
            auto* lightColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<LightComponent>());
            for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
                const auto& transform = *static_cast<TransformComponent*>(transformColumn->Get(i));
                const auto& light = *static_cast<LightComponent*>(lightColumn->Get(i));
                const std::string label = nameColumn
                    ? static_cast<NameComponent*>(nameColumn->Get(i))->name
                    : GetEntityLabel(registry, entities[i]);
                const ImU32 color = (light.type == LightType::Directional)
                    ? IM_COL32(255, 210, 90, 220)
                    : IM_COL32(255, 160, 96, 220);
                const char* text = (light.type == LightType::Directional) ? "L" : "P";
                drawIcon(transform.worldPosition, color, text, label);
            }
        }

        if (m_showSceneCameraIcons && transformColumn &&
            (signature.test(TypeManager::GetComponentTypeID<CameraLensComponent>()) ||
             signature.test(TypeManager::GetComponentTypeID<Camera2DComponent>()))) {
            for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
                const auto& transform = *static_cast<TransformComponent*>(transformColumn->Get(i));
                const std::string label = nameColumn
                    ? static_cast<NameComponent*>(nameColumn->Get(i))->name
                    : GetEntityLabel(registry, entities[i]);
                drawIcon(transform.worldPosition, IM_COL32(110, 190, 255, 220), "C", label);
            }
        }
    }
}

void EditorLayer::DrawSequencerCameraOverlay(const DirectX::XMFLOAT4& viewRect,
                                             const DirectX::XMFLOAT4X4& view,
                                             const DirectX::XMFLOAT4X4& projection) const
{
    if (!m_gameLayer || m_sceneViewMode == SceneViewMode::Mode2D || !m_sequencerPanel.ShouldDrawCameraPaths()) {
        return;
    }

    const CinematicSequenceAsset& sequence = m_sequencerPanel.GetSequence();
    const uint64_t selectedSectionId = m_sequencerPanel.GetSelectedSectionId();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    auto drawCameraPoint = [&](const DirectX::XMFLOAT3& worldPos, ImU32 color, bool selected) {
        ImVec2 screen{};
        if (!ProjectWorldToSceneScreen(viewRect, view, projection, worldPos, screen)) {
            return;
        }
        const float radius = selected ? 6.5f : 4.5f;
        drawList->AddCircleFilled(screen, radius, color, 12);
        drawList->AddCircle(screen, radius, IM_COL32(12, 16, 24, 220), 12, selected ? 2.0f : 1.0f);
    };

    auto drawCameraSection = [&](const CinematicSection& section) {
        if (section.trackType != CinematicTrackType::Camera || section.muted || section.locked) {
            return;
        }

        const bool selected = (section.sectionId == selectedSectionId);
        const ImU32 pathColor = selected ? IM_COL32(255, 214, 102, 255) : IM_COL32(120, 170, 255, 220);
        const ImU32 startColor = selected ? IM_COL32(255, 170, 80, 255) : IM_COL32(120, 255, 180, 220);
        const ImU32 endColor = selected ? IM_COL32(255, 96, 96, 255) : IM_COL32(255, 120, 160, 220);

        constexpr int kSegments = 16;
        DirectX::XMFLOAT3 previous = {};
        bool hasPrevious = false;
        for (int segment = 0; segment <= kSegments; ++segment) {
            const float t = static_cast<float>(segment) / static_cast<float>(kSegments);
            DirectX::XMFLOAT3 point =
                (section.camera.cameraMode == CinematicCameraMode::LookAtCamera)
                ? DirectX::XMFLOAT3{
                    section.camera.startEye.x + (section.camera.endEye.x - section.camera.startEye.x) * t,
                    section.camera.startEye.y + (section.camera.endEye.y - section.camera.startEye.y) * t,
                    section.camera.startEye.z + (section.camera.endEye.z - section.camera.startEye.z) * t
                }
                : DirectX::XMFLOAT3{
                    section.camera.startPosition.x + (section.camera.endPosition.x - section.camera.startPosition.x) * t,
                    section.camera.startPosition.y + (section.camera.endPosition.y - section.camera.startPosition.y) * t,
                    section.camera.startPosition.z + (section.camera.endPosition.z - section.camera.startPosition.z) * t
                };

            if (hasPrevious) {
                ImVec2 a{};
                ImVec2 b{};
                if (ProjectWorldToSceneScreen(viewRect, view, projection, previous, a) &&
                    ProjectWorldToSceneScreen(viewRect, view, projection, point, b)) {
                    drawList->AddLine(a, b, pathColor, selected ? 2.5f : 1.5f);
                }
            }
            previous = point;
            hasPrevious = true;
        }

        const DirectX::XMFLOAT3 start =
            (section.camera.cameraMode == CinematicCameraMode::LookAtCamera) ? section.camera.startEye : section.camera.startPosition;
        const DirectX::XMFLOAT3 end =
            (section.camera.cameraMode == CinematicCameraMode::LookAtCamera) ? section.camera.endEye : section.camera.endPosition;
        drawCameraPoint(start, startColor, selected);
        drawCameraPoint(end, endColor, selected);
    };

    for (const auto& track : sequence.masterTracks) {
        for (const auto& section : track.sections) {
            drawCameraSection(section);
        }
    }
    for (const auto& binding : sequence.bindings) {
        for (const auto& track : binding.tracks) {
            for (const auto& section : track.sections) {
                drawCameraSection(section);
            }
        }
    }
}

void EditorLayer::DrawSceneBoundsOverlay(const DirectX::XMFLOAT4& viewRect,
                                         const DirectX::XMFLOAT4X4& view,
                                         const DirectX::XMFLOAT4X4& projection) const
{
    if (!m_gameLayer || m_sceneViewMode == SceneViewMode::Mode2D) {
        return;
    }

    Registry& registry = m_gameLayer->GetRegistry();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImU32 color = (m_sceneShadingMode == SceneShadingMode::Wireframe)
        ? IM_COL32(120, 220, 255, 180)
        : IM_COL32(120, 180, 255, 120);
    static constexpr int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };

    for (Archetype* archetype : registry.GetAllArchetypes()) {
        const auto& signature = archetype->GetSignature();
        if (!signature.test(TypeManager::GetComponentTypeID<MeshComponent>()) ||
            !signature.test(TypeManager::GetComponentTypeID<TransformComponent>())) {
            continue;
        }
        auto* meshColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<MeshComponent>());
        auto* transformColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
        for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
            const auto* mesh = static_cast<MeshComponent*>(meshColumn->Get(i));
            const auto* transform = static_cast<TransformComponent*>(transformColumn->Get(i));
            if (!mesh || !mesh->model || !mesh->isVisible || !transform) {
                continue;
            }

            DirectX::BoundingBox bounds{};
            if (!TryGetLiveMeshWorldBounds(*mesh, transform, bounds)) {
                continue;
            }
            DirectX::XMFLOAT3 corners[8]{};
            BuildBoundingBoxCorners(bounds, corners);
            for (const auto& edge : edges) {
                ImVec2 a{};
                ImVec2 b{};
                if (ProjectWorldToSceneScreen(viewRect, view, projection, corners[edge[0]], a) &&
                    ProjectWorldToSceneScreen(viewRect, view, projection, corners[edge[1]], b)) {
                    drawList->AddLine(a, b, color, m_sceneShadingMode == SceneShadingMode::Wireframe ? 1.6f : 1.0f);
                }
            }
        }
    }
}

void EditorLayer::DrawSceneCollisionOverlay(const DirectX::XMFLOAT4& viewRect,
                                            const DirectX::XMFLOAT4X4& view,
                                            const DirectX::XMFLOAT4X4& projection) const
{
    if (!m_gameLayer || m_sceneViewMode == SceneViewMode::Mode2D) {
        return;
    }

    using namespace DirectX;
    Registry& registry = m_gameLayer->GetRegistry();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    static constexpr int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };

    for (Archetype* archetype : registry.GetAllArchetypes()) {
        const auto& signature = archetype->GetSignature();
        if (!signature.test(TypeManager::GetComponentTypeID<ColliderComponent>()) ||
            !signature.test(TypeManager::GetComponentTypeID<TransformComponent>())) {
            continue;
        }

        auto* colliderColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<ColliderComponent>());
        auto* transformColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
        for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
            const auto& collider = *static_cast<ColliderComponent*>(colliderColumn->Get(i));
            const auto& transform = *static_cast<TransformComponent*>(transformColumn->Get(i));
            if (!collider.enabled) {
                continue;
            }

            const XMMATRIX world = XMLoadFloat4x4(&transform.worldMatrix);
            for (const auto& element : collider.elements) {
                if (!element.enabled) {
                    continue;
                }

                BoundingBox localBounds{};
                switch (element.type) {
                case ColliderShape::Sphere:
                    localBounds.Center = { element.offsetLocal.x, element.offsetLocal.y, element.offsetLocal.z };
                    localBounds.Extents = { element.radius, element.radius, element.radius };
                    break;
                case ColliderShape::Capsule:
                    localBounds.Center = {
                        element.offsetLocal.x,
                        element.offsetLocal.y + element.height * 0.5f,
                        element.offsetLocal.z
                    };
                    localBounds.Extents = {
                        element.radius,
                        element.height * 0.5f + element.radius,
                        element.radius
                    };
                    break;
                case ColliderShape::Box:
                default:
                    localBounds.Center = { element.offsetLocal.x, element.offsetLocal.y, element.offsetLocal.z };
                    localBounds.Extents = { element.size.x * 0.5f, element.size.y * 0.5f, element.size.z * 0.5f };
                    break;
                }

                BoundingBox worldBounds{};
                localBounds.Transform(worldBounds, world);
                XMFLOAT3 corners[8]{};
                BuildBoundingBoxCorners(worldBounds, corners);
                const ImU32 color = IM_COL32(
                    static_cast<int>(std::clamp(element.color.x, 0.0f, 1.0f) * 255.0f),
                    static_cast<int>(std::clamp(element.color.y, 0.0f, 1.0f) * 255.0f),
                    static_cast<int>(std::clamp(element.color.z, 0.0f, 1.0f) * 255.0f),
                    190);

                for (const auto& edge : edges) {
                    ImVec2 a{};
                    ImVec2 b{};
                    if (ProjectWorldToSceneScreen(viewRect, view, projection, corners[edge[0]], a) &&
                        ProjectWorldToSceneScreen(viewRect, view, projection, corners[edge[1]], b)) {
                        drawList->AddLine(a, b, color, 1.0f);
                    }
                }
            }
        }
    }
}

void EditorLayer::DrawStatsOverlay(const DirectX::XMFLOAT4& viewRect, const char* label) const
{
    if (viewRect.z <= 1.0f || viewRect.w <= 1.0f) {
        return;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 boxMin(viewRect.x + 10.0f, viewRect.y + 10.0f);
    const ImVec2 boxMax(viewRect.x + 190.0f, viewRect.y + 64.0f);
    drawList->AddRectFilled(boxMin, boxMax, IM_COL32(10, 12, 16, 150), 6.0f);
    drawList->AddRect(boxMin, boxMax, IM_COL32(70, 78, 92, 180), 6.0f);

    const auto& selection = EditorSelection::Instance();
    std::string line1 = std::string(label) + " View  |  " + SceneViewModeToString(m_sceneViewMode);
    if (std::strcmp(label, "Scene") == 0) {
        line1 += "  |  ";
        line1 += SceneShadingModeToString(m_sceneShadingMode);
    }
    std::ostringstream line2;
    line2 << "FPS " << std::fixed << std::setprecision(1) << ImGui::GetIO().Framerate
          << "  |  Frame " << std::setprecision(2) << (1000.0f / (std::max)(ImGui::GetIO().Framerate, 0.01f)) << " ms";
    std::ostringstream line3;
    line3 << "Selected " << selection.GetSelectedEntityCount() << "  |  Cam " << std::setprecision(1) << m_cameraMoveSpeed;

    drawList->AddText(ImVec2(boxMin.x + 10.0f, boxMin.y + 8.0f), IM_COL32(230, 235, 245, 255), line1.c_str());
    drawList->AddText(ImVec2(boxMin.x + 10.0f, boxMin.y + 24.0f), IM_COL32(210, 216, 226, 240), line2.str().c_str());
    drawList->AddText(ImVec2(boxMin.x + 10.0f, boxMin.y + 40.0f), IM_COL32(210, 216, 226, 240), line3.str().c_str());
}

