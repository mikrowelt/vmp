/*
 * Local ownership verification for the source-built legitimacy component.
 *
 * Replaces the closed-source legitimacy.dll ownership gate (MTL session ->
 * /ros/validate -> Rockstar). No network access of any kind is performed:
 * ownership is proven by either
 *   1. a cached DPAPI-protected ownership ticket (DigitalEntitlements), or
 *   2. local Steam API ownership of GTA V (appid 271590), or
 *   3. a developer-provided cl_ownershipTicket convar / VMP_OWNERSHIP_TICKET
 *      environment override.
 *
 * Portions ported from legacy ros-patches-five (LegitimacyChecking.cpp).
 */

#include <StdInc.h>

#include <KnownFolders.h>
#include <ShlObj.h>

#include <dpapi.h>

#include <LegitimacyAPI.h>

#include <Error.h>
#include <CoreConsole.h>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

// kept for DllGameComponent's adhesive sanity check (GetProcAddress on us)
extern "C" DLL_EXPORT void IDidntDoNothing()
{
}

// {38D8F400-AA8A-4784-A9F0-26A08628577E}
static const GUID CfxStorageGuid =
	{ 0x38d8f400, 0xaa8a, 0x4784, { 0xa9, 0xf0, 0x26, 0xa0, 0x86, 0x28, 0x57, 0x7e } };

#pragma comment(lib, "rpcrt4.lib")
#pragma comment(lib, "crypt32.lib")

std::string GetOwnershipPath()
{
	PWSTR appDataPath;
	HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &appDataPath);

	if (SUCCEEDED(hr))
	{
		std::string cfxPath = ToNarrow(appDataPath) + "\\DigitalEntitlements";
		if (!CreateDirectory(ToWide(cfxPath).c_str(), nullptr))
		{
			auto error = GetLastError();

			if (error != ERROR_ALREADY_EXISTS)
			{
				FatalError("CreateDirectory for %s failed: GetLastError = 0x%x\n\nMake sure your AppData folder is not write-protected.", cfxPath, error);
			}
		}

		CoTaskMemFree(appDataPath);

		RPC_CSTR str;
		UuidToStringA(&CfxStorageGuid, &str);

		cfxPath += "\\";
		cfxPath += (char*)str;

		RpcStringFreeA(&str);

		return cfxPath;
	}

	FatalError("SHGetKnownFolderPath for FOLDERID_LocalAppData failed: HRESULT = 0x%08x\n\nMake sure your AppData folder is not write-protected.", hr);
	return "";
}

static std::string g_entitlementSource;

__declspec(noinline) static void SetEntitlementSource(const std::string& entitlementSource)
{
	g_entitlementSource = entitlementSource;
}

__declspec(noinline) static bool HasEntitlementSource()
{
	return !g_entitlementSource.empty();
}

bool LoadOwnershipTicket()
{
	std::string filePath = GetOwnershipPath();

	FILE* f = _wfopen(ToWide(filePath).c_str(), L"rb");

	if (!f)
	{
		return false;
	}

	std::vector<uint8_t> fileData;
	int pos;

	// get the file length
	fseek(f, 0, SEEK_END);
	pos = ftell(f);
	fseek(f, 0, SEEK_SET);

	// resize the buffer
	fileData.resize(pos);

	// read the file and close it
	fread(&fileData[0], 1, pos, f);

	fclose(f);

	// decrypt the stored data - setup blob
	DATA_BLOB cryptBlob;
	cryptBlob.pbData = &fileData[0];
	cryptBlob.cbData = fileData.size();

	DATA_BLOB outBlob;

	// call DPAPI
	if (CryptUnprotectData(&cryptBlob, nullptr, nullptr, nullptr, nullptr, 0, &outBlob))
	{
		// parse the file
		std::string data(reinterpret_cast<char*>(outBlob.pbData), outBlob.cbData);

		// free the out data
		LocalFree(outBlob.pbData);

		rapidjson::Document doc;
		doc.Parse(data.c_str(), data.size());

		if (!doc.HasParseError())
		{
			if (doc.IsObject() && doc.HasMember("guid"))
			{
				SetEntitlementSource(doc["guid"].GetString());

				if (HasEntitlementSource())
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool SaveOwnershipTicket(const std::string& guid)
{
	rapidjson::Document doc;
	doc.SetObject();
	doc.AddMember("guid", rapidjson::Value(guid.c_str(), guid.size(), doc.GetAllocator()), doc.GetAllocator());

	rapidjson::StringBuffer sb;
	rapidjson::Writer<rapidjson::StringBuffer> w(sb);

	if (!doc.Accept(w))
	{
		return false;
	}

	// encrypt the actual string
	DATA_BLOB cryptBlob;
	cryptBlob.pbData = reinterpret_cast<uint8_t*>(const_cast<char*>(sb.GetString()));
	cryptBlob.cbData = sb.GetLength();

	DATA_BLOB outBlob;

	std::string filePath = GetOwnershipPath();

	FILE* f = _wfopen(ToWide(filePath).c_str(), L"wb");

	if (!f)
	{
		return false;
	}

	if (CryptProtectData(&cryptBlob, nullptr, nullptr, nullptr, nullptr, 0, &outBlob))
	{
		fwrite(outBlob.pbData, 1, outBlob.cbData, f);
		fclose(f);

		LocalFree(outBlob.pbData);
	}

	return true;
}

#include <steam/steam_api.h>

// GTA V Steam appid
constexpr AppId_t kGTAVAppId = 271590;

static bool VerifySteamOwnershipInternal(std::string* outSource)
{
	// set the app ID context for the Steam API
	SetEnvironmentVariableW(L"SteamAppId", fmt::sprintf(L"%d", kGTAVAppId).c_str());

	{
		struct deleter
		{
			~deleter()
			{
				_unlink("steam_appid.txt");
			}
		} deleter;

		FILE* f = fopen("steam_appid.txt", "w");

		if (f)
		{
			fprintf(f, "%d", kGTAVAppId);
			fclose(f);
		}

		if (!SteamAPI_Init())
		{
			trace("%s: SteamAPI_Init failed (Steam not running/signed in?)\n", __func__);
			return false;
		}
	}

	struct shutdown
	{
		~shutdown()
		{
			SteamAPI_Shutdown();
		}
	} shutdown;

	if (!SteamApps())
	{
		trace("%s: no ISteamApps interface\n", __func__);
		return false;
	}

	// local subscription check
	if (!SteamApps()->BIsSubscribedApp(kGTAVAppId))
	{
		trace("%s: BIsSubscribedApp(%d) is false\n", __func__, kGTAVAppId);
		return false;
	}

	// verify the license owner matches the current user (family sharing lends,
	// but still proves *some* ownership relationship; accept either way but log)
	auto owner = SteamApps()->GetAppOwner();
	auto self = SteamUser()->GetSteamID();

	trace("%s: GTA V owned via Steam (owner %lld, user %lld)\n", __func__, owner.ConvertToUint64(), self.ConvertToUint64());

	*outSource = fmt::sprintf("steam:%lld", self.ConvertToUint64());

	return true;
}

bool VerifySteamOwnership()
{
	try
	{
		std::string source;

		if (VerifySteamOwnershipInternal(&source))
		{
			SetEntitlementSource(source);
			return true;
		}
	}
	catch (const std::exception& e)
	{
		trace("%s: exception: %s\n", __func__, e.what());
	}

	return false;
}

bool LegitimateCopy()
{
	if (LoadOwnershipTicket())
	{
		trace("%s: ownership proven by cached DPAPI ticket\n", __func__);
		return true;
	}

	if (VerifySteamOwnership() && SaveOwnershipTicket(ros::GetEntitlementSource()))
	{
		trace("%s: ownership proven by Steam, ticket cached\n", __func__);
		return true;
	}

	return false;
}

namespace ros
{
	__declspec(noinline) std::string GetEntitlementSource()
	{
		return g_entitlementSource;
	}

	__declspec(noinline) std::string GetApiIdentifier()
	{
		return g_entitlementSource;
	}
}

void LoadOwnershipEarly()
{
	// developer override: seed the ownership ticket from an env var
	if (auto ticketEnv = getenv("VMP_OWNERSHIP_TICKET"))
	{
		if (ticketEnv[0] != '\0')
		{
			SaveOwnershipTicket(ticketEnv);
		}
	}
}

static InitFunction initFunction([]()
{
	LoadOwnershipTicket();
});

static HookFunction hookFunction([]()
{
	LoadOwnershipTicket();
});
