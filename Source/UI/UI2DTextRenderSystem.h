// UI2DTextRenderSystem の UI 関連宣言をまとめます。
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "Component/RectTransformComponent.h"
#include "Component/TextComponent.h"
#include "Component/TransformComponent.h"
#include "Font/FontManager.h"
#include "RenderContext/RenderContext.h"
#include "RenderContext/RenderQueue.h"
#include "UI/UIHitTestSystem.h"

class UI2DTextRenderSystem
{
public:
    static void RenderText(const std::vector<UI2DTextPacket>& packets, const RenderContext& rc)
    {
        if (packets.empty() || !rc.commandList || rc.displayWidth <= 1 || rc.displayHeight <= 1) {
            return;
        }
        if (std::fabs(rc.projectionMatrix._34) > 0.0001f) {
            return;
        }

        const DirectX::XMFLOAT4 viewRect{
            0.0f,
            0.0f,
            static_cast<float>(rc.displayWidth),
            static_cast<float>(rc.displayHeight)
        };

        for (const UI2DTextPacket& packet : packets) {
            TransformComponent transform{};
            transform.worldPosition = packet.worldPosition;
            transform.worldRotation = packet.worldRotation;
            transform.worldScale = packet.worldScale;

            RectTransformComponent rect{};
            rect.sizeDelta = packet.sizeDelta;
            rect.pivot = packet.pivot;

            std::array<DirectX::XMFLOAT2, 4> corners{};
            if (!UIHitTestSystem::ComputeScreenCorners(transform, rect, viewRect, rc.viewMatrix, rc.projectionMatrix, corners)) {
                continue;
            }

            if (packet.pixelSnap || rc.cameraPixelSnap) {
                for (auto& corner : corners) {
                    corner.x = std::round(corner.x);
                    corner.y = std::round(corner.y);
                }
            }

            const float minX = (std::min)((std::min)(corners[0].x, corners[1].x), (std::min)(corners[2].x, corners[3].x));
            const float maxX = (std::max)((std::max)(corners[0].x, corners[1].x), (std::max)(corners[2].x, corners[3].x));
            const float minY = (std::min)((std::min)(corners[0].y, corners[1].y), (std::min)(corners[2].y, corners[3].y));
            const float maxY = (std::max)((std::max)(corners[0].y, corners[1].y), (std::max)(corners[2].y, corners[3].y));

            FontAlign align = FontAlign::Center;
            float x = (minX + maxX) * 0.5f;
            if (packet.alignment == static_cast<int>(TextAlignment::Left)) {
                align = FontAlign::Left;
                x = minX;
            } else if (packet.alignment == static_cast<int>(TextAlignment::Right)) {
                align = FontAlign::Right;
                x = maxX;
            }

            const float scale = (std::max)(0.01f, packet.fontSize / 32.0f);
            const float y = ((minY + maxY) * 0.5f) - packet.fontSize * 0.5f;
            const std::wstring wideText = ToWide(packet.text);
            if (wideText.empty()) {
                continue;
            }

            FontManager::Instance().DrawFormat(
                rc.commandList,
                packet.fontAssetPath.empty() ? std::string("ComboFont") : packet.fontAssetPath,
                x,
                y,
                packet.color,
                scale,
                align,
                L"%s",
                wideText.c_str());
        }
    }

private:
    static std::wstring ToWide(const std::string& text)
    {
        if (text.empty()) {
            return {};
        }

        int required = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        if (required <= 0) {
            required = MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, nullptr, 0);
            if (required <= 0) {
                return {};
            }
            std::wstring wide(static_cast<size_t>(required), L'\0');
            MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, wide.data(), required);
            if (!wide.empty() && wide.back() == L'\0') {
                wide.pop_back();
            }
            return wide;
        }

        std::wstring wide(static_cast<size_t>(required), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), required);
        if (!wide.empty() && wide.back() == L'\0') {
            wide.pop_back();
        }
        return wide;
    }
};
