// OXI 2026

#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"

class FOxiRenderingModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// Must happen at PostConfigInit, before global shaders are compiled.
		AddShaderSourceDirectoryMapping(TEXT("/OxiShaders"), FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders")));
	}
};

IMPLEMENT_MODULE(FOxiRenderingModule, OxiRendering);
