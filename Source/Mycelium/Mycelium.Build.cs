// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Mycelium : ModuleRules
{
    public Mycelium(ReadOnlyTargetRules Target) : base(Target)
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
            "StateTreeModule",
            "GameplayStateTreeModule",
            "Niagara",
            "UMG",
            "Slate",
            "Json",
            "JsonUtilities"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });

        PublicIncludePaths.AddRange(new string[] {
            "Mycelium",
            "Mycelium/Variant_Strategy",
            "Mycelium/Variant_Strategy/UI",
            "Mycelium/Variant_TwinStick",
            "Mycelium/Variant_TwinStick/AI",
            "Mycelium/Variant_TwinStick/Gameplay",
            "Mycelium/Variant_TwinStick/UI"
        });

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
