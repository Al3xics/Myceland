// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ML_MoveRecordingComponent.generated.h"

class AML_PlayerCharacter;
class UML_RollBackSubsystem;

/**
 * Owns all state required for move recording (undo system) and undo playback.
 * Extracted from AML_PlayerController to isolate the undo responsibility.
 */
UCLASS(ClassGroup=(Myceland), meta=(BlueprintSpawnableComponent))
class MYCELAND_API UML_MoveRecordingComponent : public UActorComponent
{
	GENERATED_BODY()

private:

	bool bMoveInProgress = false;

	FIntPoint MoveStartAxial = FIntPoint::ZeroValue;
	FVector   MoveStartWorld = FVector::ZeroVector;

	FIntPoint MoveEndAxial   = FIntPoint::ZeroValue;
	FVector   MoveEndWorld   = FVector::ZeroVector;

	TArray<FIntPoint> ActiveMoveAxialPath;

	UPROPERTY(Transient)
	TSet<FIntPoint> ActiveMovePickedCollectibles;

	bool bSuppressMoveRecording = false;
	bool bUndoMovePlayback = false;

	UPROPERTY(Transient)
	TSet<FIntPoint> UndoMoveRemainingCollectibles;

	bool bUndoRestoreCollectibles = false;

public:

	// --- Queries ---

	bool IsMoveInProgress() const { return bMoveInProgress; }
	bool IsUndoMovePlayback() const { return bUndoMovePlayback; }

	// --- Normal move recording ---

	/** Call at the start of every player-initiated move. */
	void BeginMoveRecord(const FIntPoint& Start, const FIntPoint& End,
	                     const FVector& StartWorld, const FVector& EndWorld,
	                     const TArray<FIntPoint>& Path);

	/** Record a collectible picked up while a move is in progress. */
	void NotifyCollectiblePicked(const FIntPoint& Axial);

	/**
	 * Call when the path finishes. Notifies the wave subsystem and resets internal state.
	 * Returns false if the caller should abort early (CurrentTileOn was null).
	 */
	bool CommitMoveRecord(AML_PlayerCharacter* Character, UML_RollBackSubsystem* RollBackSubsystem);

	// --- Undo playback ---

	/** Begin replaying a recorded move in reverse (called by the wave subsystem via the controller wrapper). */
	void BeginUndoPlayback(const TArray<FIntPoint>& Path, const TArray<FIntPoint>& PickedCollectibleAxials);

	/**
	 * Per-waypoint restore during undo playback.
	 * Call each time the player reaches a tile during undo movement.
	 */
	void TickUndoRestore(int32 ReachedIndex, UML_RollBackSubsystem* RollBackSubsystem);
};
