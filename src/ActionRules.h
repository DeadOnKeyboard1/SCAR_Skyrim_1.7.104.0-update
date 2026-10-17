#pragma once
#include <algorithm>
#include <charconv>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <nlohmann/json.hpp>

namespace SCAR::ActionRules
{
    inline float Number(const nlohmann::json& data, const char* key)
    {
        const float value = data.at(key).get<float>();
        if (!std::isfinite(value)) throw std::invalid_argument("Non-finite SCAR number");
        return value;
    }

    inline std::string Type(std::string text)
    {
        for (auto& c : text) if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
        return text;
    }

    inline std::optional<bool> DebugFlag(std::string_view text)
    {
        const auto value = Type(std::string(text));
        if (value == "TRUE" || value == "1") return true;
        if (value == "FALSE" || value == "0") return false;
        return std::nullopt;
    }

    inline std::optional<float> ComboChance(std::string_view text)
    {
        const auto first = text.find_first_not_of(" \t\r\n");
        if (first == std::string_view::npos) return 100.f;
        text = text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
        if (text.starts_with('+')) { text.remove_prefix(1); if (text.starts_with('-')) return std::nullopt; }
        if (text.empty()) return std::nullopt;
        float value{};
        const auto parsed = std::from_chars(text.data(), text.data()+text.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data()+text.size() || !std::isfinite(value)) return std::nullopt;
        return std::clamp(value, 0.f, 100.f);
    }

    inline bool ChancePasses(float chance, float roll)
    {
        return std::isfinite(chance) && std::isfinite(roll) && chance > 0.f && (chance >= 100.f || chance >= roll);
    }

    inline bool WithinAngle(float angle, float start, float end)
    {
        if (!std::isfinite(angle) || !std::isfinite(start) || !std::isfinite(end)) return false;
        return start < end ? angle >= start && angle <= end : angle >= start || angle <= end;
    }
}
