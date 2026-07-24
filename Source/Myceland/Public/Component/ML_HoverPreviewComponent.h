// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/ML_CoreData.h"
#include "ML_HoverPreviewComponent.generated.h"

class AML_BoardSpawner;
class AML_PlayerController;
class AML_PlayerCharacter;
class AML_Tile;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHoveredTileChanged, AML_Tile*, HoveredTile, bool, bIsReachable);

UCLASS(ClassGroup=(Myceland), meta=(BlueprintSpawnableComponent))
class MYCELAND_API UML_HoverPreviewComponent : public UActorComponent
{
	GENERATED_BODY()

private:

	UPROPERTY(Transient)
	AML_PlayerController* OwningController = nullptr;

	UPROPERTY(Transient)
	AML_PlayerCharacter* PlayerCharacter = nullptr;

	UPROPERTY(Transient)
	AML_Tile* LastCursorHoveredTile = nullptr;

	// True when LastCursorHoveredTile is currently glowing as the player's own tile.
	// Lets us refresh its glow back to a normal cursor hover once the player leaves it.
	bool bLastCursorTileIsPlayerTile = false;

	UPROPERTY(Transient)
	AML_Tile* LastPathHoveredTile = nullptr;

	UPROPERTY(Transient)
	TArray<AML_Tile*> CurrentPreviewPath;

	// Gamepad only: the plantable neighbor tiles currently glowing around the player (GlowPlantableAvailable).
	UPROPERTY(Transient)
	TArray<AML_Tile*> HighlightedPlantables;

	// Gamepad only: the single tile currently selected (cursor glow) — plantable or not, the right stick can
	// hover any tile around the player. Owned here so the board visuals (plantable set + selection) live in one
	// place. The gamepad handler only sets/reads it; the plant button rejects it if it is not plantable.
	UPROPERTY(Transient)
	AML_Tile* GamepadSelectedTile = nullptr;

	// Gamepad only: the exit plane currently broadcast as available (player standing on its exit tile).
	UPROPERTY(Transient)
	AActor* CurrentExitPlane = nullptr;

	bool bCurrentHoveredTileReachable = false;

	EML_PlayerMovementMode CurrentMovementMode = EML_PlayerMovementMode::FreeMovement;

	FTimerHandle HoverPreviewTimerHandle;
	
	// Overrides cursor-based tile detection. When set, the hover preview (path glow + OnHoveredTileChanged)
	// is computed against this tile instead of whatever is under the cursor.
	// Used during board exit: the exit tile is forced so the path highlights even if the cursor is elsewhere.
	// Cleared when the exit is confirmed, canceled, or the player leaves board mode.
	UPROPERTY(Transient)
	AML_Tile* ForcedHoverTile = nullptr;
	
	bool bShowPreviews = true;

	// False when a gamepad is active: cursor position must not influence tile hover.
	bool bCursorHoverEnabled = true;

	void UpdateHoverPreview();
	void TickPathHoverPreview();
	void TickCursorHoverPreview();
	void ClearCursorHoverPreview();
	void SetHoveredTileState(AML_Tile* HoveredTile, bool bIsReachable);
	TArray<AML_Tile*> BuildPreviewPathFromTile(const AML_Tile* StartTile, const AML_Tile* TargetTile) const;

	// ===== Gamepad board visuals (plantable highlight + selection) =====

	/** Rebuilds the plantable-neighbor glow around the player (gamepad + inside board only). */
	void RefreshPlantableHighlight();

	/** Stops glowing every currently-highlighted plantable tile. */
	void ClearPlantableHighlight();

	/** True when the plantable highlight should currently show (gamepad, inside board, stopped). */
	bool ShouldHighlightPlantableNow() const;

	/** Clears the gamepad plant selection and its cursor glow. */
	void ClearGamepadSelection();

	/** Validates / auto-selects the gamepad selection (default = a neighbor when stopped, plantable preferred). */
	void UpdateGamepadSelection();

	/** Recomputes the whole gamepad board visual: selection, plantable glow, and exit-plane availability. */
	void RefreshGamepadBoardVisuals();

	/** Broadcasts exit-plane availability (via the controller delegate) for the player's current tile. */
	void UpdateExitHighlight();

	/** Broadcasts "unavailable" for the currently highlighted exit plane, if any, and forgets it. */
	void ClearExitHighlight();

	/** Returns a default neighbor of the player's tile (direction-agnostic): a plantable one if any exists,
	 *  otherwise any neighbor so the cursor always rests on a tile around the player. Nullptr if no neighbor. */
	AML_Tile* FindDefaultSelectableNeighbor() const;

	/** True if Tile is a neighbor of the player's current tile (plantable or not). */
	bool IsSelectableNeighborOfPlayer(const AML_Tile* Tile) const;

	UFUNCTION()
	void HandleBoardActivityStateChanged(bool bIsMoving);

	UFUNCTION()
	void HandleWavePropagationFinished();

	// Undo / reset animation: hide the board visuals while it plays, recompute once it finishes (covers
	// undoing an action that did not move the player, e.g. undoing the plant that killed the player).
	UFUNCTION()
	void HandleRollbackAnimating(bool bAnimating);

public:

	UPROPERTY(BlueprintAssignable, Category = "Hover Preview Component|Hover")
	FOnHoveredTileChanged OnHoveredTileChanged;

	void Initialize(AML_PlayerController* Controller, AML_PlayerCharacter* Character);
	void NotifyMovementModeChanged(EML_PlayerMovementMode NewMode);
	void NotifyPlayerTileChanged();

	void ClearPathHoverPreview();
	void StartHoverPreviewTimer();
	void StopHoverPreviewTimer();

	// Clears the currently active glow (cursor + path) immediately.
	// Used when a board's glow is toggled OFF while it is being hovered: the per-tick
	// same-tile short-circuit would otherwise leave a stale glow on the hovered tile.
	void ClearActiveGlow();
	
	void SetForcedHoverTile(AML_Tile* Tile);
	void ClearForcedHoverTile();

	// ===== Gamepad plant selection (called by the gamepad input handler) =====

	/** Sets the gamepad plant selection (right-stick aim), rendering its cursor glow + updating plantables. */
	void SetGamepadSelectedTile(AML_Tile* Tile);

	/** The tile the plant button should act on, or nullptr. */
	AML_Tile* GetGamepadSelectedTile() const { return GamepadSelectedTile; }

	void UpdateShowPreviews(const bool Value);
	void NotifyInputDeviceChanged(EML_InputDevice NewDevice);
};
