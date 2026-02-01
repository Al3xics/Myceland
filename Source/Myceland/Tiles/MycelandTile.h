#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "MycelandTile.generated.h"

UCLASS(Blueprintable)
class MYCELAND_API AMycelandTile : public AActor
{
	GENERATED_BODY()

public:
	AMycelandTile();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tile")
	UStaticMeshComponent* HighlightTileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tile")
	UStaticMeshComponent* HexagonCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tile")
	FIntPoint AxialCoord = FIntPoint(0, 0);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tile")
	bool bBlocked = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tile")
	TArray<class AMycelandTile*> Neighbors;

public:
	UFUNCTION(BlueprintCallable, Category="Tile")
	void SetBlocked(bool bNewBlocked);

	UFUNCTION(BlueprintPure, Category="Tile")
	bool IsBlocked() const { return bBlocked; }

	UFUNCTION(BlueprintCallable, Category="Tile")
	void SetAxialCoord(const FIntPoint& InAxial) { AxialCoord = InAxial; }

	UFUNCTION(BlueprintPure, Category="Tile")
	FIntPoint GetAxialCoord() const { return AxialCoord; }

	UFUNCTION(BlueprintCallable, Category="Tile")
	void SetNeighbors(const TArray<class AMycelandTile*>& InNeighbors) { Neighbors = InNeighbors; }

	UFUNCTION(BlueprintPure, Category="Tile")
	const TArray<class AMycelandTile*>& GetNeighbors() const { return Neighbors; }
};
