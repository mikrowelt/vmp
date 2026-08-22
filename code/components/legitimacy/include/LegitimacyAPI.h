#pragma once

namespace ros
{
	std::string
#if defined(COMPILING_LEGITIMACY)
	DLL_EXPORT
#endif
	GetEntitlementSource();

	std::string
#if defined(COMPILING_LEGITIMACY)
	DLL_EXPORT
#endif
	GetApiIdentifier();
}
