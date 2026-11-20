#pragma once

#include <imgui.h>
#include <functional>
#include <vector>
#include <algorithm>
#include <string_view>

namespace ebooster::UI {

enum class Sizing {
    Fixed,
    Stretch
};

struct Element {
    Sizing sizing{Sizing::Fixed};
    float size{0.0f}; // width or height for Fixed; flex weight for Stretch (default 1.0f)
    std::function<void(float allocatedSize)> draw;

    static Element Fixed(float px, std::function<void()> fn) {
        return Element{Sizing::Fixed, px, [f = std::move(fn)](float) { if (f) f(); }};
    }

    static Element Fixed(float px, std::function<void(float allocated)> fn) {
        return Element{Sizing::Fixed, px, std::move(fn)};
    }

    static Element Stretch(float weight, std::function<void(float allocated)> fn) {
        return Element{Sizing::Stretch, weight, std::move(fn)};
    }

    static Element Stretch(float weight, std::function<void()> fn) {
        return Element{Sizing::Stretch, weight, [f = std::move(fn)](float) { if (f) f(); }};
    }

    static Element Fill(std::function<void(float allocated)> fn) {
        return Element{Sizing::Stretch, 1.0f, std::move(fn)};
    }

    static Element Fill(std::function<void()> fn) {
        return Element{Sizing::Stretch, 1.0f, [f = std::move(fn)](float) { if (f) f(); }};
    }
};

// ============================================================================
// Horizontal Row Layout (Automatic Flexbox & Spacing)
// ============================================================================
inline void Row(float spacing, const std::vector<Element>& elements) {
    if (elements.empty()) return;

    float totalAvail = ImGui::GetContentRegionAvail().x;
    float fixedSum = 0.0f;
    float stretchWeightSum = 0.0f;

    for (const auto& el : elements) {
        if (el.sizing == Sizing::Fixed) {
            fixedSum += el.size;
        } else {
            stretchWeightSum += (el.size > 0.0f ? el.size : 1.0f);
        }
    }

    float totalSpacing = spacing * static_cast<float>(elements.size() - 1);
    float remainingForStretch = std::max(0.0f, totalAvail - fixedSum - totalSpacing);

    for (size_t i = 0; i < elements.size(); ++i) {
        if (i > 0) {
            ImGui::SameLine(0, spacing);
        }

        const auto& el = elements[i];
        float allocated = 0.0f;
        if (el.sizing == Sizing::Fixed) {
            allocated = el.size;
        } else {
            float weight = (el.size > 0.0f ? el.size : 1.0f);
            allocated = (stretchWeightSum > 0.0f) ? (remainingForStretch * (weight / stretchWeightSum)) : 0.0f;
        }

        if (el.draw) {
            ImGui::SetNextItemWidth(allocated);
            el.draw(allocated);
        }
    }
}

inline void Row(const std::vector<Element>& elements) {
    Row(ImGui::GetStyle().ItemSpacing.x, elements);
}

// ============================================================================
// Vertical Column Layout (Automatic Vertical Flex & Clamping)
// ============================================================================
inline void Column(float spacing, const std::vector<Element>& elements) {
    if (elements.empty()) return;

    float totalAvail = ImGui::GetContentRegionAvail().y;
    float fixedSum = 0.0f;
    float stretchWeightSum = 0.0f;

    for (const auto& el : elements) {
        if (el.sizing == Sizing::Fixed) {
            fixedSum += el.size;
        } else {
            stretchWeightSum += (el.size > 0.0f ? el.size : 1.0f);
        }
    }

    float totalSpacing = spacing * static_cast<float>(elements.size() - 1);
    float remainingForStretch = std::max(0.0f, totalAvail - fixedSum - totalSpacing);

    for (size_t i = 0; i < elements.size(); ++i) {
        if (i > 0 && spacing > 0.0f) {
            ImGui::Dummy(ImVec2(0, spacing));
        }

        const auto& el = elements[i];
        float allocated = 0.0f;
        if (el.sizing == Sizing::Fixed) {
            allocated = el.size;
        } else {
            float weight = (el.size > 0.0f ? el.size : 1.0f);
            allocated = (stretchWeightSum > 0.0f) ? (remainingForStretch * (weight / stretchWeightSum)) : 0.0f;
        }

        if (el.draw) {
            el.draw(allocated);
        }
    }
}

// ============================================================================
// Anchors & Alignments (Automatic Flush Placement)
// ============================================================================
inline void AnchorRight(float itemWidth, float rightMargin, std::function<void()> drawFn) {
    float availW = ImGui::GetContentRegionAvail().x;
    float targetX = ImGui::GetCursorPosX() + availW - itemWidth - rightMargin;
    if (targetX > ImGui::GetCursorPosX()) {
        ImGui::SetCursorPosX(targetX);
    }
    if (drawFn) drawFn();
}

inline void AnchorRightSameLine(float itemWidth, float rightMargin, std::function<void()> drawFn) {
    float availW = ImGui::GetContentRegionAvail().x;
    float targetX = ImGui::GetCursorPosX() + availW - itemWidth - rightMargin;
    ImGui::SameLine(0, 0);
    if (targetX > ImGui::GetCursorPosX()) {
        ImGui::SetCursorPosX(targetX);
    }
    if (drawFn) drawFn();
}

inline void AnchorBottom(float itemHeight, float bottomMargin, std::function<void()> drawFn) {
    float availY = ImGui::GetContentRegionAvail().y;
    if (availY > itemHeight + bottomMargin) {
        ImGui::Dummy(ImVec2(0, availY - itemHeight - bottomMargin));
    }
    if (drawFn) drawFn();
}

inline void CenterHorizontal(float itemWidth, std::function<void()> drawFn) {
    float availW = ImGui::GetContentRegionAvail().x;
    if (availW > itemWidth) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availW - itemWidth) * 0.5f);
    }
    if (drawFn) drawFn();
}

// ============================================================================
// Modern RAII Card Container Scope
// ============================================================================
struct CardScope {
    bool isOpen{false};

    CardScope(const char* str_id, const ImVec2& size = ImVec2(0, 0),
              ImVec4 bg = ImVec4(0.08f, 0.10f, 0.14f, 0.95f),
              ImVec4 border = ImVec4(0.18f, 0.22f, 0.30f, 0.80f),
              float rounding = 8.0f,
              ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
        ImGui::PushStyleColor(ImGuiCol_Border,  border);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, rounding);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        isOpen = ImGui::BeginChild(str_id, size, true, flags);
    }

    ~CardScope() {
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }

    explicit operator bool() const { return isOpen; }
};

// ============================================================================
// Modern Lambda-Based Card Container
// ============================================================================
template<typename Fn>
inline void Card(const char* str_id, const ImVec2& size, Fn&& fn,
                 ImVec4 bg = ImVec4(0.08f, 0.10f, 0.14f, 0.95f),
                 ImVec4 border = ImVec4(0.18f, 0.22f, 0.30f, 0.80f),
                 float rounding = 8.0f,
                 ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
    ImGui::PushStyleColor(ImGuiCol_Border,  border);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    if (ImGui::BeginChild(str_id, size, true, flags)) {
        fn();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

} // namespace ebooster::UI
