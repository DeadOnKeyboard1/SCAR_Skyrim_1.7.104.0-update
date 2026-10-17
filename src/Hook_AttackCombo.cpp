#include "Hook_AttackCombo.h"
#include "DataHandler.h"
#include "Function.h"

namespace SCAR
{
	static bool ShouldNextAttack(const char* a_payload)
	{
		const auto chance = ActionRules::ComboChance(a_payload ? a_payload : "");
		return chance && ActionRules::ChancePasses(*chance, Random::get<float>(0.f, 100.f));
	}

	EventResult AnimEventHook::ProcessEvent(RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_sink, const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource)
	{
		if (!a_event || !a_event->holder || a_event->tag.empty() || _strcmpi("SCAR_ComboStart", a_event->tag.c_str()) != 0)
			return _ProcessEvent(a_sink, a_event, a_eventSource);

		auto actor = a_event->holder->As<RE::Actor>();
		auto combatTarg = actor ? actor->GetActorRuntimeData().currentCombatTarget.get() : nullptr;
		if (actor && actor->GetActorRuntimeData().currentProcess && actor->GetActorRuntimeData().currentProcess->high && combatTarg &&
			ShouldNextAttack(a_event->payload.c_str()) && actor->RequestLOS(combatTarg.get()) && AttackRangeCheck::CheckPathing(actor, combatTarg.get())) {
			std::vector<SCARActionData> dataArr;
			auto scarClip = DataHandler::GetSCARDataClip(actor, &dataArr);
			if (scarClip) {
				DEBUG("Find SCAR Action Data in clip \"{}\" of \"{}\"", scarClip->animationName.c_str(), actor->GetName());
				std::sort(dataArr.begin(), dataArr.end(), SCARActionData::SortByWeight);
				for (const auto& data : dataArr) {
					if (data.PerformSCARAction(actor, combatTarg.get()))
						break;
				}
			}
		}

		return _ProcessEvent(a_sink, a_event, a_eventSource);
	}

}
