// Runtime parameter-override authoring section for EffectEditorPanel.
// Extracted from EffectEditorPanel.cpp; method bodies stay on the panel class.

#include "EffectEditorPanel.h"

#include <string>
#include <vector>
#include <imgui.h>

#include "Component/EffectParameterOverrideComponent.h"
#include "EffectRuntime/EffectParameterBindings.h"
#include "Entity/Entity.h"
#include "Registry/Registry.h"

EffectParameterOverrideComponent EffectEditorPanel::BuildSuggestedOverrideComponent() const
{
    EffectParameterOverrideComponent component;
    component.enabled = false;

    for (const auto& parameter : m_asset.exposedParameters) {
        if (parameter.valueType == EffectValueType::Float && component.scalarParameter.empty()) {
            component.scalarParameter = parameter.name;
            component.scalarValue = parameter.defaultValue.x;
        } else if (parameter.valueType == EffectValueType::Color && component.colorParameter.empty()) {
            component.colorParameter = parameter.name;
            component.colorValue = parameter.defaultValue;
        }
    }

    return component;
}

void EffectEditorPanel::EnsureRuntimeOverrideComponent(EntityID entity)
{
    if (!m_registry || Entity::IsNull(entity) || !m_registry->IsAlive(entity)) {
        return;
    }

    EffectParameterOverrideComponent suggested = BuildSuggestedOverrideComponent();
    if (suggested.scalarParameter.empty() && suggested.colorParameter.empty()) {
        return;
    }

    auto* existing = m_registry->GetComponent<EffectParameterOverrideComponent>(entity);
    if (!existing) {
        m_registry->AddComponent(entity, suggested);
        return;
    }

    if (existing->scalarParameter.empty() && !suggested.scalarParameter.empty()) {
        existing->scalarParameter = suggested.scalarParameter;
        existing->scalarValue = suggested.scalarValue;
    }
    if (existing->colorParameter.empty() && !suggested.colorParameter.empty()) {
        existing->colorParameter = suggested.colorParameter;
        existing->colorValue = suggested.colorValue;
    }
}

void EffectEditorPanel::DrawRuntimeOverrideSection(const char* label, EntityID entity, bool autoAttach)
{
    if (!m_registry || Entity::IsNull(entity) || !m_registry->IsAlive(entity)) {
        ImGui::TextDisabled("%s unavailable.", label);
        return;
    }

    if (autoAttach) {
        EnsureRuntimeOverrideComponent(entity);
    }

    auto* overrideComponent = m_registry->GetComponent<EffectParameterOverrideComponent>(entity);
    if (!overrideComponent) {
        ImGui::TextDisabled("%s has no runtime override component.", label);
        if (ImGui::Button((std::string("Add Override##") + label).c_str())) {
            EnsureRuntimeOverrideComponent(entity);
        }
        return;
    }

    auto drawParameterCombo = [&](const char* comboLabel, EffectValueType type, std::string& currentName) {
        std::vector<const EffectExposedParameter*> candidates;
        for (const auto& parameter : m_asset.exposedParameters) {
            if (parameter.valueType == type) {
                candidates.push_back(&parameter);
            }
        }

        if (candidates.empty()) {
            ImGui::TextDisabled("No %s parameters.", type == EffectValueType::Float ? "float" : "color");
            return;
        }

        const char* previewValue = currentName.empty() ? "(None)" : currentName.c_str();
        if (ImGui::BeginCombo(comboLabel, previewValue)) {
            for (const auto* candidate : candidates) {
                const bool selected = currentName == candidate->name;
                if (ImGui::Selectable(candidate->name.c_str(), selected)) {
                    currentName = candidate->name;
                    if (type == EffectValueType::Float) {
                        overrideComponent->scalarValue = candidate->defaultValue.x;
                    } else {
                        overrideComponent->colorValue = candidate->defaultValue;
                    }
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    };

    ImGui::TextUnformatted(label);
    ImGui::Checkbox((std::string("Enabled##") + label).c_str(), &overrideComponent->enabled);
    ImGui::Separator();

    if (!overrideComponent->scalarParameter.empty()) {
        drawParameterCombo((std::string("Float Parameter##") + label).c_str(), EffectValueType::Float, overrideComponent->scalarParameter);
        ImGui::DragFloat((std::string("Float Value##") + label).c_str(), &overrideComponent->scalarValue, 0.01f);
        if (const char* binding = EffectParameterBindings::DescribeBinding(EffectValueType::Float, overrideComponent->scalarParameter)) {
            ImGui::TextDisabled("Binding: %s", binding);
        } else {
            ImGui::TextDisabled("Binding: custom / inactive");
        }
    } else {
        ImGui::TextDisabled("No float override selected.");
    }

    ImGui::Spacing();

    if (!overrideComponent->colorParameter.empty()) {
        drawParameterCombo((std::string("Color Parameter##") + label).c_str(), EffectValueType::Color, overrideComponent->colorParameter);
        ImGui::ColorEdit4((std::string("Color Value##") + label).c_str(), &overrideComponent->colorValue.x);
        if (const char* binding = EffectParameterBindings::DescribeBinding(EffectValueType::Color, overrideComponent->colorParameter)) {
            ImGui::TextDisabled("Binding: %s", binding);
        } else {
            ImGui::TextDisabled("Binding: custom / inactive");
        }
    } else {
        ImGui::TextDisabled("No color override selected.");
    }
}
