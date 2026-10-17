#include "NativeValidation.h"

namespace SCAR::NativeValidation
{
    namespace
    {
        bool Readable(std::uintptr_t address, std::size_t size, bool executable = false)
        {
            if (!address || !size || address + size < address) return false;
            const auto end = address + size;
            while (address < end) {
                MEMORY_BASIC_INFORMATION info{};
                if (!VirtualQuery(reinterpret_cast<void*>(address),&info,sizeof(info)) || info.State != MEM_COMMIT || info.Protect & (PAGE_GUARD|PAGE_NOACCESS)) return false;
                if (executable && !(info.Protect & (PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))) return false;
                const auto next = reinterpret_cast<std::uintptr_t>(info.BaseAddress)+info.RegionSize;
                if (next <= address) return false;
                address = next;
            }
            return true;
        }

        std::optional<std::span<const std::uint8_t>> Body(std::uintptr_t address)
        {
            DWORD64 base{};
            const auto entry = RtlLookupFunctionEntry(address,&base,nullptr);
            if (!entry || base != REL::Module::get().base() || base+entry->BeginAddress != address || entry->EndAddress <= entry->BeginAddress) return std::nullopt;
            const auto size = entry->EndAddress-entry->BeginAddress;
            if (size > 65536 || !Readable(address,size,true)) return std::nullopt;
            return std::span(reinterpret_cast<const std::uint8_t*>(address),size);
        }

        bool Bytes(std::span<const std::uint8_t> body, std::size_t offset, std::initializer_list<std::uint8_t> wanted)
        {
            return offset <= body.size() && wanted.size() <= body.size()-offset && std::equal(wanted.begin(),wanted.end(),body.begin()+offset);
        }
    }

    std::uintptr_t CallTarget(std::uintptr_t site)
    {
        std::int32_t relative{};
        std::memcpy(&relative,reinterpret_cast<void*>(site+1),4);
        return site+5+relative;
    }

    bool Check()
    {
        if (REL::Module::IsVR()) return false;
        const auto ae = REL::Module::IsAE();
        const auto sites = Sites(ae);
        const auto action = RELOCATION_ID(48139,49170).address();
        const auto actionBody = Body(action);
        if (!actionBody) return false;
        for (const auto offset : {sites.angle,sites.distance,sites.action}) {
            if (offset+5 > actionBody->size()) return false;
            const auto target = CallTarget(action+offset);
            if (!Readable(target,1,true) || !Call(*actionBody,action,offset,target)) return false;
        }
        if (CallTarget(action+sites.distance) != RELOCATION_ID(48140,49171).address() ||
            CallTarget(action+sites.action) != RELOCATION_ID(43239,44440).address()) return false;
        if (ae && CallTarget(action+sites.angle) != REL::ID(47297).address()) return false;
        if (!Bytes(*actionBody,sites.distance+5,{0x84,0xC0})) return false;
        if (ae && REL::Module::get().version() == REL::Version{1,7,104,0}) {
            if (!Bytes(*actionBody,sites.angle-0x32,{0x48,0x8B,0x8D,0xD0,0,0,0}) ||
                !Bytes(*actionBody,sites.angle-3,{0x49,0x8B,0xCF}) ||
                !Bytes(*actionBody,sites.distance-6,{0x48,0x8B,0xD3,0x49,0x8B,0xCD})) return false;
        } else {
            // Preserve the original SE/AE branch only with its target stack load.
            const std::array<std::uint8_t,5> load{0x48,0x8B,0x4C,0x24,0x68};
            if (sites.angle < 0x40 || std::search(actionBody->begin()+sites.angle-0x40,actionBody->begin()+sites.angle,load.begin(),load.end()) == actionBody->begin()+sites.angle) return false;
        }
        const auto table = REL::Relocation<std::uintptr_t>{RE::VTABLE_Character[2]}.address();
        if (!Readable(table+8,8) || !Readable(*reinterpret_cast<const std::uintptr_t*>(table+8),1,true)) return false;
        INFO("SCAR native CALL/instruction/ABI validation passed");
        return true;
    }
}
