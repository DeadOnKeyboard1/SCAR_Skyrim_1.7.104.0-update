#include "LoadGame.h"
#include "DataHandler.h"
#include "DebugAPI/DebugAPI.h"
#include "Hook_AttackCombo.h"
#include "Hook_AttackStart.h"
#include "NativeValidation.h"

namespace SCAR
{
	static bool hooksInstalled{};
	static bool overlayRegistered{};

	void EventCallback(SKSE::MessagingInterface::Message* msg)
	{
		if (!msg) return;
		if (msg->type == SKSE::MessagingInterface::kPostLoad) {
			static bool attempted{};
			if (attempted) return;
			attempted = true;
			if (!NativeValidation::Check()) {
				ERROR("SCAR hook conflict or unsupported instructions; no hooks installed");
				return;
			}
			// CommonLib initializes the pool once; later requests cannot grow it.
			// Angle/action calls need two 14-byte stubs in addition to the cave.
			SKSE::AllocTrampoline(AttackDistancHook::TrampolineBytes + 2 * 14);
			// For First Attack
			if (!AttackDistancHook::Install()) return;
			AttackAngleHook::InstallHook();
			AttackActionHook::InstallHook();

			// For Combo
			AnimEventHook::InstallHook();
			hooksInstalled = true;

			auto dataHandler = SCAR::DataHandler::GetSingleton();
			if (dataHandler->precisionPtr) {
				INFO("Obtained PrecisionAPI - {:x}", AsAddress(dataHandler->precisionPtr));
			}

			if (dataHandler->settings->enableDebugLog.get_data()) {
				spdlog::set_level(spdlog::level::debug);
				DEBUG("Enable Debug Log!");
			}

			if (dataHandler->settings->enableDebugOverlay.get_data()) {
				DEBUG("Enable Debug Overlay!");
			}
		} else if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
			if (hooksInstalled && !overlayRegistered && DataHandler::GetSingleton()->settings->enableDebugOverlay.get_data()) {
				DebugOverlayMenu::Register();
				overlayRegistered = true;
			}
		}
	}

}
