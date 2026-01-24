// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Oxi : ModuleRules
{
	public Oxi(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(new string[] {
			"Oxi"
		});

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"AIModule",
				"Core",
				"CoreUObject",
				"Engine",
				"EnhancedInput",
				"PhysicsCore",
				"InputCore",
				"HeadMountedDisplay",
				"UMG"
			}
		);

        if (Target.bBuildEditor)
        {
    		PrivateDependencyModuleNames.AddRange(new string[] { "AssetTools", "UnrealEd" });
        }

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
