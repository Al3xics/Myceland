// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ML_PlayerController.generated.h"

struct FInputActionValue;
class AML_PlayerCharacter;
class AML_BoardSpawner;
class AML_Tile;

UCLASS()
class MYCELAND_API AML_PlayerController : public APlayerController
{
public:

private:
	GENERATED_BODY()

private:
	// Path in world space (tile centers)
	TArray<FVector> CurrentPathWorld;
	int32 CurrentPathIndex = 0;

	// ==================== Helpers ====================
	AML_Tile* GetTileUnderCursor() const;
	AML_BoardSpawner* GetBoardFromCurrentTile(const AML_PlayerCharacter* MycelandCharacter) const;
	bool IsTileWalkable(const AML_Tile* Tile) const;
	bool BuildPath_AxialBFS(const FIntPoint& StartAxial, const FIntPoint& GoalAxial, const TMap<FIntPoint, AML_Tile*>& GridMap, TArray<FIntPoint>& OutAxialPath) const;
	void StartMoveAlongPath(const TArray<FIntPoint>& AxialPath, const TMap<FIntPoint, AML_Tile*>& GridMap);
	void TickMoveAlongPath(float DeltaTime);
	void InitNumberOfEnergyForLevel();
	
protected:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Myceland Controller")
	void OnSetDestinationTriggered();

	// ==================== Movement tuning ====================
	UPROPERTY(EditAnywhere, Category="Myceland|Movement")
	float AcceptanceRadius = 12.f;

	UPROPERTY(EditAnywhere, Category="Myceland|Movement")
	float MoveSpeedScale = 1.f;

	// 0 = strict center-to-center, 1 = maximum smoothing (skip corners easily)
	UPROPERTY(EditAnywhere, Category="Myceland|Movement|Smoothing", meta=(ClampMin="0.0", ClampMax="1.0"))
	float CornerCutStrength = 0.5f;

	// Distance (in cm) used to decide if we can skip a waypoint (scaled by strength)
	UPROPERTY(EditAnywhere, Category="Myceland|Movement|Smoothing", meta=(ClampMin="0.0"))
	float BaseCornerCutDistance = 80.f;

public:
	UPROPERTY(BlueprintReadWrite, Category="Myceland Controller|Energy")
	int CurrentEnergy = 0;
	
	UFUNCTION(BlueprintCallable, Category = "Myceland Controller")
	void TryPlantGrass(FHitResult HitResult, bool& CanPlantGrass, AML_Tile*& HitTile);
	
	UFUNCTION(BlueprintCallable, Category = "Myceland Controller")
	void ConfirmTurn(AML_Tile* HitTile);
};
