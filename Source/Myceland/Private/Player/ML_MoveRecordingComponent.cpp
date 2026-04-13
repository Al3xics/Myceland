// Copyright Myceland Team, All Rights Reserved.

#include "Player/ML_MoveRecordingComponent.h"

#include "Player/ML_PlayerCharacter.h"
#include "Subsystem/ML_WavePropagationSubsystem.h"
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

void UML_MoveRecordingComponent::NotifyCollectiblePicked(const FIntPoint& Axial)
{
	if (!bMoveInProgress)
		return;
	ActiveMovePickedCollectibles.Add(Axial);
}

bool UML_MoveRecordingComponent::CommitMoveRecord(AML_PlayerCharacter* Character,
                                                   UML_WavePropagationSubsystem* WaveSubsystem)
{
	if (IsValid(WaveSubsystem))
	{
		if (bUndoMovePlayback)
		{
			bUndoMovePlayback = false;
			bSuppressMoveRecording = false;

			// Restore any leftovers (e.g. original start tile depending on timing)
			if (bUndoRestoreCollectibles && UndoMoveRemainingCollectibles.Num() > 0)
			{
				for (const FIntPoint& Ax : UndoMoveRemainingCollectibles)
					WaveSubsystem->RestoreCollectibleDuringUndoMove(Ax);
				UndoMoveRemainingCollectibles.Reset();
			}
			bUndoRestoreCollectibles = false;

			WaveSubsystem->FinishUndoAnimation();
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
				WaveSubsystem->NotifyMoveCompleted(
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

void UML_MoveRecordingComponent::TickUndoRestore(int32 ReachedIndex, UML_WavePropagationSubsystem* WaveSubsystem)
{
	if (!bUndoMovePlayback || !bUndoRestoreCollectibles)
		return;

	const int32 LeftIndex = ReachedIndex - 1; // tile behind the player
	if (LeftIndex < 0 || LeftIndex >= ActiveMoveAxialPath.Num())
		return;

	const FIntPoint LeftAxial = ActiveMoveAxialPath[LeftIndex];
	if (!UndoMoveRemainingCollectibles.Contains(LeftAxial))
		return;

	if (IsValid(WaveSubsystem))
		WaveSubsystem->RestoreCollectibleDuringUndoMove(LeftAxial);

	UndoMoveRemainingCollectibles.Remove(LeftAxial);
}
