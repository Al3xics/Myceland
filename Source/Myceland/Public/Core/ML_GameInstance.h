// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GameplayTagContainer.h"
#include "Core/ML_CoreData.h"
#include "ML_GameInstance.generated.h"

class UML_SaveSubsystem;

/**
 * Project GameInstance. Holds the player's current ProgressionState, kept in sync with the
 * save: every change made through SetProgressionState() is written straight back to it, and
 * the value is read back from the active slot whenever a gameplay level loads.
 *
 * Also the place where the save learns which level it is in: no other system watches map
 * loads, and "Continue" needs that level to know where to drop the player back in.
 */
UCLASS()
class MYCELAND_API UML_GameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	UFUNCTION(BlueprintPure, Category="Myceland Progression")
	EML_ProgressionState GetProgressionState() const { return ProgressionState; }

	// Updates the progression state and immediately persists it to the save. All writes go
	// through here so the save can never drift from the GameInstance's value.
	UFUNCTION(BlueprintCallable, Category="Myceland Progression")
	void SetProgressionState(EML_ProgressionState NewState);

protected:
	// Read-only from Blueprints — mutate via SetProgressionState so every change is saved.
	UPROPERTY(BlueprintReadOnly, Category="Myceland Progression")
	EML_ProgressionState ProgressionState = EML_ProgressionState::W1L0;

private:
	// Convenience accessor for this GameInstance's save subsystem (null before Init / in CDO).
	UML_SaveSubsystem* GetSaveSubsystem() const;

	// Stamps the loaded level into the active save slot and pulls the slot's progression back
	// into this GameInstance. Skips the menu, which is not a place the player resumes into.
	//
	// Bound to OnWorldInitializedActors rather than PostLoadMapWithWorld on purpose: the latter
	// fires *after* World->BeginPlay(), by which point AML_BoardSpawner and AML_NarrativeTrigger
	// have already read the save. This one fires from InitializeActorsForPlay, just before.
	void HandleWorldInitializedActors(const UWorld::FActorsInitializedParams& Params);

	// Reverse lookup of a loaded world in the DeveloperSettings' Levels map. Invalid when the
	// world isn't one of the configured levels (an archive or test map, say).
	static FGameplayTag ResolveLevelTag(const UWorld* LoadedWorld);

	// "Level.World1.Level2" -> "World 1 - Level 2", the label shown in the save-slot list.
	static FString MakeLevelDisplayName(const FGameplayTag& LevelTag);

	FDelegateHandle WorldInitializedActorsHandle;
};
