// Copyright Myceland Team, All Rights Reserved.

#include "Subsystem/ML_AmbienceSubsystem.h"

#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Subsystem/ML_SoundSubsystem.h"
#include "Subsystem/ML_WinLoseSubsystem.h"
#include "Tiles/ML_BoardSpawner.h"

DEFINE_LOG_CATEGORY_STATIC(LogMycelandAmbience, Log, All);

void UML_AmbienceSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	DevSettings = UML_MycelandDeveloperSettings::GetMycelandDeveloperSettings();
	WinLoseSubsystem = InWorld.GetSubsystem<UML_WinLoseSubsystem>();

	InitializePuzzleCount();

	if (WinLoseSubsystem)
	{
		WinLoseSubsystem->OnWin.AddDynamic(this, &UML_AmbienceSubsystem::HandlePuzzleWon);
	}

	if (DevSettings && DevSettings->bAutoStartAmbienceEnviro)
	{
		StartAmbience();
	}
}

void UML_AmbienceSubsystem::Deinitialize()
{
	StopAmbience();

	if (WinLoseSubsystem)
	{
		WinLoseSubsystem->OnWin.RemoveDynamic(this, &UML_AmbienceSubsystem::HandlePuzzleWon);
	}

	WonBoards.Reset();
	WinLoseSubsystem = nullptr;
	DevSettings = nullptr;

	Super::Deinitialize();
}

void UML_AmbienceSubsystem::StartAmbience()
{
	if (bAmbienceRunning)
	{
		return;
	}

	if (!DevSettings)
	{
		DevSettings = UML_MycelandDeveloperSettings::GetMycelandDeveloperSettings();
	}

	if (!DevSettings || (DevSettings->LivingAmbienceEventPaths.IsEmpty() && DevSettings->DeadAmbienceEventPaths.IsEmpty()))
	{
		UE_LOG(LogMycelandAmbience, Warning, TEXT("Ambience Enviro has no FMOD event paths configured."));
		return;
	}

	bAmbienceRunning = true;
	PlayNextAmbienceSound();
}

void UML_AmbienceSubsystem::StopAmbience()
{
	bAmbienceRunning = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AmbienceTimerHandle);
	}
}

float UML_AmbienceSubsystem::GetLivingAmbienceRatio() const
{
	if (TotalPuzzleCount <= 0)
	{
		return 0.0f;
	}

	return FMath::Clamp(static_cast<float>(WonPuzzleCount) / static_cast<float>(TotalPuzzleCount), 0.0f, 1.0f);
}

void UML_AmbienceSubsystem::HandlePuzzleWon()
{
	if (!WinLoseSubsystem)
	{
		return;
	}

	AML_BoardSpawner* WonBoard = WinLoseSubsystem->CurrentBoardSpawner;
	if (!IsValid(WonBoard))
	{
		return;
	}

	const FObjectKey WonBoardKey(WonBoard);
	if (WonBoards.Contains(WonBoardKey))
	{
		return;
	}

	WonBoards.Add(WonBoardKey);
	WonPuzzleCount = FMath::Clamp(WonPuzzleCount + 1, 0, FMath::Max(TotalPuzzleCount, 1));

	UE_LOG(LogMycelandAmbience, Log, TEXT("Ambience Enviro puzzle won: %d/%d (living ratio %.2f)"),
		WonPuzzleCount,
		TotalPuzzleCount,
		GetLivingAmbienceRatio());
}

void UML_AmbienceSubsystem::PlayNextAmbienceSound()
{
	if (!bAmbienceRunning || !DevSettings)
	{
		return;
	}

	const TArray<FString>& LivingEvents = DevSettings->LivingAmbienceEventPaths;
	const TArray<FString>& DeadEvents = DevSettings->DeadAmbienceEventPaths;

	const bool bCanPlayLiving = !LivingEvents.IsEmpty();
	const bool bCanPlayDead = !DeadEvents.IsEmpty();
	if (!bCanPlayLiving && !bCanPlayDead)
	{
		StopAmbience();
		return;
	}

	const float LivingRatio = GetLivingAmbienceRatio();
	const bool bPickLiving = bCanPlayLiving && (!bCanPlayDead || FMath::FRand() < LivingRatio);
	const TArray<FString>& EventPool = bPickLiving ? LivingEvents : DeadEvents;
	const FString& EventPath = EventPool[FMath::RandRange(0, EventPool.Num() - 1)];

	if (UML_SoundSubsystem* SoundSubsystem = UML_SoundSubsystem::Get(this))
	{
		SoundSubsystem->StartSound2DByPath(EventPath);
		UE_LOG(LogMycelandAmbience, Verbose, TEXT("Played %s ambience event: %s"),
			bPickLiving ? TEXT("living") : TEXT("dead"),
			*EventPath);
	}

	ScheduleNextAmbienceSound();
}

void UML_AmbienceSubsystem::ScheduleNextAmbienceSound()
{
	UWorld* World = GetWorld();
	if (!World || !DevSettings || !bAmbienceRunning)
	{
		return;
	}

	const float MinDelay = FMath::Max(0.0f, DevSettings->AmbienceMinDelay);
	const float MaxDelay = FMath::Max(MinDelay, DevSettings->AmbienceMaxDelay);
	const float Delay = FMath::FRandRange(MinDelay, MaxDelay);

	World->GetTimerManager().SetTimer(
		AmbienceTimerHandle,
		this,
		&UML_AmbienceSubsystem::PlayNextAmbienceSound,
		Delay,
		false);
}

void UML_AmbienceSubsystem::InitializePuzzleCount()
{
	WonBoards.Reset();
	WonPuzzleCount = 0;
	TotalPuzzleCount = GetConfiguredPuzzleCountForCurrentLevel();

	if (TotalPuzzleCount <= 0 && DevSettings && DevSettings->bFallbackToBoardSpawnerCount)
	{
		TArray<AActor*> FoundBoards;
		UGameplayStatics::GetAllActorsOfClass(this, AML_BoardSpawner::StaticClass(), FoundBoards);
		TotalPuzzleCount = FoundBoards.Num();
	}

	TotalPuzzleCount = FMath::Max(TotalPuzzleCount, 1);

	UE_LOG(LogMycelandAmbience, Log, TEXT("Ambience Enviro initialized for %s with %d puzzle(s)."),
		*GetCleanMapName(),
		TotalPuzzleCount);
}

int32 UML_AmbienceSubsystem::GetConfiguredPuzzleCountForCurrentLevel() const
{
	if (!DevSettings)
	{
		return 0;
	}

	const FString CurrentMapName = GetCleanMapName();
	for (const FML_LevelAmbiencePuzzleCount& Entry : DevSettings->AmbiencePuzzleCounts)
	{
		if (!Entry.Level.IsValid())
		{
			continue;
		}

		const TSoftObjectPtr<UWorld>* LevelAsset = DevSettings->Levels.Find(Entry.Level);
		if (!LevelAsset)
		{
			continue;
		}

		const FSoftObjectPath LevelPath = LevelAsset->ToSoftObjectPath();
		if (LevelPath.GetAssetName() == CurrentMapName)
		{
			return Entry.PuzzleCount;
		}
	}

	return 0;
}

FString UML_AmbienceSubsystem::GetCleanMapName() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return FString();
	}

	FString MapName = World->GetMapName();
	if (MapName.StartsWith(TEXT("UEDPIE_")))
	{
		const int32 PrefixEnd = MapName.Find(TEXT("_"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 7);
		if (PrefixEnd != INDEX_NONE)
		{
			MapName = MapName.Mid(PrefixEnd + 1);
		}
	}
	return MapName;
}
