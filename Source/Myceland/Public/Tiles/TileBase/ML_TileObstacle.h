#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ML_TileObstacle.generated.h"

class UML_WinLoseSubsystem;
class AML_Tile;

UCLASS()
class MYCELAND_API AML_TileObstacle : public AActor
{
	GENERATED_BODY()

public:
	AML_TileObstacle();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<AML_Tile> OwnerTile;

	UFUNCTION()
	void HandleWin();

	AML_Tile* ResolveOwnerTile() const;

public:
	// THIS is what you use in BP
	UFUNCTION(BlueprintImplementableEvent, Category="Myceland")
	void SwitchAlive();
};