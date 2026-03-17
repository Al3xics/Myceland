// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Myceland : ModuleRules
{
	public Myceland(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule", // No need, but still used by unreal's template
			"NavigationSystem", // No need, but still used by unreal's template
			"StateTreeModule", // No need, but still used by unreal's template
			"GameplayStateTreeModule", // No need, but still used by unreal's template
			"Niagara",
			"UMG",
			"Json",
			"DeveloperSettings",
			"GameplayTags"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore"
		});

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}