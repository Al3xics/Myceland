// Copyright Myceland Team, All Rights Reserved.

#include "Subsystem/ML_BoardActionSubsystem.h"

#include "Engine/World.h"
#include "Player/ML_PlayerController.h"
#include "Subsystem/ML_CinematicSubsystem.h"
#include "Subsystem/ML_WinLoseSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogMycelandBoardAction, Log, All);

namespace
{
	const TCHAR* LexReason(const EML_BoardActionReason Reason)
	{
		switch (Reason)
		{
		case EML_BoardActionReason::Wave:           return TEXT("Wave");
		case EML_BoardActionReason::SpawnAnimation: return TEXT("SpawnAnimation");
		case EML_BoardActionReason::Undo:           return TEXT("Undo");
		case EML_BoardActionReason::Reset:          return TEXT("Reset");
		default:                                    return TEXT("Other");
		}
	}
}

UML_BoardActionSubsystem* UML_BoardActionSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject) return nullptr;

	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UML_BoardActionSubsystem>() : nullptr;
}

bool UML_BoardActionSubsystem::IsTickable() const
{
	// Only tick while something holds the board: outside a turn this subsystem costs nothing.
	return Tokens.Num() > 0;
}

TStatId UML_BoardActionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UML_BoardActionSubsystem, STATGROUP_Tickables);
}

void UML_BoardActionSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const int32 CountBefore = Tokens.Num();
	PruneTokens();

	if (Tokens.Num() != CountBefore)
	{
		ApplyInputLock();
		if (Tokens.Num() == 0)
			OnBoardBusyChanged.Broadcast(false);
		return;
	}

	// Several other systems (rollback, cinematics, teleporters) drive EnableInput on their own. While we
	// hold a token our invariant must win, so we re-assert the lock instead of trusting that nobody else
	// touched the switch since the last frame.
	if (Tokens.Num() > 0)
		ApplyInputLock();
}

void UML_BoardActionSubsystem::BeginBoardAction(UObject* Owner, const EML_BoardActionReason Reason, const float TimeoutSeconds)
{
	if (!IsValid(Owner))
	{
		UE_LOG(LogMycelandBoardAction, Warning, TEXT("BeginBoardAction called with no owner (%s): ignored."), LexReason(Reason));
		return;
	}

	const UWorld* World = GetWorld();
	if (!World) return;

	const double ExpireTime = World->GetTimeSeconds() + FMath::Max(0.1f, TimeoutSeconds);

	// Re-entrant call for the same owner: refresh its deadline rather than stacking a second token that
	// its single EndBoardAction would never release.
	for (FML_BoardActionToken& Token : Tokens)
	{
		if (Token.Owner.Get() == Owner)
		{
			Token.Reason = Reason;
			Token.ExpireTime = ExpireTime;
			return;
		}
	}

	const bool bWasIdle = Tokens.Num() == 0;

	FML_BoardActionToken& NewToken = Tokens.AddDefaulted_GetRef();
	NewToken.Owner = Owner;
	NewToken.Reason = Reason;
	NewToken.ExpireTime = ExpireTime;

	UE_LOG(LogMycelandBoardAction, Verbose, TEXT("Board locked by '%s' (%s). Tokens: %d"),
		*Owner->GetName(), LexReason(Reason), Tokens.Num());

	ApplyInputLock();

	if (bWasIdle)
		OnBoardBusyChanged.Broadcast(true);
}

void UML_BoardActionSubsystem::EndBoardAction(UObject* Owner)
{
	if (!Owner) return;

	const int32 Removed = Tokens.RemoveAll([Owner](const FML_BoardActionToken& Token)
	{
		return Token.Owner.Get() == Owner;
	});

	if (Removed == 0) return;

	UE_LOG(LogMycelandBoardAction, Verbose, TEXT("'%s' released the board. Tokens left: %d"),
		*Owner->GetName(), Tokens.Num());

	ApplyInputLock();

	if (Tokens.Num() == 0)
		OnBoardBusyChanged.Broadcast(false);
}

void UML_BoardActionSubsystem::PruneTokens()
{
	const UWorld* World = GetWorld();
	if (!World) return;

	const double Now = World->GetTimeSeconds();

	Tokens.RemoveAll([Now](const FML_BoardActionToken& Token)
	{
		// An owner destroyed mid-animation (reset, level teardown) releases its token by dying.
		if (!Token.Owner.IsValid())
			return true;

		if (Now < Token.ExpireTime)
			return false;

		UE_LOG(LogMycelandBoardAction, Warning,
			TEXT("Board action '%s' (%s) never called EndBoardAction: releasing its lock on the timeout. ")
			TEXT("This is a bug -- the animation that took this token has no end event."),
			*Token.Owner->GetName(), LexReason(Token.Reason));
		return true;
	});
}

void UML_BoardActionSubsystem::ApplyInputLock()
{
	AML_PlayerController* PlayerController = GetPlayerController();
	if (!PlayerController) return;

	const bool bShouldDisable = Tokens.Num() > 0;

	if (bShouldDisable)
	{
		PlayerController->DisableInput(PlayerController);
		bInputDisabledByUs = true;
		return;
	}

	if (!bInputDisabledByUs) return;

	bInputDisabledByUs = false;

	// The win sequence and cinematics take the input lock for themselves and release it on their own
	// schedule. Handing the input back here would cut them off mid-sequence.
	if (IsInputLockOwnedElsewhere()) return;

	PlayerController->EnableInput(PlayerController);
}

bool UML_BoardActionSubsystem::IsInputLockOwnedElsewhere() const
{
	const UWorld* World = GetWorld();
	if (!World) return false;

	if (const UML_WinLoseSubsystem* WinLoseSubsystem = World->GetSubsystem<UML_WinLoseSubsystem>())
	{
		if (WinLoseSubsystem->IsWinSequenceActive())
			return true;
	}

	if (const UML_CinematicSubsystem* CinematicSubsystem = World->GetSubsystem<UML_CinematicSubsystem>())
	{
		if (CinematicSubsystem->IsCinematicPlaying())
			return true;
	}

	return false;
}

AML_PlayerController* UML_BoardActionSubsystem::GetPlayerController() const
{
	const UWorld* World = GetWorld();
	return World ? Cast<AML_PlayerController>(World->GetFirstPlayerController()) : nullptr;
}
