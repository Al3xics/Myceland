// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ML_BoardActionSubsystem.generated.h"

class AML_PlayerController;
class UML_CinematicSubsystem;
class UML_WinLoseSubsystem;

/**
 * Why a board action holds the lock. Purely informative: every reason locks the board the same way.
 * It only exists so the timeout warning can name what forgot to release its token.
 */
UENUM(BlueprintType)
enum class EML_BoardActionReason : uint8
{
	Wave            UMETA(DisplayName = "Wave Propagation"),
	SpawnAnimation  UMETA(DisplayName = "Spawn Animation"),
	Undo            UMETA(DisplayName = "Undo"),
	Reset           UMETA(DisplayName = "Reset"),
	Other           UMETA(DisplayName = "Other")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FML_OnBoardBusyChanged, bool, bIsBusy);

/**
 * Tracks whether a board action (a turn) is still resolving, and owns the player input lock while it is.
 *
 * The board is busy as long as at least one token is held. Tokens compose: the wave takes one for the
 * whole propagation, each animation spawned by that propagation takes its own, and the player only gets
 * the input back when the LAST one is released. That overlap is what removes the gap between the end of
 * the propagation and the start of the animations it triggered.
 *
 * A token is keyed by its owner, held as a weak pointer: an actor destroyed mid-animation drops its token
 * on its own. The timeout is a bug detector, not a mechanism -- if it ever fires, something forgot to
 * call EndBoardAction and the log says who.
 */
UCLASS()
class MYCELAND_API UML_BoardActionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UML_BoardActionSubsystem* Get(const UObject* WorldContextObject);

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	/**
	 * Take a token: the board stays busy until Owner releases it. Calling it twice for the same owner
	 * only refreshes its timeout, so a re-entrant caller can't leak a second token it will never release.
	 */
	UFUNCTION(BlueprintCallable, Category = "Myceland Board Action", meta = (DefaultToSelf = "Owner", AdvancedDisplay = "TimeoutSeconds"))
	void BeginBoardAction(UObject* Owner, EML_BoardActionReason Reason = EML_BoardActionReason::Other, float TimeoutSeconds = 10.f);

	/** Release Owner's token. Harmless if it holds none. */
	UFUNCTION(BlueprintCallable, Category = "Myceland Board Action", meta = (DefaultToSelf = "Owner"))
	void EndBoardAction(UObject* Owner);

	/** True while at least one action is still resolving: no move, no plant, no undo, no reset. */
	UFUNCTION(BlueprintPure, Category = "Myceland Board Action")
	bool IsBoardBusy() const { return Tokens.Num() > 0; }

	/** Fires on the transitions only (idle -> busy, busy -> idle), never on intermediate tokens. */
	UPROPERTY(BlueprintAssignable, Category = "Myceland Board Action")
	FML_OnBoardBusyChanged OnBoardBusyChanged;

private:
	struct FML_BoardActionToken
	{
		TWeakObjectPtr<UObject> Owner;
		EML_BoardActionReason Reason = EML_BoardActionReason::Other;
		double ExpireTime = 0.0;
	};

	TArray<FML_BoardActionToken> Tokens;

	/** True while WE are the ones holding the input disabled, so we never re-enable input we didn't take. */
	bool bInputDisabledByUs = false;

	/** Drop tokens whose owner died, and time out the ones that were never released. */
	void PruneTokens();

	/** Re-assert the input lock from the current token count. */
	void ApplyInputLock();

	/** Win sequence and cinematics own the input lock themselves; we must not steal it back from them. */
	bool IsInputLockOwnedElsewhere() const;

	AML_PlayerController* GetPlayerController() const;
};
