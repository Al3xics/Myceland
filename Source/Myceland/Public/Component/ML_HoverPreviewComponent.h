// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/ML_CoreData.h"
#include "ML_HoverPreviewComponent.generated.h"

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

	void UpdateHoverPreview();
	void TickHoverPreview();
	void TickCursorHoverPreview();
	void ClearCursorHoverPreview();
	TArray<AML_Tile*> BuildPreviewPath(const AML_Tile* TargetTile) const;

public:

	UPROPERTY(BlueprintAssignable, Category = "Hover Preview Component|Hover")
	FOnHoveredTileChanged OnHoveredTileChanged;

	void Initialize(AML_PlayerController* Controller, AML_PlayerCharacter* Character);
	void NotifyMovementModeChanged(EML_PlayerMovementMode NewMode);

	void ClearHoverPreview();
	void StartHoverPreviewTimer();
	void StopHoverPreviewTimer();
};
