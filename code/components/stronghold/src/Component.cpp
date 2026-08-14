/*
 * Public stub for the private stronghold component.
 * Provides the minimum required for the build system to resolve the dependency.
 */

#include "StdInc.h"
#include "ComponentLoader.h"
#include "ResumeComponent.h"

class ComponentInstance : public LifeCyclePreInitComponentBase<Component>
{
public:
	virtual bool Initialize()
	{
		InitFunctionBase::RunAll();
		return true;
	}

	virtual bool DoGameLoad(void* module)
	{
		HookFunction::RunAll();
		return true;
	}

	virtual bool Shutdown()
	{
		return true;
	}
};

extern "C" DLL_EXPORT Component* CreateComponent()
{
	return new ComponentInstance();
}
