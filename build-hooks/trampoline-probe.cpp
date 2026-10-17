#include "PCH.h"
#include "Hook_AttackStart.h"
#include <cstdio>

static bool probe(bool, RE::Actor*, RE::Actor*, RE::AttackData*) { return true; }

int main()
{
    constexpr auto caveBytes = SCAR::AttackDistancHook::TrampolineBytes;
    constexpr auto totalBytes = caveBytes + 2 * 14;
    static_assert(caveBytes == 58 && totalBytes == 86);
    auto memory = static_cast<std::byte*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    if (!memory) return 1;
    auto& trampoline = SKSE::GetTrampoline();
    trampoline.set_trampoline(memory, totalBytes);
    std::array<std::byte, 27> prolog{};
    std::array<std::byte, 4> epilog{};
    DKUtil::Hook::Patch before{ prolog.data(), prolog.size() };
    DKUtil::Hook::Patch after{ epilog.data(), epilog.size() };
    auto cave = DKUtil::Hook::AddCaveHook(reinterpret_cast<std::uintptr_t>(memory + 512),
        { 0, 5 }, FUNC_INFO(probe), &before, &after);
    cave->Enable();
    if (trampoline.allocated_size() != caveBytes) return 2;
    for (std::size_t i = 0; i < 2; ++i) {
        auto source = reinterpret_cast<std::uintptr_t>(memory + 640 + i * 16);
        *reinterpret_cast<unsigned char*>(source) = 0xE8;
        *reinterpret_cast<std::int32_t*>(source + 1) = 0;
        trampoline.write_call<5>(source, reinterpret_cast<std::uintptr_t>(memory + 768 + i * 16));
    }
    if (trampoline.allocated_size() != totalBytes || trampoline.free_size() != 0) return 3;
    std::printf("PASS: real DKUtil cave = %zu bytes; two real CommonLib CALL stubs = 28 bytes; total = %zu bytes\n", caveBytes, totalBytes);
    return 0;
}
