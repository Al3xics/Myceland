// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tiles/ML_TileBase.h"
#include "ML_TileParasite.generated.h"

class USceneComponent;

UCLASS()
class MYCELAND_API AML_TileParasite : public AML_TileBase
{
	GENERATED_BODY()

public:
	AML_TileParasite();

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
