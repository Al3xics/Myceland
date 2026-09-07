// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ML_EnergyComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnergyChanged, int32, NewEnergy);

UCLASS(ClassGroup=(Myceland), meta=(BlueprintSpawnableComponent))
class MYCELAND_API UML_EnergyComponent : public UActorComponent
{
	GENERATED_BODY()

private:

	UPROPERTY()
	int32 CurrentEnergy = 0;

	// ---------- Infinite energy cheat (UML_CheatSubsystem) ----------
	// Off outside the demo cheat mode, and the only thing it does is floor the stock in
	// SetCurrentEnergy, so every path that would lower it is covered at once.

	UPROPERTY()
	bool bInfiniteEnergy = false;

	UPROPERTY()
	int32 InfiniteEnergyAmount = 99;

	// Stock the player would have without the cheat, handed back when it is switched off.
	UPROPERTY()
	int32 EnergyWithoutCheat = 0;

public:

	UPROPERTY(BlueprintAssignable, Category = "Energy Component|Delegates")
	FOnEnergyChanged OnEnergyChanged;

	UFUNCTION(BlueprintCallable, Category = "Energy Component|Energy")
	int32 GetCurrentEnergy() const { return CurrentEnergy; }

	UFUNCTION(BlueprintCallable, Category = "Energy Component|Energy")
	void SetCurrentEnergy(int32 NewEnergy);

	UFUNCTION(BlueprintCallable, Category = "Energy Component|Energy")
	void AddEnergy(int32 Delta);

	UFUNCTION(BlueprintCallable, Category = "Energy Component|Energy")
	void InitNumberOfEnergyForLevel(int32 Energy);

	// ==================== Cheat ====================

	// Demo cheat: floors the energy stock at Amount so nothing can drain it. Switching it back off
	// restores the stock the player would have had (the budget of the board they are on).
	UFUNCTION(BlueprintCallable, Category = "Energy Component|Cheat")
	void SetInfiniteEnergy(bool bEnabled, int32 Amount = 99);

	UFUNCTION(BlueprintPure, Category = "Energy Component|Cheat")
	bool IsInfiniteEnergy() const { return bInfiniteEnergy; }
};
