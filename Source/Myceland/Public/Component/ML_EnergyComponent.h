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

public:

	UPROPERTY(BlueprintAssignable, Category = "Myceland|Energy")
	FOnEnergyChanged OnEnergyChanged;

	UFUNCTION(BlueprintCallable, Category = "Myceland|Energy")
	int32 GetCurrentEnergy() const { return CurrentEnergy; }

	UFUNCTION(BlueprintCallable, Category = "Myceland|Energy")
	void SetCurrentEnergy(int32 NewEnergy);

	UFUNCTION(BlueprintCallable, Category = "Myceland|Energy")
	void AddEnergy(int32 Delta);

	UFUNCTION(BlueprintCallable, Category = "Myceland|Energy")
	void InitNumberOfEnergyForLevel(int32 Energy);
};
