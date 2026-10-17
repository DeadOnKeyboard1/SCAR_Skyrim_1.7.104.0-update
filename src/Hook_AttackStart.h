#pragma once
#include "DKUtil/Hook.hpp"
#include "DataHandler.h"
#include "Function.h"

namespace SCAR
{
	using namespace DKUtil::Alias;

	class AttackDistancHook
	{
		// 1-6-353: 0x837410 + 0x404
		static inline constexpr std::uint64_t AE_FuncID = 49170;
		static inline constexpr std::uint64_t AE_AIFuncID = 49171;
		static inline constexpr std::ptrdiff_t AE_OffsetL = 0x404;
		static inline constexpr std::ptrdiff_t AE_OffsetH = 0x409;

		// 1-5-97: 0x80C020 + 0x4A6
		static inline constexpr std::uint64_t SE_FuncID = 48139;
		static inline constexpr std::uint64_t SE_AIFuncID = 48140;
		static inline constexpr std::ptrdiff_t SE_OffsetL = 0x4A6;
		static inline constexpr std::ptrdiff_t SE_OffsetH = 0x4AB;

		// al
		// r15 / r13
		// rsp+68
		// rsp+20
		static inline constexpr Patch AE_Prolog
		{
			// call AIFunc
			"\xE8\x00\x00\x00\x00"
			// mov rcx, rax
			"\x48\x89\xC1"
			// mov rdx, r15
			"\x4C\x89\xFA"
			// mov r8, [rsp+0x68]
			"\x4C\x8B\x44\x24\x68"
			// mov r9, [rsp+0x20]
			"\x4C\x8B\x4C\x24\x20"
			// push r13 ~ r15
			"\x41\x55\x41\x57",
			25
		};

		static inline constexpr Patch AE_Epilog{
			// pop r15 ~ r13
			"\x41\x5F\x41\x5D",
			4
		};

		// 1.7.104: target actor is saved at rbp+0xD0, not rsp+0x68.
		static inline constexpr Patch AE17104_Prolog{
			"\xE8\x00\x00\x00\x00"
			"\x48\x89\xC1"
			"\x4C\x89\xFA"
			"\x4C\x8B\x85\xD0\x00\x00\x00"
			"\x4C\x8B\x4C\x24\x20"
			"\x41\x55\x41\x57",
			27
		};

		static inline constexpr Patch SE_Prolog
		{
			// call AIFunc
			"\xE8\x00\x00\x00\x00"
			// mov rcx, rax
			"\x48\x89\xC1"
			// mov rdx, r13
			"\x4C\x89\xEA"
			// mov r8, [rsp+0x68]
			"\x4C\x8B\x44\x24\x68"
			// mov r9, [rsp+0x20]
			"\x4C\x8B\x4C\x24\x20"
			// push r12 ~ r13
			"\x41\x54\x41\x55",
			25
		};

		static inline constexpr Patch SE_Epilog{
			// pop r13 ~ r12
			"\x41\x5D\x41\x5C",
			4
		};

		static bool RecheckAttackDistance(bool a_originResult, RE::Actor* a_attacker, RE::Actor* a_target, RE::AttackData* a_attackData);

	public:
		static const Patch& Prolog(bool ae, bool primary)
		{
			return primary ? AE17104_Prolog : (ae ? AE_Prolog : SE_Prolog);
		}
		static const Patch& Epilog(bool ae) { return ae ? AE_Epilog : SE_Epilog; }
		static constexpr std::size_t TrampolineBytes = sizeof(Imm64) + AE17104_Prolog.Size +
			sizeof(DKUtil::Hook::SubRsp<>) + sizeof(DKUtil::Hook::CallRip) +
			sizeof(DKUtil::Hook::AddRsp) + AE_Epilog.Size + sizeof(DKUtil::Hook::JmpRel);
		static bool Install()
		{
			auto handle = DKUtil::Hook::AddCaveHook(
				DKUtil::Hook::IDToAbs(AE_FuncID, SE_FuncID),
				DKUtil::Hook::RuntimeOffset(AE_OffsetL, AE_OffsetH, SE_OffsetL, SE_OffsetH),
				FUNC_INFO(RecheckAttackDistance),
				&Prolog(REL::Module::IsAE(), REL::Module::get().version() == REL::Version{1,7,104,0}),
				&Epilog(REL::Module::IsAE()));

			// recalculate displacement
			const auto relative = static_cast<std::intptr_t>(DKUtil::Hook::IDToAbs(AE_AIFuncID, SE_AIFuncID)) - static_cast<std::intptr_t>(handle->TramEntry + 5);
			if (relative < std::numeric_limits<Disp32>::min() || relative > std::numeric_limits<Disp32>::max()) {
				ERROR("SCAR distance trampoline is out of CALL range; hook not installed");
				return false;
			}
			DKUtil::Hook::WriteImm(handle->TramEntry + sizeof(OpCode), static_cast<Imm32>(relative));

			handle->Enable();
			return true;
		}
	};

	class AttackAngleHook
	{
	public:
		static void InstallHook()
		{
			auto& trampoline = SKSE::GetTrampoline();

			REL::Relocation<std::uintptr_t> AttackDistanceBase{ RELOCATION_ID(48139, 49170) };
			_GetAttackAngle = trampoline.write_call<5>(AttackDistanceBase.address() + REL::Relocate(0x493, 0x3F1), GetAttackAngle);
			INFO("Hook GetAttackAngle!");
		}

	private:
		static bool GetAttackAngle(RE::Actor* a1, RE::Actor* a2, const RE::NiPoint3& a3, const RE::NiPoint3& a4, RE::BGSAttackData* a5, float a6, void* a7, bool a8);

		static inline REL::Relocation<decltype(GetAttackAngle)> _GetAttackAngle;
	};

	class AttackActionHook
	{
	public:
		static void InstallHook()
		{
			auto& trampoline = SKSE::GetTrampoline();

			REL::Relocation<std::uintptr_t> AttackActionBase{ RELOCATION_ID(48139, 49170) };
			_PerformAttackAction = trampoline.write_call<5>(AttackActionBase.address() + REL::Relocate(0x4D7, 0x435), PerformAttackAction);
			INFO("Hook PerformAttackAction!");
		}

	private:
		static bool PerformAttackAction(RE::TESActionData* a_actionData);

		static inline REL::Relocation<decltype(PerformAttackAction)> _PerformAttackAction;
	};
}
