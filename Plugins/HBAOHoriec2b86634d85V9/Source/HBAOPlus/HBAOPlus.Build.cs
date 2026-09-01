// Copyright 2026 matiasgql. All Rights Reserved.
using System.Collections.Generic;
using UnrealBuildTool;
using System.IO;

public class HBAOPlus : ModuleRules
{
	public HBAOPlus(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"RenderCore",
			"RHI"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Renderer",
			"RenderCore",
			"RHI",
			"Projects"
		});

		string RendererDir = Path.Combine(EngineDirectory, "Source", "Runtime", "Renderer");

		PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(RendererDir, "Private"),
				Path.Combine(RendererDir, "Internal"),
				Path.Combine(RendererDir, "Private/PostProcess"),
			}
		);

		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(RendererDir, "Public"),
				Path.Combine(RendererDir, "Internal"),
			}
		);

		bUseRTTI = false;
	}
}