// OXI 2025

using UnrealBuildTool;

public class OxiEditor : ModuleRules
{
	public OxiEditor(ReadOnlyTargetRules Target) : base(Target)
	{

		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(new string[] {
			"OxiEditor"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"EditorSubsystem",
			"Engine",
			"UnrealEd",
            "Oxi"
        });
	}
}