#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ML_CinematicSubsystem.generated.h"

class AActor;
class ALevelSequenceActor;
class APlayerController;
class ULevelSequence;
class ULevelSequencePlayer;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCinematicStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCinematicFinished);

UCLASS()
class MYCELAND_API UML_CinematicSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Myceland Cinematics")
	bool PlayCinematic(
	ULevelSequence* LevelSequence,
	AActor* BlendCamera = nullptr,
	float BlendTime = 1.0f,
	float ReturnBlendTime = 0.75f,
	bool bWaitForBlendToFinish = true,
	APlayerController* PlayerController = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "Myceland Cinematics")
	void StopCurrentCinematic();

	UFUNCTION(BlueprintPure, Category = "Myceland Cinematics")
	bool IsCinematicPlaying() const { return bIsCinematicPlaying; }

	UPROPERTY(BlueprintAssignable, Category = "Myceland Cinematics")
	FOnCinematicStarted OnCinematicStarted;

	UPROPERTY(BlueprintAssignable, Category = "Myceland Cinematics")
	FOnCinematicFinished OnCinematicFinished;

private:
	UPROPERTY(Transient)
	ULevelSequence* PendingLevelSequence = nullptr;

	UPROPERTY(Transient)
	ULevelSequencePlayer* CurrentSequencePlayer = nullptr;

	UPROPERTY(Transient)
	ALevelSequenceActor* CurrentSequenceActor = nullptr;

	UPROPERTY(Transient)
	APlayerController* CachedPlayerController = nullptr;

	UPROPERTY(Transient)
	AActor* OriginalViewTarget = nullptr;

	FTimerHandle BlendTimerHandle;

	bool bIsCinematicPlaying = false;
	float CachedReturnBlendTime = 0.75f;

	void StartPendingCinematic();
	void FinishCinematic(bool bBroadcastFinished);
	void DisablePlayerInput(APlayerController* PlayerController);
	void RestorePlayerInput();
	void ClearCinematicReferences();

	UFUNCTION()
	void HandleCinematicFinished();
};