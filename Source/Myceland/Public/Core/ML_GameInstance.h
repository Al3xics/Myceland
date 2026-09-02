// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Core/ML_CoreData.h"
#include "ML_GameInstance.generated.h"

class UML_SaveSubsystem;

/**
 * Project GameInstance. Holds the player's current ProgressionState, kept in sync with the
 * save: the value is loaded from the save on Init(), and every change made through
 * SetProgressionState() is written straight back to it.
 */
UCLASS()
class MYCELAND_API UML_GameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	// Loads the persisted ProgressionState from the save into this GameInstance.
	virtual void Init() override;

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
};
