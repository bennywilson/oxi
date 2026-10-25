// OXI 2026

using System.IO;
using UnrealBuildTool;

public class OxiRendering : ModuleRules
{
	public OxiRendering(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"Engine",
			"RenderCore",
			"Renderer",
			"RHI"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Projects"
		});

		// FPostProcessingInputs (the scene view extension post-process hook's argument) lives in Renderer/Internal,
		// which UBT only exposes to engine-scoped modules.
		PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Source", "Runtime", "Renderer", "Internal"));
	}
}
