#pragma once

#include <tlhelp32.h>

namespace fox::detail
{
inline bool IsProcessRunning(const wchar_t* processName)
{
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (snapshot == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	PROCESSENTRY32W entry = { sizeof(entry) };
	bool found = false;

	if (Process32FirstW(snapshot, &entry))
	{
		do
		{
			if (_wcsicmp(entry.szExeFile, processName) == 0)
			{
				found = true;
				break;
			}
		} while (Process32NextW(snapshot, &entry));
	}

	CloseHandle(snapshot);

	return found;
}
}

// Returns whether the FoxG anti-cheat client is currently running.
// The result is cached for 10 seconds, as this is called from hot paths
// (e.g. the frontend render hook).
inline bool getXState()
{
	static bool cachedState = false;
	static ULONGLONG lastCheck = 0;

	const ULONGLONG now = GetTickCount64();

	if (now - lastCheck > 10000)
	{
		lastCheck = now;
		cachedState = fox::detail::IsProcessRunning(L"FoxG.exe");
	}

	return cachedState;
}

// Returns whether the FACEIT anti-cheat client is currently running.
inline bool getFaceItState()
{
	static bool cachedState = false;
	static ULONGLONG lastCheck = 0;

	const ULONGLONG now = GetTickCount64();

	if (now - lastCheck > 10000)
	{
		lastCheck = now;
		cachedState = fox::detail::IsProcessRunning(L"FACEIT.exe") ||
			fox::detail::IsProcessRunning(L"faceitclient.exe");
	}

	return cachedState;
}
