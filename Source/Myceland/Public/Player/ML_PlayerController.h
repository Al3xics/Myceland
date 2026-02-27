// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ML_CoreData.h"
#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "GameFramework/PlayerController.h"
#include "ML_PlayerController.generated.h"

class UML_MycelandDeveloperSettings;
struct FInputActionValue;
class AML_PlayerCharacter;
class AML_BoardSpawner;
class AML_Tile;

UCLASS()
class MYCELAND_API AML_PlayerController : public APlayerController
{
	GENERATED_BODY()

private:

	// ==================== References ====================

	UPROPERTY()
	const UML_MycelandDeveloperSettings* DevSettings;

	UPROPERTY()
	AML_PlayerCharacter* MycelandCharacter;

	// ==================== State ====================

	TArray<FVector> CurrentPathWorld;
	int32 CurrentPathIndex = 0;

	bool bIsMoving = false;
	EML_PlayerMovementMode CurrentMovementMode = EML_PlayerMovementMode::InsideBoard;

	// Exit hold
	float ExitHoldTimer = 0.f;
	bool bIsHoldingExitInput = false;
	bool bIsHoldingFreeInput = false;

	UPROPERTY(Transient)
	AML_Tile* PendingExitTile = nullptr;

	// Board entry
	bool bPendingFreeMovementOnArrival = false;
	bool bPendingBoardEntryOnArrival = false;

	UPROPERTY(Transient)
	AML_Tile* PendingBoardEntryTargetTile = nullptr;
	
	// Move and Plant
	UPROPERTY(Transient)
	AML_Tile* PendingPlantTargetTile = nullptr;
	
	bool bPendingPlantOnArrival = false;

	// ==================== Undo ====================

	// ---- Move recording for Undo ----
	bool bMoveInProgress = false;

	FIntPoint MoveStartAxial = FIntPoint::ZeroValue;
	FVector   MoveStartWorld = FVector::ZeroVector;

	FIntPoint MoveEndAxial   = FIntPoint::ZeroValue;
	FVector   MoveEndWorld   = FVector::ZeroVector;

	TArray<FIntPoint> ActiveMoveAxialPath;

	UPROPERTY(Transient)
	TSet<FIntPoint> ActiveMovePickedCollectibles;

	// Flags
	bool bSuppressMoveRecording = false;
	bool bUndoMovePlayback = false;

	// ---- Undo move collectible restore ----
	UPROPERTY(Transient)
	TSet<FIntPoint> UndoMoveRemainingCollectibles;

	bool bUndoRestoreCollectibles = false;

	// ==================== Helpers ====================

	AML_Tile* GetTileUnderCursor() const;
	bool IsTileWalkable(const AML_Tile* Tile) const;
	AML_Tile* FindNearestWalkableTile(const FVector& WorldLocation, const TMap<FIntPoint, AML_Tile*>& GridMap) const;

	// ==================== Pathfinding ====================

	bool BuildPath_AxialBFS(const FIntPoint& StartAxial, const FIntPoint& GoalAxial, const TMap<FIntPoint, AML_Tile*>& GridMap, TArray<FIntPoint>& OutAxialPath) const;

	// ==================== Movement ====================

	void StartMoveAlongPath(const TArray<FIntPoint>& AxialPath, const TMap<FIntPoint, AML_Tile*>& GridMap);
	void StartMoveToWorldLocation(const FVector& WorldLocation);
	void TickMoveAlongPath(float DeltaTime);
	void OnPathFinished();

	// ==================== Board Exit / Entry ====================

	void TickExitHold(float DeltaTime);
	void ConfirmExitBoard();

	// ==================== Delegates ====================

	UFUNCTION()
	void HandleBoardStateChanged(const AML_Tile* NewTile);

protected:

	// ==================== Lifecycle ====================

	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void OnPossess(APawn* aPawn) override;

	// ==================== Input ====================

	// Bind to OnStarted  — one shot per click (BFS, exit hold trigger, board re-entry)
	UFUNCTION(BlueprintCallable, Category = "Myceland Controller")
	void OnSetDestinationStarted();

	// Bind to OnTriggered — every frame while held (continuous free movement)
	UFUNCTION(BlueprintCallable, Category = "Myceland Controller")
	void OnSetDestinationTriggered();

	// Bind to OnCompleted / OnCanceled
	UFUNCTION(BlueprintCallable, Category = "Myceland Controller")
	void OnSetDestinationReleased();
	
	// Bind to OnStarted - for Plant and Move only
	UFUNCTION(BlueprintCallable, Category = "Myceland Controller")
	void OnMoveAndPlantStarted();

	// ==================== Movement Tuning ====================

	UPROPERTY(EditAnywhere, Category = "Myceland|Movement")
	float AcceptanceRadius = 12.f;

	UPROPERTY(EditAnywhere, Category = "Myceland|Movement")
	float MoveSpeedScale = 1.f;

	// 0 = strict center-to-center, 1 = maximum smoothing
	UPROPERTY(EditAnywhere, Category = "Myceland|Movement|Smoothing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CornerCutStrength = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Myceland|Movement|Smoothing", meta = (ClampMin = "0.0"))
	float BaseCornerCutDistance = 80.f;
	
	// ==================== Hover Preview ====================
    
	UPROPERTY(Transient)
	AML_Tile* LastHoveredTile = nullptr;
    
	UPROPERTY(Transient)
	TArray<AML_Tile*> CurrentPreviewPath;
    
	void TickHoverPreview(float DeltaTime);
	void ClearHoverPreview();
	TArray<AML_Tile*> BuildPreviewPath(const AML_Tile* TargetTile) const;

public:

	// ==================== Energy ====================

	UPROPERTY(BlueprintReadWrite, Category = "Myceland Controller|Energy")
	int CurrentEnergy = 0;

	UFUNCTION(BlueprintCallable, Category = "Myceland Controller|Energy")
	void InitNumberOfEnergyForLevel(int32 Energy);
	
	// ==================== Hover Preview Events (for Blueprints) ====================
    
	// Called when hover path changes
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Myceland Controller|Hover")
	void OnHoverPathUpdated(const TArray<AML_Tile*>& PathTiles);
    
	// Called when hover is cleared
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Myceland Controller|Hover")
	void OnHoverPathCleared();

	// ==================== Actions ====================

	UFUNCTION(BlueprintCallable, Category = "Myceland Controller")
	void TryPlantGrass(FHitResult HitResult, bool& CanPlantGrass, AML_Tile*& HitTile);

	UFUNCTION(BlueprintCallable, Category = "Myceland Controller")
	void ConfirmTurn(AML_Tile* HitTile);
	
	UFUNCTION(BlueprintCallable, Category="Myceland Controller|Movement")
	bool MovePlayerToAxial(const FIntPoint& TargetAxial, bool bUsePath, bool bFallbackTeleport, const FVector& TeleportFallbackWorld);

	UFUNCTION(BlueprintCallable, Category="Myceland|Undo")
	void StartMoveAlongAxialPathForUndo(const TArray<FIntPoint>& AxialPath, const TArray<FIntPoint>& PickedCollectibleAxials);

	void NotifyCollectiblePickedOnAxial(const FIntPoint& Axial);

	bool IsMoveInProgress() const { return bMoveInProgress; }
	bool IsUndoMovePlayback() const { return bUndoMovePlayback; }
};