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
#include "Component/ML_NavigationBridgeComponent.h"
#include "Input/ML_InputDeviceManager.h"
#include "Input/Handlers/ML_MouseKeyboardInputHandler.h"
#include "Input/Handlers/ML_GamepadInputHandler.h"
#include "ML_PlayerController.generated.h"

class AML_CameraRail;
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
	bool bInCinematicMode = false;

	// Last device seen by HandleInputDeviceChanged — used to detect Gamepad→MK transitions.
	EML_InputDevice PreviousInputDevice = EML_InputDevice::MouseKeyboard;

	// When true, the next MK input action is silently consumed (cursor just reappeared).
	bool bShouldConsumeNextInput = false;

	// Cursor position saved when switching to gamepad, restored when showing the cursor again.
	FVector2D LockedCursorPos = FVector2D::ZeroVector;

	// ==================== Movement - Path Tick & Callbacks ====================

	void TickMoveAlongPath(float DeltaTime);
	void OnPathFinished();
	void SetIsMoving(bool bNewIsMoving);

	// ==================== Movement - Path Management ====================

	bool StartRecordedBoardMove(const TArray<FIntPoint>& AxialPath, const TMap<FIntPoint, AML_Tile*>& GridMap,
		EML_PlayerBoardActionState ActionState = EML_PlayerBoardActionState::Moving, AML_Tile* PlantTarget = nullptr);

	/**
	 * Redirects the active in-progress world-space path to follow FullMergedAxialPath,
	 * while preserving the exact logical target index currently being aimed at.
	 */
	void ExtendMoveAlongPath(const TArray<FIntPoint>& FullMergedAxialPath, const TMap<FIntPoint, AML_Tile*>& GridMap,
		int32 PreservedPathIndex);

	// ==================== Movement - Tile Movement (private helpers) ====================

	void ExecutePlant(AML_Tile* HitTile);
	bool RejectPlantWithFeedback();

	// ==================== Camera Queries ====================

	AML_CameraRail* FindClosestCameraRailFromPlayer(const FVector& WorldLocation);

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

	// Bind to IA_Move → Started
	UFUNCTION(BlueprintCallable, Category = "Myceland Controller")
	void OnSetDestinationStarted();

	// Bind to IA_Move → Triggered
	UFUNCTION(BlueprintCallable, Category = "Myceland Controller")
	void OnSetDestinationTriggered();

	// Bind to IA_Move → Completed / Canceled
	UFUNCTION(BlueprintCallable, Category = "Myceland Controller")
	void OnSetDestinationReleased();

	// Bind to IA_MoveAndPlant → Started
	UFUNCTION(BlueprintCallable, Category = "Myceland Controller")
	void OnMoveAndPlantStarted();

	// Bind to IA_GamepadConfirm → Started (gamepad button — replaces IA_Move gamepad binding)
	UFUNCTION(BlueprintCallable, Category = "Myceland Controller")
	void OnGamepadConfirmStarted();

	// Bind to IA_GamepadMoveAndPlant → Started (gamepad button — replaces IA_MoveAndPlant gamepad binding)
	UFUNCTION(BlueprintCallable, Category = "Myceland Controller")
	void OnGamepadMoveAndPlantStarted();

	// Bind to IA_GamepadMove → Triggered (Axis2D, left stick)
	UFUNCTION(BlueprintCallable, Category = "Myceland Controller")
	void OnGamepadMoveAxis(const FVector2D& Value);

	UFUNCTION(BlueprintCallable, Category = "Myceland Controller")
	void OnGamepadMoveReleased();

	UFUNCTION(BlueprintCallable, Category = "Myceland Controller")
	void OnSkipNarrativeLine();

	// Reacts to input device switches to update cursor visibility.
	UFUNCTION()
	void HandleInputDeviceChanged(EML_InputDevice NewDevice);

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
	// ==================== Tile Query ====================

	AML_Tile* GetTileUnderCursor() const;

	static AML_Tile* ExtractTileFromHit(const FHitResult& Hit);

	// ==================== Ground Validation ====================

	bool IsClickableGround(const FHitResult& Hit) const;

	// ==================== Character Access ====================

	AML_PlayerCharacter* GetMycelandCharacter() const { return MycelandCharacter; }

	// ==================== Movement - Tile Movement ====================

	bool Move(AML_Tile* TargetTile, int32 StopBeforeTarget = 0);
	bool Plant(AML_Tile* TargetTile);

	// ==================== Components ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Myceland Controller|Components")
	UML_EnergyComponent* EnergyComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Myceland Controller|Components")
	UML_HoverPreviewComponent* HoverPreviewComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Myceland Controller|Components")
	UML_MoveRecordingComponent* MoveRecordingComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Myceland Controller|Components")
	UML_BoardTransitionComponent* TransitionComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Myceland Controller|Components")
	UML_NavigationBridgeComponent* NavigationBridgeComponent = nullptr;

	// ==================== Input Components ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Myceland Controller|Input")
	UML_InputDeviceManager* InputDeviceManager = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Myceland Controller|Input")
	UML_MouseKeyboardInputHandler* MouseKeyboardHandler = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Myceland Controller|Input")
	UML_GamepadInputHandler* GamepadHandler = nullptr;

	// ==================== Delegates ====================

	// Called when grass is successfully planted on a tile
	UPROPERTY(BlueprintAssignable, Category = "Myceland Controller|Plant")
	FOnGrassPlanted OnGrassPlanted;

	// ==================== Camera ====================

	UFUNCTION(BlueprintCallable, Category="Myceland Controller|Camera")
	void BlendToViewTarget(AActor* NewViewTarget, float BlendTime = 2.f, EViewTargetBlendFunction BlendFunc = VTBlend_Linear);

	// ==================== Movement Control ====================

	/** Called by TransitionComponent (ConfirmExitBoard, HandlePathFinished). */
	void StartMoveAlongPath(const TArray<FIntPoint>& AxialPath, const TMap<FIntPoint, AML_Tile*>& GridMap);

	/** Called by TransitionComponent (ConfirmExitBoard, HandlePathFinished). */
	void StartNavMeshMovement(const FVector& WorldLocation);

	/** Called by TransitionComponent to change the movement mode and notify other systems. */
	void SetMovementMode(EML_PlayerMovementMode NewMode);

	// ==================== Callbacks & State Management ====================

	/** Called by TransitionComponent when turn-toward-tile completes. */
	void ConfirmTurn(AML_Tile* HitTile);

	void UpdateCursorVisibility(const bool bVisible);
	void NotifyCinematicModeChanged(const bool bInCinematicMode);

	// ==================== Actions ====================

	UFUNCTION(BlueprintCallable, Category="Myceland Controller")
	bool MovePlayerToAxial(const FIntPoint& TargetAxial, bool bUsePath, bool bFallbackTeleport, const FVector& TeleportFallbackWorld);

	UFUNCTION(BlueprintCallable, Category="Myceland Controller|Undo")
	void StartMoveAlongAxialPathForUndo(const TArray<FIntPoint>& AxialPath, const TArray<FIntPoint>& PickedCollectibleAxials);

	void NotifyCollectiblePickedOnAxial(const FIntPoint& Axial);

	// ==================== Queries ====================

	bool IsMoveInProgress() const { return MoveRecordingComponent && MoveRecordingComponent->IsMoveInProgress(); }
	bool IsUndoMovePlayback() const { return MoveRecordingComponent && MoveRecordingComponent->IsUndoMovePlayback(); }
	
	float GetMoveSpeedScale() const { return MoveSpeedScale; }
	float GetShortPressThreshold() const { return ShortPressThreshold; }

	// ==================== Component Wrappers ====================
	// These allow components to call through the controller without cross-component includes.

	EML_PlayerMovementMode GetMovementMode() const { return TransitionComponent ? TransitionComponent->GetMovementMode() : EML_PlayerMovementMode::FreeMovement; }
	EML_PlayerBoardActionState GetBoardActionState() const { return TransitionComponent ? TransitionComponent->GetBoardActionState() : EML_PlayerBoardActionState::Idle; }
	bool IsHoldingExitInput() const { return TransitionComponent && TransitionComponent->IsHoldingExitInput(); }
	bool HasEnergy()const { return EnergyComponent && EnergyComponent->GetCurrentEnergy() > 0; }

	void RequestExitHold(AML_Tile* ExitBorderTile, const FVector& WorldTarget);
	void CancelExitHold();
	void RequestBoardEntry(AML_Tile* TargetTile);
	void StopNavMeshMovement();

	/** Stops NavMesh movement and cancels any pending board entry. Called when a cinematic interrupts navigation. */
	void CancelPendingNavigation();
	AML_Tile* FindReachableExitBorderTile(const AML_BoardSpawner* Board, const FVector& OutsideDestination) const;
	AML_Tile* PredictNavMeshEntryTile(const AML_BoardSpawner* Board, const FVector& Destination) const;
	void SetForcedHoverTile(AML_Tile* Tile);
	void ClearForcedHoverTile();
	void ClearPathHoverPreview();

	/** Clears the currently active glow (cursor + path). Called by AML_BoardSpawner when glow is toggled OFF. */
	void ClearActiveGlow();

	/**
	 * Called by AML_BoardSpawner when its transition is toggled OFF. If the player is currently inside
	 * that board, stops any board movement and ejects them back to free movement.
	 */
	void NotifyBoardTransitionDisabled(const AML_BoardSpawner* Board);
};
