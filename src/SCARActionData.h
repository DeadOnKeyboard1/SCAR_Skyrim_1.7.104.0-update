#pragma once
#include "nlohmann/json.hpp"
#include "ActionRules.h"

namespace SCAR
{
	using json = nlohmann::json;

	using DefaultObject = RE::BGSDefaultObjectManager::DefaultObject;

	struct SCARActionData
	{
	private:
		float weight = 0.f;

		std::string IdleAnimationEditorID = "";

		float minDistance = 0.f;

		float maxDistance = 150.f;

		float startAngle = -60.f;

		float endAngle = 60.f;

		float chance = 100.f;

		std::string actionType = "RA";

		std::optional<float> weaponLength;

	public:
		friend void from_json(const json& j, SCARActionData& a_data);
		friend class DataHandler;

		bool PerformSCARAction(RE::Actor* a_attacker, RE::Actor* a_target) const;

		static bool SortByWeight(const SCARActionData& a_data1, const SCARActionData& a_data2);

	private:
		const DefaultObject GetActionObject() const;
		_NODISCARD const float GetStartAngle() const { return static_cast<float>(startAngle / 180.f * std::numbers::pi); };
		_NODISCARD const float GetEndAngle() const { return static_cast<float>(endAngle / 180.f * std::numbers::pi); };

		_NODISCARD const bool IsLeftAttack() const;
		_NODISCARD const bool IsBashAttack() const;
		_NODISCARD float GetWeaponReach(RE::Actor* a_attacker) const;
	};

	void from_json(const json& j, SCARActionData& a_data);
}
