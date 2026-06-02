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
			"AIModule",
			"NavigationSystem",
			"Niagara",
			"UMG",
			"Json",
			"Foliage",
			"LevelSequence",
			"MovieScene",
			"DeveloperSettings",
			"CinematicCamera",
            "ProceduralMeshComponent",
            "GameplayTags", "FMODStudio"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore", "FMODStudio"
		});

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}