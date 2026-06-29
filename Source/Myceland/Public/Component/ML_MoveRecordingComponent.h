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
	const TArray<FIntPoint>& GetActiveMoveAxialPath() const { return ActiveMoveAxialPath; }

	// --- Normal move recording ---

	/** Call at the start of every player-initiated move. */
	void BeginMoveRecord(const FIntPoint& Start, const FIntPoint& End,
	                     const FVector& StartWorld, const FVector& EndWorld,
	                     const TArray<FIntPoint>& Path);

	/**
	 * Called when the player redirects to a new destination while a move is already in progress.
	 *
	 * Instead of restarting the record (which would lose the path already walked),
	 * this splices the new BFS sub-path onto the portion of the current path that
	 * has already been committed (i.e. the waypoints up to and including
	 * CurrentPathIndexInRecord).
	 *
	 * @param NewEndAxial        The new destination axial.
	 * @param NewEndWorld        The new destination world position.
	 * @param NewSubPath         BFS path from the player's current axial position to NewEndAxial.
	 *                           Must start at the axial the player is currently heading toward
	 *                           (the waypoint at CurrentPathIndexInRecord).
	 * @param CurrentPathIndexInRecord
	 *                           The index in ActiveMoveAxialPath that the player has
	 *                           already reached / is currently moving toward.
	 *                           Everything before this index is already "walked" history.
	 */
	const TArray<FIntPoint>& ExtendMoveRecord(const FIntPoint& NewEndAxial,
	                                          const FVector& NewEndWorld,
	                                          const TArray<FIntPoint>& NewSubPath,
	                                          int32 CurrentPathIndexInRecord);

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
