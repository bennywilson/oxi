// OXI 2025

#include "OxiEditor.h"

DEFINE_LOG_CATEGORY(LogOxiEditor);

#include "Modules/ModuleManager.h"

class FOxiEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FOxiEditorModule, OxiEditor)
