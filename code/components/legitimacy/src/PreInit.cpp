/*
 * Component pre-init (ownership gate) and in-game SC-related hooks.
 *
 * Ported from legacy ros-patches-five (LauncherTool.cpp), with the entire
 * ROS/MTL subprocess launcher flow removed: ownership is verified locally
 * (see Ownership.cpp) and no ros:legit/ros:launcher subprocess is ever
 * spawned.
 */

#include "StdInc.h"

#include <Error.h>

#include <CL2LaunchMode.h>
#include <LaunchMode.h>
#include <MinHook.h>

#include <CrossBuildRuntime.h>

#include "Hooking.h"
#include "Hooking.Aux.h"

#include <HostSharedData.h>
#include <CfxState.h>

#include <shlobj.h>
#include <wincrypt.h>

// ---------------------------------------------------------------------------
// Launcher-skip knobs (referenced by ros/LoopbackTcpServer.cpp)
// ---------------------------------------------------------------------------

bool CanSafelySkipLauncher()
{
	// The ROS/MTL web flow no longer works with Rockstar's current launcher
	// web app, and it is not required: the game loads GTA5.exe in-process and
	// every ROS endpoint it talks to is stubbed locally by this component.
	return true;
}

void SetCanSafelySkipLauncher(bool value)
{
	// no-op: the launcher is unconditionally skipped
}

// ---------------------------------------------------------------------------
// Ownership gate
// ---------------------------------------------------------------------------

bool LegitimateCopy();
void LoadOwnershipEarly();
void OnPreInitHook();

void Component_RunPreInit()
{
	LoadOwnershipEarly();
	OnPreInitHook();

	// no ownership gate in tool-mode subprocesses (ros:service etc.)
	if (getenv("CitizenFX_ToolMode") != nullptr && getenv("CitizenFX_ToolMode")[0] != 0)
	{
		return;
	}

	static HostSharedData<CfxState> hostData("CfxInitState");

	if (!hostData->IsMasterProcess())
	{
		return;
	}

	trace("legitimacy: verifying local ownership...\n");

	if (!LegitimateCopy())
	{
		FatalError(
			"Could not verify ownership of Grand Theft Auto V.\n\n"
			"Ownership is verified locally (no Rockstar services are contacted). "
			"To pass this check, either:\n"
			"- sign into Steam with an account that owns GTA V, or\n"
			"- launch the game once via the Rockstar Games Launcher on this machine, or\n"
			"- set the VMP_OWNERSHIP_TICKET environment variable to a valid ownership ticket.");
	}

	trace("legitimacy: ownership verified locally.\n");
}

// ---------------------------------------------------------------------------
// In-game hooks (SC/launcher compatibility)
// ---------------------------------------------------------------------------

static HICON hIcon;

static InitFunction iconFunction([]()
{
	hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(1));
});

static HICON WINAPI LoadIconStub(HINSTANCE, LPCSTR)
{
	return hIcon;
}

static HLOCAL WINAPI LocalFreeStub(HLOCAL hMem)
{
	if (hMem && strstr((char*)hMem, "Entrust "))
	{
		return NULL;
	}

	return LocalFree(hMem);
}

extern HRESULT WINAPI __stdcall CoCreateInstanceStub(_In_ REFCLSID rclsid, _In_opt_ LPUNKNOWN pUnkOuter, _In_ DWORD dwClsContext, _In_ REFIID riid, _COM_Outptr_ _At_(*ppv, _Post_readable_size_(_Inexpressible_(varies))) LPVOID FAR* ppv);
extern BOOL WINAPI __stdcall CreateProcessAStub(_In_opt_ LPCSTR lpApplicationName, _Inout_opt_ LPSTR lpCommandLine, _In_opt_ LPSECURITY_ATTRIBUTES lpProcessAttributes, _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes, _In_ BOOL bInheritHandles, _In_ DWORD dwCreationFlags, _In_opt_ LPVOID lpEnvironment, _In_opt_ LPCSTR lpCurrentDirectory, _In_ LPSTARTUPINFOA lpStartupInfo, _Out_ LPPROCESS_INFORMATION lpProcessInformation);

static DWORD WINAPI CertGetNameStringStubA(_In_ PCCERT_CONTEXT pCertContext, _In_ DWORD dwType, _In_ DWORD dwFlags, _In_opt_ void* pvTypePara, _Out_writes_to_opt_(cchNameString, return) LPSTR pszNameString, _In_ DWORD cchNameString)
{
	return CertGetNameStringA(pCertContext, dwType, dwFlags, pvTypePara, pszNameString, cchNameString);
}

#include <winsock2.h>
#include <iphlpapi.h>

DWORD _stdcall NotifyIpInterfaceChangeFake(_In_ ADDRESS_FAMILY Family, _In_ void* Callback, _In_opt_ PVOID CallerContext, _In_ BOOLEAN InitialNotification, _Inout_ HANDLE* NotificationHandle)
{
	*NotificationHandle = NULL;
	return NO_ERROR;
}

static InitFunction initFunctionF([]()
{
	DisableToolHelpScope scope;

	// #TODORDR: this hangs on pre-20H1 Windows in a chain from WinVerifyTrust leading to an infinite wait??
	MH_Initialize();
	MH_CreateHookApi(L"iphlpapi.dll", "NotifyIpInterfaceChange", NotifyIpInterfaceChangeFake, NULL);
	MH_EnableHook(MH_ALL_HOOKS);
});

static HookFunction hookFunction([]()
{
	if (!IsWindows7SP1OrGreater())
	{
		FatalError("Windows 7 SP1 or higher is required to run the legitimacy component.");
	}

	// newer SC SDK will otherwise overflow in cert name
	hook::iat("crypt32.dll", CertGetNameStringStubA, "CertGetNameStringA");
	hook::iat("kernel32.dll", LocalFreeStub, "LocalFree");

	hook::iat("user32.dll", LoadIconStub, "LoadIconA");
	hook::iat("user32.dll", LoadIconStub, "LoadIconW");

	hook::iat("ole32.dll", CoCreateInstanceStub, "CoCreateInstance");
	hook::iat("kernel32.dll", CreateProcessAStub, "CreateProcessA");

#ifdef GTA_FIVE
	// bypass the check routine for sky init
	void* skyInit = hook::pattern("48 8D A8 D8 FE FF FF 48 81 EC 10 02 00 00 41 BE").count(1).get(0).get<void>(-0x14);
	char* skyInitLoc = hook::pattern("EB 13 48 8D 0D ? ? ? ? 83 FB 08 74 07").count(2).get(0).get<char>(0x71);

	hook::call(skyInitLoc, skyInit);

	// same for distantlights
	void* distantLightInit = hook::pattern("48 8D 68 A1 48 81 EC F0 00 00 00 BE 01 00").count(1).get(0).get<void>(-0x10);

	hook::call(skyInitLoc + (xbr::IsGameBuildOrGreater<2060>() ? 0x2FA : 0x30B), distantLightInit);
#endif
});
