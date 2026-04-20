// Copyright Myceland Team, All Rights Reserved.

#include "Component/ML_MoveRecordingComponent.h"

#include "Player/ML_PlayerCharacter.h"
#include "Subsystem/ML_RollBackSubsystem.h"
#include "Tiles/ML_Tile.h"

void UML_MoveRecordingComponent::BeginMoveRecord(const FIntPoint& Start, const FIntPoint& End,
                                                  const FVector& StartWorld, const FVector& EndWorld,
                                                  const TArray<FIntPoint>& Path)
{
	bMoveInProgress = true;
	bUndoMovePlayback = false;
	bSuppressMoveRecording = false;

	MoveStartAxial = Start;
	MoveEndAxial   = End;
	MoveStartWorld = StartWorld;
	MoveEndWorld   = EndWorld;

	ActiveMoveAxialPath = Path;
	ActiveMovePickedCollectibles.Reset();
}

const TArray<FIntPoint>& UML_MoveRecordingComponent::ExtendMoveRecord(const FIntPoint& NewEndAxial,
                                                                      const FVector& NewEndWorld,
                                                                      const TArray<FIntPoint>& NewSubPath,
                                                                      int32 CurrentPathIndexInRecord)
{
	// Nothing to extend if no move is in progress.
	if (!bMoveInProgress || bUndoMovePlayback)
		return ActiveMoveAxialPath;

	// NewSubPath must contain at least start + end (i.e. it must be a real path segment).
	if (NewSubPath.Num() < 2)
		return ActiveMoveAxialPath;

	// Keep the "walked" prefix: everything up to (and including) the waypoint the
	// player has already reached / is about to reach.
	// CurrentPathIndexInRecord is the index the TickMoveAlongPath is currently aiming at,
	// so all indices < CurrentPathIndexInRecord have already been physically crossed.
	const int32 KeepUpTo = FMath::Clamp(CurrentPathIndexInRecord + 1, 0, ActiveMoveAxialPath.Num());

	TArray<FIntPoint> KeptPath;
	KeptPath.Reserve(KeepUpTo + NewSubPath.Num());

	// Copy the already-walked portion.
	for (int32 i = 0; i < KeepUpTo; ++i)
		KeptPath.Add(ActiveMoveAxialPath[i]);

	// Append the new sub-path.
	// NewSubPath[0] is the junction tile (the axial the player is currently heading toward).
	// If it's already the last element of the kept portion, skip it to avoid a duplicate.
	int32 SubPathStart = 0;
	if (KeptPath.Num() > 0 && NewSubPath[0] == KeptPath.Last())
		SubPathStart = 1;

	for (int32 i = SubPathStart; i < NewSubPath.Num(); ++i)
		KeptPath.Add(NewSubPath[i]);

	// Commit the merged path.
	ActiveMoveAxialPath = MoveTemp(KeptPath);

	// Update the destination; the start stays the same (it's where the move originally began).
	MoveEndAxial = NewEndAxial;
	MoveEndWorld = NewEndWorld;
	return ActiveMoveAxialPath;
}

void UML_MoveRecordingComponent::NotifyCollectiblePicked(const FIntPoint& Axial)
{
	if (!bMoveInProgress)
		return;
	ActiveMovePickedCollectibles.Add(Axial);
}

bool UML_MoveRecordingComponent::CommitMoveRecord(AML_PlayerCharacter* Character,
                                                   UML_RollBackSubsystem* RollBackSubsystem)
{
	if (IsValid(RollBackSubsystem))
	{
		if (bUndoMovePlayback)
		{
			bUndoMovePlayback = false;
			bSuppressMoveRecording = false;

			// Restore any leftovers (e.g. original start tile depending on timing)
			if (bUndoRestoreCollectibles && UndoMoveRemainingCollectibles.Num() > 0)
			{
				for (const FIntPoint& Ax : UndoMoveRemainingCollectibles)
					RollBackSubsystem->RestoreCollectibleDuringUndoMove(Ax);
				UndoMoveRemainingCollectibles.Reset();
			}
			bUndoRestoreCollectibles = false;

			RollBackSubsystem->FinishUndoAnimation();
		}
		else if (bMoveInProgress && ActiveMoveAxialPath.Num() > 0)
		{
			if (!bSuppressMoveRecording)
			{
				if (!IsValid(Character) || !IsValid(Character->CurrentTileOn))
				{
					// Preserve original behavior: abort TickMoveAlongPath without resetting
					return false;
				}

				const TArray<FIntPoint> Picked = ActiveMovePickedCollectibles.Array();
				RollBackSubsystem->NotifyMoveCompleted(
					MoveStartAxial,
					MoveEndAxial,
					ActiveMoveAxialPath,
					MoveStartWorld,
					MoveEndWorld,
					Picked
				);
			}
			else
			{
				bSuppressMoveRecording = false;
			}
		}
	}

	bMoveInProgress = false;
	ActiveMoveAxialPath.Reset();
	ActiveMovePickedCollectibles.Reset();
	return true;
}

void UML_MoveRecordingComponent::BeginUndoPlayback(const TArray<FIntPoint>& Path,
                                                    const TArray<FIntPoint>& PickedCollectibleAxials)
{
	bUndoMovePlayback = true;
	bSuppressMoveRecording = true;

	bUndoRestoreCollectibles = (PickedCollectibleAxials.Num() > 0);
	UndoMoveRemainingCollectibles.Reset();
	for (const FIntPoint& Ax : PickedCollectibleAxials)
		UndoMoveRemainingCollectibles.Add(Ax);

	bMoveInProgress = true;
	ActiveMoveAxialPath = Path;
	ActiveMovePickedCollectibles.Reset();
}

void UML_MoveRecordingComponent::TickUndoRestore(int32 ReachedIndex, UML_RollBackSubsystem* RollBackSubsystem)
{
	if (!bUndoMovePlayback || !bUndoRestoreCollectibles)
		return;

	const int32 LeftIndex = ReachedIndex - 1; // tile behind the player
	if (LeftIndex < 0 || LeftIndex >= ActiveMoveAxialPath.Num())
		return;

	const FIntPoint LeftAxial = ActiveMoveAxialPath[LeftIndex];
	if (!UndoMoveRemainingCollectibles.Contains(LeftAxial))
		return;

	if (IsValid(RollBackSubsystem))
		RollBackSubsystem->RestoreCollectibleDuringUndoMove(LeftAxial);

	UndoMoveRemainingCollectibles.Remove(LeftAxial);
}
