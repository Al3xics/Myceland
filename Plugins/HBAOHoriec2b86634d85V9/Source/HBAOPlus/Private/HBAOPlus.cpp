// Copyright 2026 matiasgql. All Rights Reserved.
#include "HBAOPlus.h"
#include "HBAOPlusViewExtension.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"
#include "Engine/Engine.h"
#include "Misc/CoreDelegates.h"
#include "Runtime/Launch/Resources/Version.h"

IMPLEMENT_MODULE(FHBAOPlusModule, HBAOPlus)

void FHBAOPlusModule::StartupModule()
{
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("HBAOPlus"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/HBAOPlus"), PluginShaderDir);

	if (GEngine)
	{
		OnPostEngineInit();
	}
	else
	{
#if ENGINE_MINOR_VERSION >= 8
		FCoreDelegates::GetOnPostEngineInit().AddRaw(this, &FHBAOPlusModule::OnPostEngineInit);
#else
		FCoreDelegates::OnPostEngineInit.AddRaw(this, &FHBAOPlusModule::OnPostEngineInit);
#endif
	}
}

void FHBAOPlusModule::OnPostEngineInit()
{
	if (GEngine)
	{
		// Register the view extension
		FHBAOPlusViewExtension::Register();
	}
}

void FHBAOPlusModule::ShutdownModule()
{
	// Unregister the view extension
	FHBAOPlusViewExtension::Unregister();
}
