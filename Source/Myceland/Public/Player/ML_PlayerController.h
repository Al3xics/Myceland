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
	GENERATED_BODY()

private:
	// World-space path (tile centers)
	TArray<FVector> CurrentPathWorld;
	int32 CurrentPathIndex = 0;

	// Helpers
	AML_Tile* GetTileUnderCursor() const;
	AML_BoardSpawner* GetBoardFromCurrentTile(const AML_PlayerCharacter* MycelandCharacter) const;
	bool IsTileWalkable(const AML_Tile* Tile) const;
	bool BuildPath_AxialBFS(const FIntPoint& StartAxial, const FIntPoint& GoalAxial, const TMap<FIntPoint, AML_Tile*>& GridMap, TArray<FIntPoint>& OutAxialPath) const;
	void StartMoveAlongPath(const TArray<FIntPoint>& AxialPath, const TMap<FIntPoint, AML_Tile*>& GridMap);
	void TickMoveAlongPath(float DeltaTime);
	void InitNumberOfEnergyForLevel();

	// ---- Move recording for Undo ----
	bool bMoveInProgress = false;

	FIntPoint MoveStartAxial = FIntPoint::ZeroValue;
	FVector   MoveStartWorld = FVector::ZeroVector;

	FIntPoint MoveEndAxial   = FIntPoint::ZeroValue;
	FVector   MoveEndWorld   = FVector::ZeroVector;

	TArray<FIntPoint> ActiveMoveAxialPath;

	UPROPERTY(Transient)
	TSet<FIntPoint> ActiveMovePickedCollectibles;

	// flags
	bool bSuppressMoveRecording = false;
	bool bUndoMovePlayback = false;

	// ---- Undo move collectible restore ----
	UPROPERTY(Transient)
	TSet<FIntPoint> UndoMoveRemainingCollectibles;

	bool bUndoRestoreCollectibles = false;

protected:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category="Myceland Controller")
	void OnSetDestinationTriggered();

	UPROPERTY(EditAnywhere, Category="Myceland|Movement")
	float AcceptanceRadius = 12.f;

	UPROPERTY(EditAnywhere, Category="Myceland|Movement")
	float MoveSpeedScale = 1.f;

	UPROPERTY(EditAnywhere, Category="Myceland|Movement|Smoothing", meta=(ClampMin="0.0", ClampMax="1.0"))
	float CornerCutStrength = 0.5f;

	UPROPERTY(EditAnywhere, Category="Myceland|Movement|Smoothing", meta=(ClampMin="0.0"))
	float BaseCornerCutDistance = 80.f;

public:
	UPROPERTY(BlueprintReadWrite, Category="Myceland Controller|Energy")
	int CurrentEnergy = 0;

	UFUNCTION(BlueprintCallable, Category="Myceland Controller")
	void TryPlantGrass(FHitResult HitResult, bool& CanPlantGrass, AML_Tile*& HitTile);

	UFUNCTION(BlueprintCallable, Category="Myceland Controller")
	void ConfirmTurn(AML_Tile* HitTile);

	UFUNCTION(BlueprintCallable, Category="Myceland Controller|Movement")
	bool MovePlayerToAxial(const FIntPoint& TargetAxial, bool bUsePath, bool bFallbackTeleport, const FVector& TeleportFallbackWorld);

	UFUNCTION(BlueprintCallable, Category="Myceland|Undo")
	void StartMoveAlongAxialPathForUndo(
		const TArray<FIntPoint>& AxialPath,
		const TArray<FIntPoint>& PickedCollectibleAxials
	);

	void NotifyCollectiblePickedOnAxial(const FIntPoint& Axial);

	bool IsMoveInProgress() const { return bMoveInProgress; }

	bool IsUndoMovePlayback() const { return bUndoMovePlayback; }
};