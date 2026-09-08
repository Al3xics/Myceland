// Copyright Myceland Team, All Rights Reserved.

#include "Core/ML_GameInstance.h"
#include "Core/ML_GameplayTags.h"
#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "Save System/ML_SaveSubsystem.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

void UML_GameInstance::Init()
{
	Super::Init();

	// No save is loaded at this point on purpose: the menu runs first and the player has not
	// picked a slot yet (see UML_SaveSubsystem::Initialize). ProgressionState is therefore
	// left at its default here and refreshed from the chosen slot on the first level load.
	WorldInitializedActorsHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(
		this, &UML_GameInstance::HandleWorldInitializedActors);
}

void UML_GameInstance::Shutdown()
{
	if (WorldInitializedActorsHandle.IsValid())
	{
		FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitializedActorsHandle);
		WorldInitializedActorsHandle.Reset();
	}

	Super::Shutdown();
}

void UML_GameInstance::HandleWorldInitializedActors(const UWorld::FActorsInitializedParams& Params)
{
	UWorld* LoadedWorld = Params.World;

	// The delegate is global: editor preview worlds, and other PIE instances' worlds, also
	// come through here. Only this GameInstance's own game world is ours to record.
	if (!LoadedWorld || !LoadedWorld->IsGameWorld()) return;
	if (LoadedWorld->GetGameInstance() && LoadedWorld->GetGameInstance() != this) return;

	const FGameplayTag LevelTag = ResolveLevelTag(LoadedWorld);

	// The menu is not somewhere the player resumes into, and an unconfigured map (archive,
	// test level) has nothing meaningful to record either.
	if (!LevelTag.IsValid() || LevelTag == ML_GameplayTags::Level_Menu) return;

	UML_SaveSubsystem* SaveSys = GetSaveSubsystem();
	if (!SaveSys) return;

	// Entering a gameplay level straight from the editor skips New Game / Continue entirely,
	// so make sure there is something to write to before recording anything.
	SaveSys->EnsureActiveSlot();

	SaveSys->SetCurrentLevel(LevelTag, MakeLevelDisplayName(LevelTag));
	ProgressionState = SaveSys->GetProgressionState();
}

FGameplayTag UML_GameInstance::ResolveLevelTag(const UWorld* LoadedWorld)
{
	if (!LoadedWorld) return FGameplayTag::EmptyTag;

	const UML_MycelandDeveloperSettings* Settings = GetDefault<UML_MycelandDeveloperSettings>();
	if (!Settings) return FGameplayTag::EmptyTag;

	// In PIE the package carries a UEDPIE_N_ prefix that never matches the configured path.
	const FString WorldPackage = UWorld::RemovePIEPrefix(LoadedWorld->GetOutermost()->GetName());

	for (const TPair<FGameplayTag, TSoftObjectPtr<UWorld>>& Entry : Settings->Levels)
	{
		if (Entry.Value.GetLongPackageName() == WorldPackage)
			return Entry.Key;
	}

	return FGameplayTag::EmptyTag;
}

FString UML_GameInstance::MakeLevelDisplayName(const FGameplayTag& LevelTag)
{
	// Derived from the tag rather than a second config table to maintain: the tag names are
	// already the canonical list of levels. "Level.World1.Level2" -> "World 1 - Level 2".
	FString Path = LevelTag.ToString();
	Path.RemoveFromStart(TEXT("Level."));
	Path.ReplaceInline(TEXT("."), TEXT(" - "));

	FString Result;
	Result.Reserve(Path.Len() + 4);

	for (int32 i = 0; i < Path.Len(); ++i)
	{
		const TCHAR Current = Path[i];

		// Split "World1" and "WorldMap" without touching the " - " separators we just inserted.
		if (i > 0 && FChar::IsAlpha(Path[i - 1])
			&& (FChar::IsDigit(Current) || FChar::IsUpper(Current)))
		{
			Result.AppendChar(TEXT(' '));
		}

		Result.AppendChar(Current);
	}

	return Result;
}

void UML_GameInstance::SetProgressionState(EML_ProgressionState NewState)
{
	if (ProgressionState == NewState) return;

	ProgressionState = NewState;

	if (UML_SaveSubsystem* SaveSys = GetSaveSubsystem())
	{
		SaveSys->SetProgressionState(NewState);
	}
}

UML_SaveSubsystem* UML_GameInstance::GetSaveSubsystem() const
{
	return GetSubsystem<UML_SaveSubsystem>();
}
