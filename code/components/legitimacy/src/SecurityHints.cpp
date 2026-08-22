/*
 * Pattern-hint storage exports (SecurityPatternSaveHint/LoadHints).
 *
 * The closed legitimacy.dll used to provide these to Hooking.Patterns.cpp
 * (which resolves them via GetProcAddress). Storage is a simple append-only
 * local file - the same format the legacy tree used for its local hint
 * cache (data/cache/hints_<build>.dat), so existing caches keep working.
 */

#include <StdInc.h>

#include <Hooking.h>
#include <CrossBuildRuntime.h>

#include <mutex>

static std::mutex g_hintsMutex;

static fwPlatformString GetHintsFileName()
{
	return MakeRelativeCitPath(ToWide(fmt::sprintf("data\\cache\\hints_%s.dat", xbr::GetCurrentGameBuildString())));
}

static std::pair<uintptr_t, uintptr_t> GetUnadjustedModuleRange()
{
	uintptr_t begin = (uintptr_t)GetModuleHandleW(nullptr);
	uintptr_t end = begin;

	auto dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(begin);
	auto ntHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(begin + dosHeader->e_lfanew);

	end += ntHeader->OptionalHeader.SizeOfImage;

	return { hook::get_unadjusted(begin), hook::get_unadjusted(end) };
}

extern "C" DLL_EXPORT void SecurityPatternSaveHint(uint64_t hash, uintptr_t hint)
{
	static auto exeRange = GetUnadjustedModuleRange();

	if (hint < exeRange.first || hint >= exeRange.second)
	{
		return;
	}

	std::unique_lock lock(g_hintsMutex);

	FILE* hints = _pfopen(GetHintsFileName().c_str(), _P("ab"));

	if (hints)
	{
		fwrite(&hash, 1, sizeof(hash), hints);
		fwrite(&hint, 1, sizeof(hint), hints);

		fclose(hints);
	}
}

extern "C" DLL_EXPORT bool SecurityPatternLoadHints(uint64_t hash, uintptr_t* outHints, size_t* hintsCount)
{
	if (!hintsCount)
	{
		return false;
	}

	std::unique_lock lock(g_hintsMutex);

	// count/fetch all hints matching the hash
	size_t found = 0;

	FILE* hints = _pfopen(GetHintsFileName().c_str(), _P("rb"));

	if (hints)
	{
		uint64_t entryHash;
		uintptr_t entryHint;

		while (fread(&entryHash, 1, sizeof(entryHash), hints) == sizeof(entryHash) &&
			fread(&entryHint, 1, sizeof(entryHint), hints) == sizeof(entryHint))
		{
			if (entryHash == hash)
			{
				if (outHints && found < *hintsCount)
				{
					outHints[found] = entryHint;
				}

				found++;
			}
		}

		fclose(hints);
	}

	if (found == 0)
	{
		*hintsCount = 0;
		return false;
	}

	if (outHints)
	{
		// caller passed a buffer sized from a previous count call; only
		// succeed if the count still matches exactly
		bool exact = (found == *hintsCount);
		*hintsCount = found;

		return exact;
	}

	*hintsCount = found;
	return true;
}
