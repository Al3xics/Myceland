// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tiles/ML_TileBase.h"
#include "ML_TileObstacle.generated.h"

class UML_WinLoseSubsystem;
class AML_Tile;
class AML_BoardSpawner;

UCLASS()
class MYCELAND_API AML_TileObstacle : public AML_TileBase
{
	GENERATED_BODY()

public:
	AML_TileObstacle();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintImplementableEvent, Category="Myceland Obstacle")
	void SwitchAlive();

private:
	UPROPERTY()
	TObjectPtr<UML_WinLoseSubsystem> WinLoseSubsystem = nullptr;

	UPROPERTY()
	TObjectPtr<AML_Tile> OwnerTile = nullptr;

	UFUNCTION()
	void HandleWin();

	AML_Tile* ResolveOwnerTile() const;
};