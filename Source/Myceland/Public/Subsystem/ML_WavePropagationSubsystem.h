// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ML_WavePropagationSubsystem.generated.h"

class AML_PlayerController;
class UML_PropagationRules;
class AML_BoardSpawner;
class UML_WinLoseSubsystem;

UCLASS()
class MYCELAND_API UML_WavePropagationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

private:
	UPROPERTY()
	UML_WinLoseSubsystem* WinLoseSubsystem;
	
	UPROPERTY()
	AML_BoardSpawner* BoardSpawner;
	
	UPROPERTY()
	TArray<UML_PropagationRules*> Rules;
	
	UPROPERTY(Transient)
	TMap<int32, FIntPoint> FutureChanges;
	
	UPROPERTY()
	AML_PlayerController* PlayerController = nullptr;
	
	bool bIsResolvingTiles = false;
	int ActiveTilUpdates = 0;
	
	void EndTileResolved();
	void ComputesChanges();
	void RunWave();
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
	UFUNCTION(BlueprintCallable, Category="Myceland Wave Propagation")
	void RunFullCycle();
	
	UFUNCTION(BlueprintCallable, Category="Myceland Wave Propagation")
	void BeginTileResolved();
	
	UFUNCTION(BlueprintCallable, Category="Myceland Wave Propagation")
	void RegisterTileUpdate();
	
	UFUNCTION(BlueprintCallable, Category="Myceland Wave Propagation")
	void UnregisterTileUpdate();
	
	UFUNCTION(BlueprintCallable, Category="Myceland Wave Propagation")
	void SetInitialTileChanged(FIntPoint AxialCoord);

	UFUNCTION(BlueprintPure, Category="Myceland Wave Propagation")
	const TArray<UML_PropagationRules*>& GetRules() const { return Rules; }
};
