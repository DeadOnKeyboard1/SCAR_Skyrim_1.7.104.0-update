#pragma once
#include <hde64.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <span>

namespace SCAR::NativeValidation
{
    struct Offsets { std::size_t angle, distance, action; };
    inline constexpr Offsets Sites(bool ae) { return ae ? Offsets{0x3F1,0x404,0x435} : Offsets{0x493,0x4A6,0x4D7}; }

    inline bool Call(std::span<const std::uint8_t> code, std::uintptr_t base, std::size_t offset, std::uintptr_t target)
    {
        bool boundary{};
        for (std::size_t pos = 0; pos < code.size();) {
            std::array<std::uint8_t,16> padded{};
            const auto length = (std::min)(padded.size(),code.size()-pos);
            std::memcpy(padded.data(),code.data()+pos,length);
            hde64s instruction{};
            hde64_disasm(padded.data(),&instruction);
            if (!instruction.len || instruction.flags & F_ERROR || instruction.len > code.size()-pos) return false;
            if (pos == offset) boundary = instruction.opcode == 0xE8 && instruction.len == 5;
            pos += instruction.len;
        }
        if (!boundary) return false;
        std::int32_t relative{};
        std::memcpy(&relative,code.data()+offset+1,4);
        return static_cast<std::intptr_t>(base+offset+5)+relative == static_cast<std::intptr_t>(target);
    }

    bool Check();
    std::uintptr_t CallTarget(std::uintptr_t site);
}
