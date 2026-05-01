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

	UPROPERTY(Transient)
	AML_Tile* LastHoveredTile = nullptr;

	UPROPERTY(Transient)
	TArray<AML_Tile*> CurrentPreviewPath;

	bool bCurrentHoveredTileReachable = false;

	EML_PlayerMovementMode CurrentMovementMode = EML_PlayerMovementMode::FreeMovement;

	FTimerHandle HoverPreviewTimerHandle;
	
	// Overrides cursor-based tile detection. When set, the hover preview (path glow + OnHoveredTileChanged)
	// is computed against this tile instead of whatever is under the cursor.
	// Used during board exit: the exit tile is forced so the path highlights even if the cursor is elsewhere.
	// Cleared when the exit is confirmed, canceled, or the player leaves board mode.
	UPROPERTY(Transient)
	AML_Tile* ForcedHoverTile = nullptr;

	void UpdateHoverPreview();
	void TickHoverPreview();
	void TickCursorHoverPreview();
	void ClearCursorHoverPreview();
	void SetHoveredTileState(AML_Tile* HoveredTile, bool bIsReachable);
	AML_Tile* FindClosestEntryExitTile(const AML_BoardSpawner* Board, const FVector& PlayerLocation) const;
	TArray<AML_Tile*> BuildPreviewPathFromTile(const AML_Tile* StartTile, const AML_Tile* TargetTile) const;

public:

	UPROPERTY(BlueprintAssignable, Category = "Hover Preview Component|Hover")
	FOnHoveredTileChanged OnHoveredTileChanged;

	void Initialize(AML_PlayerController* Controller, AML_PlayerCharacter* Character);
	void NotifyMovementModeChanged(EML_PlayerMovementMode NewMode);
	void NotifyPlayerTileChanged();

	void ClearHoverPreview();
	void StartHoverPreviewTimer();
	void StopHoverPreviewTimer();
	
	void SetForcedHoverTile(AML_Tile* Tile);
	void ClearForcedHoverTile();
};
