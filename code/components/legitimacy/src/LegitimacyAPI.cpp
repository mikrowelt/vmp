/*
 * Implementation of the SharedLegitimacyAPI surface (cfx::legitimacy) that
 * the closed-source legitimacy.dll used to provide.
 *
 * Steam integration is local-only (steam_api64.dll shipped in the app).
 * cfx.re account-linking endpoints (Discord/Discourse auth) are deliberately
 * not reimplemented: they degrade gracefully with a failure callback.
 */

#include <StdInc.h>

// define the API functions as exports (the shared header declares them as
// DLL_IMPORT for consumers)
#undef DLL_IMPORT
#define DLL_IMPORT __declspec(dllexport)

#include <SharedLegitimacyAPI.h>

#include <Error.h>

#include "SteamFlat.h"

// GTA V Steam appid
constexpr uint32_t kGTAVAppId = 271590;

static bool g_steamInitAttempted = false;
static bool g_steamInitialized = false;

static void EnsureSteamInit()
{
	if (g_steamInitAttempted)
	{
		return;
	}

	g_steamInitAttempted = true;

	// steam_api64.dll is preloaded by the launcher from the app directory;
	// set the app ID context if not already present
	if (GetEnvironmentVariableW(L"SteamAppId", nullptr, 0) == 0)
	{
		SetEnvironmentVariableW(L"SteamAppId", fmt::sprintf(L"%d", kGTAVAppId).c_str());

		FILE* f = fopen("steam_appid.txt", "w");

		if (f)
		{
			fprintf(f, "%d", kGTAVAppId);
			fclose(f);
		}
	}

	g_steamInitialized = steamflat::Init();

	trace("%s: steam init -> %d\n", __func__, g_steamInitialized);
}

namespace cfx::legitimacy
{
void AuthenticateDiscord(const char* userId, const char* code, AuthCallback callback)
{
	// cfx.re account linking is not available; degrade gracefully
	callback(false, "", 0);
}

void AuthenticateDiscourse(const char* clientId, const char* authToken, AuthCallback callback)
{
	// cfx.re account linking is not available; degrade gracefully
	callback(false, "", 0);
}

bool ShouldProcessHeaders(const char* hostname)
{
	// no cfx.re hosts to attach auth headers to
	return false;
}

void ProcessHeaders(char*, char*)
{
}

void InitSteamSDKConnection()
{
	EnsureSteamInit();
}

bool IsSteamRunning()
{
	return steamflat::IsSteamRunning();
}

bool IsSteamInitializedWrapper()
{
	EnsureSteamInit();

	return g_steamInitialized;
}

void GetSteamAuthTicketWrapper(const std::function<void(std::pair<std::string, std::string>)>& callback, bool enforceSteamAuth)
{
	EnsureSteamInit();

	if (!g_steamInitialized)
	{
		callback({ "", "" });
		return;
	}

	auto ticket = steamflat::GetAuthSessionTicketHex();

	if (ticket.empty())
	{
		trace("%s: GetAuthSessionTicket returned an empty ticket\n", __func__);
	}

	callback({ "", ticket });
}

uint64_t GetSteamIdAsIntWrapper()
{
	EnsureSteamInit();

	if (!g_steamInitialized)
	{
		return 0;
	}

	return steamflat::GetSteamId();
}

std::string GetSteamUsernameWrapper()
{
	EnsureSteamInit();

	if (!g_steamInitialized)
	{
		return "";
	}

	return steamflat::GetPersonaName();
}

void SetSteamRichPresenceWrapper(std::string key, std::string value)
{
	EnsureSteamInit();

	if (!g_steamInitialized)
	{
		return;
	}

	steamflat::SetRichPresence(key.c_str(), value.c_str());
}

void ResetSteamRichPresenceWrapper()
{
	EnsureSteamInit();

	if (!g_steamInitialized)
	{
		return;
	}

	steamflat::ClearRichPresence();
}

bool SetSteamAppId(bool legacy)
{
	// we always run under the GTA V app ID and never require an app switch
	return false;
}

void WaitForAppSwitchWrapper()
{
	// no app switch is ever requested
}
}

// ---------------------------------------------------------------------------
// misc exports provided by the closed blob
// ---------------------------------------------------------------------------

extern uint64_t ROSGetDummyAccountID();

extern "C" DLL_EXPORT uint64_t GetAccountID()
{
	return ROSGetDummyAccountID();
}
