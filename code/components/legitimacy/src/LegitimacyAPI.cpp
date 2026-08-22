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

#include <steam/steam_api.h>

#include <Error.h>

// GTA V Steam appid
constexpr AppId_t kGTAVAppId = 271590;

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

	g_steamInitialized = SteamAPI_Init();

	trace("%s: SteamAPI_Init -> %d\n", __func__, g_steamInitialized);
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
	return SteamAPI_IsSteamRunning();
}

bool IsSteamInitializedWrapper()
{
	EnsureSteamInit();

	return g_steamInitialized;
}

static void ToHex(const unsigned char* in, size_t insz, char* out, size_t outsz)
{
	const unsigned char* pin = in;
	const char* hex = "0123456789abcdef";
	char* pout = out;

	for (; pin < in + insz; pout += 2, pin++)
	{
		pout[0] = hex[(*pin >> 4) & 0xF];
		pout[1] = hex[*pin & 0xF];

		if (pout + 3 - out > outsz)
		{
			break;
		}
	}

	pout[0] = 0;
}

void GetSteamAuthTicketWrapper(const std::function<void(std::pair<std::string, std::string>)>& callback, bool enforceSteamAuth)
{
	EnsureSteamInit();

	if (!g_steamInitialized)
	{
		callback({ "", "" });
		return;
	}

	static uint8_t ticket[16384] = { 0 };
	uint32_t ticketLength = 0;

	SteamUser()->GetAuthSessionTicket(ticket, sizeof(ticket), &ticketLength);

	if (ticketLength == 0)
	{
		trace("%s: GetAuthSessionTicket returned an empty ticket\n", __func__);
		callback({ "", "" });
		return;
	}

	static char outHex[16384 * 2];
	ToHex(ticket, ticketLength, outHex, sizeof(outHex));

	callback({ "", outHex });
}

uint64_t GetSteamIdAsIntWrapper()
{
	EnsureSteamInit();

	if (!g_steamInitialized)
	{
		return 0;
	}

	return SteamUser()->GetSteamID().ConvertToUint64();
}

std::string GetSteamUsernameWrapper()
{
	EnsureSteamInit();

	if (!g_steamInitialized)
	{
		return "";
	}

	const char* name = SteamFriends()->GetPersonaName();

	return name ? name : "";
}

void SetSteamRichPresenceWrapper(std::string key, std::string value)
{
	EnsureSteamInit();

	if (!g_steamInitialized)
	{
		return;
	}

	SteamFriends()->SetRichPresence(key.c_str(), value.c_str());
}

void ResetSteamRichPresenceWrapper()
{
	EnsureSteamInit();

	if (!g_steamInitialized)
	{
		return;
	}

	SteamFriends()->ClearRichPresence();
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
