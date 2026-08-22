/*
 * Minimal dynamic binding to the modern (flat) Steam API exported by the
 * steam_api64.dll shipped in the app payload (MTL redistribution, which no
 * longer exports the classic SteamAPI_Init entry point).
 */

#pragma once

#include <cstdint>
#include <string>

namespace steamflat
{
	// one-time init; returns true if the Steam API is usable
	bool Init();

	void Shutdown();

	bool IsInitialized();
	bool IsSteamRunning();

	bool IsSubscribedApp(uint32_t appId);
	uint64_t GetAppOwner();
	uint64_t GetSteamId();

	// returns hex-encoded auth session ticket, or "" on failure
	std::string GetAuthSessionTicketHex();

	std::string GetPersonaName();
	void SetRichPresence(const char* key, const char* value);
	void ClearRichPresence();
}
