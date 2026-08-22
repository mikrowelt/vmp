#include <StdInc.h>
#include "SteamFlat.h"

// ---------------------------------------------------------------------------
// dynamic imports from steam_api64.dll (flat API only)
// ---------------------------------------------------------------------------

typedef bool (__cdecl *SteamInternal_SteamAPI_Init_t)(const char*, void*);
typedef void (__cdecl *SteamAPI_Shutdown_t)();
typedef bool (__cdecl *SteamAPI_IsSteamRunning_t)();

typedef void* (__cdecl *SteamAPI_SteamApps_v008_t)();
typedef void* (__cdecl *SteamAPI_SteamUser_v023_t)();
typedef void* (__cdecl *SteamAPI_SteamFriends_v018_t)();

typedef bool (__cdecl *SteamAPI_ISteamApps_BIsSubscribedApp_t)(void*, uint32_t);
typedef uint64_t (__cdecl *SteamAPI_ISteamApps_GetAppOwner_t)(void*);
typedef uint64_t (__cdecl *SteamAPI_ISteamUser_GetSteamID_t)(void*);
typedef uint32_t (__cdecl *SteamAPI_ISteamUser_GetAuthSessionTicket_t)(void*, void*, int, uint32_t*, const void*);
typedef const char* (__cdecl *SteamAPI_ISteamFriends_GetPersonaName_t)(void*);
typedef bool (__cdecl *SteamAPI_ISteamFriends_SetRichPresence_t)(void*, const char*, const char*);
typedef void (__cdecl *SteamAPI_ISteamFriends_ClearRichPresence_t)(void*);

#define STEAM_FN(name) static name##_t p_##name = nullptr

STEAM_FN(SteamInternal_SteamAPI_Init);
STEAM_FN(SteamAPI_Shutdown);
STEAM_FN(SteamAPI_IsSteamRunning);
STEAM_FN(SteamAPI_SteamApps_v008);
STEAM_FN(SteamAPI_SteamUser_v023);
STEAM_FN(SteamAPI_SteamFriends_v018);
STEAM_FN(SteamAPI_ISteamApps_BIsSubscribedApp);
STEAM_FN(SteamAPI_ISteamApps_GetAppOwner);
STEAM_FN(SteamAPI_ISteamUser_GetSteamID);
STEAM_FN(SteamAPI_ISteamUser_GetAuthSessionTicket);
STEAM_FN(SteamAPI_ISteamFriends_GetPersonaName);
STEAM_FN(SteamAPI_ISteamFriends_SetRichPresence);
STEAM_FN(SteamAPI_ISteamFriends_ClearRichPresence);

static bool g_steamModuleBound = false;
static bool g_steamInitialized = false;

static bool BindSteamModule()
{
	if (g_steamModuleBound)
	{
		return true;
	}

	// the launcher preloads steam_api64.dll from the app directory
	HMODULE steam = GetModuleHandleW(L"steam_api64.dll");

	if (!steam)
	{
		steam = LoadLibraryW(MakeRelativeCitPath(L"steam_api64.dll").c_str());
	}

	if (!steam)
	{
		trace("steamflat: steam_api64.dll not available\n");
		return false;
	}

	bool ok = true;

	auto bind = [&](FARPROC* out, const char* name)
	{
		*out = GetProcAddress(steam, name);

		if (!*out)
		{
			trace("steamflat: missing export %s\n", name);
			ok = false;
		}
	};

	bind((FARPROC*)&p_SteamInternal_SteamAPI_Init, "SteamInternal_SteamAPI_Init");
	bind((FARPROC*)&p_SteamAPI_Shutdown, "SteamAPI_Shutdown");
	bind((FARPROC*)&p_SteamAPI_IsSteamRunning, "SteamAPI_IsSteamRunning");
	bind((FARPROC*)&p_SteamAPI_SteamApps_v008, "SteamAPI_SteamApps_v008");
	bind((FARPROC*)&p_SteamAPI_SteamUser_v023, "SteamAPI_SteamUser_v023");
	bind((FARPROC*)&p_SteamAPI_SteamFriends_v018, "SteamAPI_SteamFriends_v018");
	bind((FARPROC*)&p_SteamAPI_ISteamApps_BIsSubscribedApp, "SteamAPI_ISteamApps_BIsSubscribedApp");
	bind((FARPROC*)&p_SteamAPI_ISteamApps_GetAppOwner, "SteamAPI_ISteamApps_GetAppOwner");
	bind((FARPROC*)&p_SteamAPI_ISteamUser_GetSteamID, "SteamAPI_ISteamUser_GetSteamID");
	bind((FARPROC*)&p_SteamAPI_ISteamUser_GetAuthSessionTicket, "SteamAPI_ISteamUser_GetAuthSessionTicket");
	bind((FARPROC*)&p_SteamAPI_ISteamFriends_GetPersonaName, "SteamAPI_ISteamFriends_GetPersonaName");
	bind((FARPROC*)&p_SteamAPI_ISteamFriends_SetRichPresence, "SteamAPI_ISteamFriends_SetRichPresence");
	bind((FARPROC*)&p_SteamAPI_ISteamFriends_ClearRichPresence, "SteamAPI_ISteamFriends_ClearRichPresence");

	g_steamModuleBound = ok;

	return ok;
}

namespace steamflat
{
	bool Init()
	{
		if (g_steamInitialized)
		{
			return true;
		}

		if (!BindSteamModule())
		{
			return false;
		}

		// empty version string: skip interface version check
		g_steamInitialized = p_SteamInternal_SteamAPI_Init("", nullptr);

		trace("steamflat: SteamInternal_SteamAPI_Init -> %d\n", g_steamInitialized);

		return g_steamInitialized;
	}

	void Shutdown()
	{
		if (g_steamInitialized && p_SteamAPI_Shutdown)
		{
			p_SteamAPI_Shutdown();
			g_steamInitialized = false;
		}
	}

	bool IsInitialized()
	{
		return g_steamInitialized;
	}

	bool IsSteamRunning()
	{
		if (!BindSteamModule())
		{
			return false;
		}

		return p_SteamAPI_IsSteamRunning();
	}

	bool IsSubscribedApp(uint32_t appId)
	{
		void* apps = p_SteamAPI_SteamApps_v008();

		if (!apps)
		{
			return false;
		}

		return p_SteamAPI_ISteamApps_BIsSubscribedApp(apps, appId);
	}

	uint64_t GetAppOwner()
	{
		void* apps = p_SteamAPI_SteamApps_v008();

		if (!apps)
		{
			return 0;
		}

		return p_SteamAPI_ISteamApps_GetAppOwner(apps);
	}

	uint64_t GetSteamId()
	{
		void* user = p_SteamAPI_SteamUser_v023();

		if (!user)
		{
			return 0;
		}

		return p_SteamAPI_ISteamUser_GetSteamID(user);
	}

	std::string GetAuthSessionTicketHex()
	{
		void* user = p_SteamAPI_SteamUser_v023();

		if (!user)
		{
			return "";
		}

		static uint8_t ticket[16384] = { 0 };
		uint32_t ticketLength = 0;

		p_SteamAPI_ISteamUser_GetAuthSessionTicket(user, ticket, sizeof(ticket), &ticketLength, nullptr);

		if (ticketLength == 0)
		{
			return "";
		}

		static const char* hex = "0123456789abcdef";
		std::string out;
		out.reserve(ticketLength * 2);

		for (uint32_t i = 0; i < ticketLength; i++)
		{
			out.push_back(hex[(ticket[i] >> 4) & 0xF]);
			out.push_back(hex[ticket[i] & 0xF]);
		}

		return out;
	}

	std::string GetPersonaName()
	{
		void* friends = p_SteamAPI_SteamFriends_v018();

		if (!friends)
		{
			return "";
		}

		const char* name = p_SteamAPI_ISteamFriends_GetPersonaName(friends);

		return name ? name : "";
	}

	void SetRichPresence(const char* key, const char* value)
	{
		void* friends = p_SteamAPI_SteamFriends_v018();

		if (!friends)
		{
			return;
		}

		p_SteamAPI_ISteamFriends_SetRichPresence(friends, key, value);
	}

	void ClearRichPresence()
	{
		void* friends = p_SteamAPI_SteamFriends_v018();

		if (!friends)
		{
			return;
		}

		p_SteamAPI_ISteamFriends_ClearRichPresence(friends);
	}
}
