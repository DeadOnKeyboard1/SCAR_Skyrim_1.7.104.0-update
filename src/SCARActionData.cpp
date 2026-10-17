#include "SCARActionData.h"
#include "DataHandler.h"
#include "Function.h"

namespace SCAR
{
	void from_json(const json& j, SCARActionData& a_data)
	{
		j.at("IdleAnimation").get_to(a_data.IdleAnimationEditorID);
		if (a_data.IdleAnimationEditorID.empty()) throw std::invalid_argument("Empty SCAR idle name");

		a_data.minDistance = ActionRules::Number(j, "MinDistance");
		a_data.minDistance = std::max(a_data.minDistance, 0.f);

		a_data.maxDistance = ActionRules::Number(j, "MaxDistance");
		a_data.maxDistance = std::max(a_data.maxDistance, 0.f);

		a_data.startAngle = ActionRules::Number(j, "StartAngle");
		a_data.startAngle = std::clamp(a_data.startAngle, -180.f, 180.f);

		a_data.endAngle = ActionRules::Number(j, "EndAngle");
		a_data.endAngle = std::clamp(a_data.endAngle, -180.f, 180.f);

		a_data.chance = std::clamp(ActionRules::Number(j, "Chance"), 0.f, 100.f);

		j.at("Type").get_to(a_data.actionType);
		a_data.actionType = ActionRules::Type(std::move(a_data.actionType));

		if (j.find("WeaponLength") != j.end())
			a_data.weaponLength.emplace(std::max(ActionRules::Number(j, "WeaponLength"), 0.f));
	}

	const bool SCARActionData::IsLeftAttack() const
	{
		return _strcmpi(actionType.c_str(), "LA") == 0 || _strcmpi(actionType.c_str(), "LPA") == 0;
	}

	const bool SCARActionData::IsBashAttack() const
	{
		return _strcmpi(actionType.c_str(), "BA") == 0 || _strcmpi(actionType.c_str(), "BPA") == 0;
	}

	float SCARActionData::GetWeaponReach(RE::Actor* a_attacker) const
	{
		if (a_attacker) {
			if (weaponLength.has_value()) {
				return weaponLength.value() * a_attacker->GetScale();
			}

			if (IsBashAttack()) {
				const auto settings = RE::GameSettingCollection::GetSingleton();
				auto setting = settings ? settings->GetSetting("fCombatBashReach") : nullptr;
				if (setting)
					return setting->GetFloat() * a_attacker->GetScale();
			} else {
				const bool leftAttack = IsLeftAttack();
				auto dataHandler = DataHandler::GetSingleton();
				if (dataHandler && dataHandler->precisionPtr) {
					auto collisionType = leftAttack ? RequestedAttackCollisionType::LeftWeapon : RequestedAttackCollisionType::RightWeapon;
					const auto length = dataHandler->precisionPtr->GetAttackCollisionCapsuleLength(a_attacker->GetHandle(), collisionType);
					if (std::isfinite(length) && length > 0.f) return length + a_attacker->GetBoundRadius();
				}
				{
					auto obj = a_attacker->GetEquippedObject(leftAttack);
					auto weap = obj ? obj->As<RE::TESObjectWEAP>() : nullptr;
					const auto settings = RE::GameSettingCollection::GetSingleton();
					auto setting = settings ? settings->GetSetting("fCombatDistance") : nullptr;
					if (weap && setting) {
						return weap->GetReach() * setting->GetFloat() * a_attacker->GetScale();
					}
				}
			}

			return a_attacker->GetRace() ? a_attacker->GetRace()->data.unarmedReach : 0.f;
		}

		return 0.f;
	}

	const DefaultObject SCARActionData::GetActionObject() const
	{
		static std::map<const std::string, const DefaultObject> actionMap = {
			{ "RA", DefaultObject::kActionRightAttack },
			{ "RPA", DefaultObject::kActionRightPowerAttack },
			{ "LA", DefaultObject::kActionLeftAttack },
			{ "LPA", DefaultObject::kActionLeftPowerAttack },
			{ "DA", DefaultObject::kActionDualAttack },
			{ "DPA", DefaultObject::kActionDualPowerAttack },
			{ "BA", DefaultObject::kActionRightAttack },
			{ "BPA", DefaultObject::kActionRightPowerAttack },
			{ "IDLE", DefaultObject::kActionIdle }
		};

		auto itr = actionMap.find(actionType);
		return itr != actionMap.end() ? itr->second : DefaultObject::kActionRightAttack;
	}

	bool SCARActionData::PerformSCARAction(RE::Actor* a_attacker, RE::Actor* a_target) const
	{
		if (!a_attacker || !a_target || !a_attacker->GetActorRuntimeData().currentProcess)
			return false;

		const float weaponReach = GetWeaponReach(a_attacker);
		if (ActionRules::ChancePasses(chance, Random::get<float>(0.f, 100.f)) && AttackRangeCheck::WithinAttackRange(a_attacker, a_target, maxDistance + weaponReach, minDistance, GetStartAngle(), GetEndAngle())) {
			auto IdleAnimation = RE::TESForm::LookupByEditorID<RE::TESIdleForm>(IdleAnimationEditorID);
			if (!IdleAnimation) {
				WARN("SCAR idle form unavailable; action ignored");
				return false;
			}

			auto result = a_attacker->GetActorRuntimeData().currentProcess->SetupSpecialIdle(a_attacker, GetActionObject(), IdleAnimation, true, true, a_target);
			if (result) {
				DEBUG("Perform SCAR Action! Name : {}, Distance: {}-{}, Angle: {}-{}, Chance: {}, Type: {}, Weight {}",
					IdleAnimationEditorID, minDistance, maxDistance, startAngle, endAngle, chance, actionType, weight);
				AttackRangeCheck::DrawOverlay(a_attacker, a_target, maxDistance + weaponReach, minDistance, GetStartAngle(), GetEndAngle());
			} else
				DEBUG("Play Idle Fail! Name : {}, Distance: {}-{}, Angle: {}-{}, Chance: {}, Type: {}, Weight {}",
					IdleAnimationEditorID, minDistance, maxDistance, startAngle, endAngle, chance, actionType, weight);

			return result;
		}

		return false;
	}

	bool SCARActionData::SortByWeight(const SCARActionData& a_data1, const SCARActionData& a_data2)
	{
		return a_data1.weight > a_data2.weight;
	}
}
