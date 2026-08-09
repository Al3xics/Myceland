// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tiles/ML_TileBase.h"
#include "ML_TileGrass.generated.h"

class AML_Tile;
class UML_WinLoseSubsystem;

UCLASS()
class MYCELAND_API AML_TileGrass : public AML_TileBase
{
	GENERATED_BODY()

public:
	AML_TileGrass();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Called when the board containing this grass is won.
	 * Implement this event inside the Grass Blueprint.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Grass")
	void SwitchAlive();

private:
	AML_Tile* ResolveOwnerTile() const;

	UFUNCTION()
	void HandleWin();

	UPROPERTY()
	TObjectPtr<AML_Tile> OwnerTile;

	UPROPERTY()
	TObjectPtr<UML_WinLoseSubsystem> WinLoseSubsystem;
};