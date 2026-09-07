// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tiles/ML_TileBase.h"
#include "ML_TileParasite.generated.h"

class USceneComponent;
class AML_Tile;

UCLASS()
class MYCELAND_API AML_TileParasite : public AML_TileBase
{
	GENERATED_BODY()

public:
	AML_TileParasite();
	
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Parasite|Propagation")
	void Propagate(int32 NeighborIndex, AML_Tile* TargetTile);

	// Call this from the parasite Blueprint when its spawn/transformation animation is visually over.
	// It is what releases the collectible that this parasite caused to spawn, so the energy never flies
	// out of a tile that is still turning. Idempotent: wiring it on several timelines is safe.
	UFUNCTION(BlueprintCallable, Category = "Parasite|Propagation")
	void NotifySpawnAnimationFinished();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parasite|Roots")
	TArray<USceneComponent*> Extremities;

	UFUNCTION(BlueprintPure, Category = "Parasite|Roots")
	const TArray<USceneComponent*>& GetExtremities() const { return Extremities; }

private:
	void NotifyRootNetworksRegistered();
	void NotifyRootNetworksUnregistered();
};
