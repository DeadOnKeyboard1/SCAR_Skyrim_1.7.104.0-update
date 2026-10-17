#include "DataHandler.h"
#include <SimpleIni.h>

namespace SCAR
{
	DataHandler::Settings::Settings()
	{
		*enableDebugLog = false;
		*enableDebugOverlay = true;
		CSimpleIniA ini;
		ini.SetUnicode();
		if (ini.LoadFile("Data/SKSE/Plugins/SCAR.ini") < 0) {
			WARN("SCAR.ini unavailable; retaining defaults");
		} else {
			for (const auto setting : {&enableDebugLog, &enableDebugOverlay}) {
				if (const auto raw = ini.GetValue("Debug", setting->get_key())) {
					const auto value = ActionRules::DebugFlag(raw);
					if (value) **setting = *value;
					else WARN("Invalid SCAR.ini setting {}; retaining default", setting->get_key());
				}
			}
		}

		PrintSettingValue(enableDebugLog);
		PrintSettingValue(enableDebugOverlay);
	}

	DataHandler::DataHandler()
	{
		precisionPtr = reinterpret_cast<PRECISION_API::IVPrecision1*>(PRECISION_API::RequestPluginAPI());
		settings = std::make_unique<Settings>();
	}

	static inline RE::hkbClipGenerator* ToClipGenerator(RE::hkbNode* a_node)
	{
		constexpr char CLASS_NAME[] = "hkbClipGenerator";

		const auto type = a_node ? a_node->GetClassType() : nullptr;
		if (type && type->name) {
			if (_strcmpi(type->name, CLASS_NAME) == 0)
				return skyrim_cast<RE::hkbClipGenerator*>(a_node);
		}

		return nullptr;
	}

	std::optional<SCARActionData> DataHandler::ParseAnnotation(std::string_view a_text, float a_weight)
	{
		constexpr std::string_view prefix = "SCAR_ActionData";
		if (!a_text.starts_with(prefix)) return std::nullopt;
		try {
			auto action = json::parse(a_text.substr(prefix.size())).get<SCARActionData>();
			if (!std::isfinite(a_weight)) throw std::invalid_argument("Non-finite SCAR weight");
			action.weight = a_weight;
			DEBUG("Get a Action Data, Name : {}, Distance: {}-{}, Angle: {}-{}, Chance: {}, Type: {}, Weight {}",
				action.IdleAnimationEditorID, action.minDistance, action.maxDistance, action.startAngle, action.endAngle, action.chance, action.actionType, action.weight);
			return action;
		} catch (const json::exception&) {
			WARN("Invalid SCAR annotation JSON; action ignored");
		} catch (const std::invalid_argument&) {
			WARN("Invalid SCAR annotation value; action ignored");
		}
		return std::nullopt;
	}

	std::vector<SCARActionData> DataHandler::GetSCARActionData(const RE::hkbClipGenerator* a_clip)
	{
		std::vector<SCARActionData> dataArr;

		auto binding = a_clip ? a_clip->binding : nullptr;
		auto animation = binding ? binding->animation : nullptr;

		if (!animation || animation->annotationTracks.empty())
			return dataArr;

		for (const auto& track : animation->annotationTracks) {
			for (const auto& anno : track.annotations) {
				const auto raw = anno.text.c_str();
				if (!raw) continue;
				if (auto action = ParseAnnotation(raw, anno.time)) dataArr.push_back(std::move(*action));
			}
		}

		return dataArr;
	}

	const RE::hkbClipGenerator* DataHandler::GetSCARDataClip(RE::Actor* a_actor, std::vector<SCARActionData>* a_actions)
	{
		if (a_actions) a_actions->clear();
		if (!a_actor)
			return nullptr;

		RE::BSAnimationGraphManagerPtr graphMgr;
		if (a_actor->GetAnimationGraphManager(graphMgr) && graphMgr && !graphMgr->graphs.empty()) {
			auto behaviourGraph = graphMgr->graphs[0] ? graphMgr->graphs[0]->behaviorGraph : nullptr;
			auto activeNodes = behaviourGraph ? behaviourGraph->activeNodes : nullptr;
			if (activeNodes) {
				for (const auto& nodeInfo : *activeNodes) {
					auto nodeClone = nodeInfo.nodeClone;
					if (nodeClone) {
						auto clipGenrator = ToClipGenerator(nodeClone);
						if (clipGenrator) {
							auto binding = clipGenrator->binding;
							auto animation = binding ? binding->animation : nullptr;
							if (animation && !animation->annotationTracks.empty()) {
								if (a_actions) {
									// A malformed clip must not hide later valid clips or vanilla attacks.
									*a_actions = GetSCARActionData(clipGenrator);
									if (!a_actions->empty()) return clipGenrator;
								} else {
									for (const auto& track : animation->annotationTracks) {
										for (const auto& anno : track.annotations) {
											const auto raw = anno.text.c_str();
											if (raw && std::string_view(raw).starts_with("SCAR_ActionData")) return clipGenrator;
										}
									}
								}
							}
						}
					}
				}
			}
		}

		return nullptr;
	}
}
