// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ML_CoreData.h"
#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "GameFramework/PlayerController.h"
#include "Component/ML_EnergyComponent.h"
#include "Component/ML_HoverPreviewComponent.h"
#include "Component/ML_MoveRecordingComponent.h"
#include "Component/ML_BoardTransitionComponent.h"
#include "ML_PlayerController.generated.h"

class UML_MycelandDeveloperSettings;
struct FInputActionValue;
class AML_PlayerCharacter;
class AML_BoardSpawner;
class AML_Tile;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGrassPlanted, AML_Tile*, PlantedTile);

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

	// Free movement target (SimpleMoveToLocation)
	UPROPERTY(Transient)
	FVector PendingFreeMovementTarget = FVector::ZeroVector;

	bool bHasFreeMovementTarget = false;
	bool bIsUsingNavMeshMovement = false;
	float FollowTime = 0.f;
	FVector HoldMoveCachedDestination = FVector::ZeroVector;

	
	
	// ==================== Helpers ====================

	void SetIsMoving(bool bNewIsMoving);

public:
	AML_Tile* GetTileUnderCursor() const;

	/** Called by TransitionComponent when turn-toward-tile completes. */
	void ConfirmTurn(AML_Tile* HitTile);

	/** Called by TransitionComponent (ConfirmExitBoard, HandlePathFinished). */
	void StartMoveAlongPath(const TArray<FIntPoint>& AxialPath, const TMap<FIntPoint, AML_Tile*>& GridMap);

	/** Called by TransitionComponent (ConfirmExitBoard, HandlePathFinished). */
	void StartNavMeshMovement(const FVector& WorldLocation);

	/** Called by TransitionComponent to change the movement mode and notify other systems. */
	void SetMovementMode(EML_PlayerMovementMode NewMode);

private:
	// ==================== Ground ====================
	
	bool IsClickableGround(const FHitResult& Hit) const;

	
	
	// ==================== Movement ====================

	void TickMoveAlongPath(float DeltaTime);
	void TickNavMeshMovement(float DeltaTime);
	void OnPathFinished();
	void StopNavMeshMovement();
	bool Move(AML_Tile* TargetTile, int32 StopBeforeTarget = 0);
	bool Plant(AML_Tile* TargetTile);
	void ExecutePlant(AML_Tile* HitTile);
	bool StartRecordedBoardMove(const TArray<FIntPoint>& AxialPath, const TMap<FIntPoint, AML_Tile*>& GridMap,
		EML_PlayerBoardActionState ActionState = EML_PlayerBoardActionState::Moving, AML_Tile* PlantTarget = nullptr);

	/**
	 * Redirects the active in-progress world-space path to follow FullMergedAxialPath,
	 * while preserving the exact logical target index currently being aimed at.
	 */
	void ExtendMoveAlongPath(const TArray<FIntPoint>& FullMergedAxialPath, const TMap<FIntPoint, AML_Tile*>& GridMap,
		int32 PreservedPathIndex);

	
	
	// ==================== Delegates ====================
	
	UFUNCTION()
	void HandleCurrentTileChanged(const AML_Tile* OldTile, const AML_Tile* NewTile);

	UFUNCTION()
	void HandleBoardStateChanged(const AML_Tile* OldTile, const AML_Tile* NewTile);

protected:
	// ==================== Lifecycle ====================

	AML_PlayerController();
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void OnPossess(APawn* aPawn) override;

	
	
	// ==================== Input ====================

	// Bind to OnStarted — one shot per click (BFS, exit hold trigger, board re-entry)
	UFUNCTION(BlueprintCallable, Category = "Myceland Controller")
	void OnSetDestinationStarted();

	// Bind to OnTriggered - maintain click when outside the board to move
	UFUNCTION(BlueprintCallable, Category = "Myceland Controller")
	void OnSetDestinationTriggered();

	// Bind to OnCompleted / OnCanceled
	UFUNCTION(BlueprintCallable, Category = "Myceland Controller")
	void OnSetDestinationReleased();

	// Bind to OnStarted - for Plant and Move only
	UFUNCTION(BlueprintCallable, Category = "Myceland Controller")
	void OnMoveAndPlantStarted();

	// ---- Input sub-handlers (called by OnSetDestinationStarted) ----
	void HandleInsideBoardClick();
	void HandleFreeMovementClick();

	
	
	// ==================== Movement Tuning ====================

	UPROPERTY(EditAnywhere, Category = "Myceland|Movement")
	float AcceptanceRadius = 12.f;

	UPROPERTY(EditAnywhere, Category = "Myceland|Movement")
	float MoveSpeedScale = 1.f;

	UPROPERTY(EditAnywhere, Category = "Myceland|Movement")
	float RotateSpeed = 10.f;

	// 0 = strict center-to-center, 1 = maximum smoothing
	UPROPERTY(EditAnywhere, Category = "Myceland|Movement|Smoothing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CornerCutStrength = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Myceland|Movement|Smoothing", meta = (ClampMin = "0.0"))
	float BaseCornerCutDistance = 80.f;
	
	// Nav Mesh Movement
	UPROPERTY(EditAnywhere, Category = "Myceland|Movement|NavMesh")
	float NavMeshAcceptanceRadius = 50.f;
	
	UPROPERTY(EditAnywhere, Category = "Myceland|Movement")
	float ShortPressThreshold = 0.5f;

public:
	// ==================== Components ====================

	UPROPERTY(BlueprintReadOnly, Category = "Myceland Controller|Components")
	UML_EnergyComponent* EnergyComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Myceland Controller|Components")
	UML_HoverPreviewComponent* HoverPreviewComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Myceland Controller|Components")
	UML_MoveRecordingComponent* MoveRecordingComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Myceland Controller|Components")
	UML_BoardTransitionComponent* TransitionComponent = nullptr;
	
	
	
	// ==================== Delegates ====================
	
	// Called when grass is successfully planted on a tile
	UPROPERTY(BlueprintAssignable, Category = "Myceland Controller|Plant")
	FOnGrassPlanted OnGrassPlanted;

	
	
	// ==================== Actions ====================
	
	UFUNCTION(BlueprintCallable, Category="Myceland Controller")
	bool MovePlayerToAxial(const FIntPoint& TargetAxial, bool bUsePath, bool bFallbackTeleport, const FVector& TeleportFallbackWorld);

	UFUNCTION(BlueprintCallable, Category="Myceland Controller|Undo")
	void StartMoveAlongAxialPathForUndo(const TArray<FIntPoint>& AxialPath, const TArray<FIntPoint>& PickedCollectibleAxials);

	void NotifyCollectiblePickedOnAxial(const FIntPoint& Axial);

	bool IsMoveInProgress() const { return MoveRecordingComponent && MoveRecordingComponent->IsMoveInProgress(); }
	bool IsUndoMovePlayback() const { return MoveRecordingComponent && MoveRecordingComponent->IsUndoMovePlayback(); }
};
