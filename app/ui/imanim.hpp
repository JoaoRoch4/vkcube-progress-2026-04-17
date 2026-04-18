#pragma once

// imanim.hpp — Lightweight per-frame animation helpers for Dear ImGui
//
// All helpers are stateless: they derive their animation phase from
// ImGui::GetTime() so no external state management is required.
//
// Inspired by the ImAnim library concept (github.com/leiradel/ImGuiAnim).
//
// Usage example:
//   if (busy)
//       ImAnim::DrawSpinner("Thinking");
//   if (ImAnim::Blink())
//       ImGui::Text("|");  // blinking cursor

#include "imgui.h"
#include <array>
#include <cmath>

namespace ImAnim {

// ── Spinner helpers ───────────────────────────────────────────────────────────

// Returns a Braille-spinner UTF-8 character that cycles at `fps` frames/sec.
inline const char* SpinnerChar(float fps = 10.0f)
{
    static constexpr std::array<const char*, 8> kFrames {
        "\xe2\xa0\x8b", // ⠋
        "\xe2\xa0\x99", // ⠙
        "\xe2\xa0\xb9", // ⠹
        "\xe2\xa0\xb8", // ⠸
        "\xe2\xa0\xbc", // ⠼
        "\xe2\xa0\xb4", // ⠴
        "\xe2\xa0\xa6", // ⠦
        "\xe2\xa0\xa7", // ⠧
    };
    const int n   = static_cast<int>(kFrames.size());
    const int idx = static_cast<int>(ImGui::GetTime() * fps) % n;
    return kFrames.at(static_cast<std::size_t>(idx));
}

// Returns an ASCII spinner character for non-Unicode contexts.
inline char SpinnerAscii(float fps = 10.0f)
{
    static constexpr std::array<char, 4> kFrames { '|', '/', '-', '\\' };
    const int idx = static_cast<int>(ImGui::GetTime() * fps) % 4;
    return kFrames.at(static_cast<std::size_t>(idx));
}

// ── Blink / pulse ─────────────────────────────────────────────────────────────

// Returns true on the "on" half of a square-wave blink cycle.
// period: full on+off cycle length in seconds.
inline bool Blink(float period = 1.0f)
{
    return std::fmod(ImGui::GetTime(), static_cast<double>(period))
           < static_cast<double>(period * 0.5f);
}

// Dot pulse animation — returns one of "   " / ".  " / ".. " / "..."
// period: time between dot steps in seconds.
inline const char* DotPulse(float period = 0.35f)
{
    static constexpr std::array<const char*, 4> kFrames { "   ", ".  ", ".. ", "..." };
    const int idx = static_cast<int>(ImGui::GetTime() / period) % 4;
    return kFrames.at(static_cast<std::size_t>(idx));
}

// ── Easing / fade ─────────────────────────────────────────────────────────────

// Smooth-step ease-in-out (Hermite): maps t in [0,1] → [0,1].
inline float SmoothStep(float t)
{
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

// Returns an alpha value [0,1] for a fade-in that started at `start_time`
// (use ImGui::GetTime() when the element first appeared) and lasts `duration`
// seconds.
inline float FadeIn(double start_time, float duration = 0.25f)
{
    const float elapsed = static_cast<float>(ImGui::GetTime() - start_time);
    return SmoothStep(elapsed / duration);
}

// ── Composite widgets ─────────────────────────────────────────────────────────

// Renders an inline "⠋ <label>..." spinner using ImGui::TextColored.
// color: spinner + label foreground; defaults to a soft yellow.
inline void DrawSpinner(const char* label = "Working",
    ImVec4 color = { 1.0f, 0.88f, 0.3f, 1.0f })
{
    ImGui::TextColored(color, "%s %s%s", SpinnerChar(), label, DotPulse());
}

// Renders a blinking block cursor `█` (or space) at the current cursor pos.
// Falls back to `|` for readability.
inline void DrawCursor(ImVec4 color = { 0.3f, 1.0f, 0.3f, 1.0f })
{
    if (Blink(1.0f))
        ImGui::TextColored(color, "\xe2\x96\x88"); // █
    else
        ImGui::TextUnformatted(" ");
}

// Returns an alpha-modulated color: useful for fade-in on newly added items.
inline ImVec4 FadedColor(ImVec4 base, float alpha)
{
    return { base.x, base.y, base.z, base.w * alpha };
}

} // namespace ImAnim
