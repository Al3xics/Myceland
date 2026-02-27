// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ML_Collectible.generated.h"

class AML_Tile;
class AML_PlayerCharacter;
class AML_PlayerController;
class AML_PlayerCharacter;
class USphereComponent;

UCLASS()
class MYCELAND_API AML_Collectible : public AActor
{
	GENERATED_BODY()
	
private:
	UPROPERTY()
	AML_Tile* OwningTile = nullptr;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Myceland Tile")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Myceland Tile")
	USphereComponent* Collision;

public:
	UPROPERTY()
	FIntPoint OwningAxial = FIntPoint::ZeroValue;

	AML_Collectible();
	virtual void Tick(float DeltaTime) override;
	
	UFUNCTION(BlueprintCallable, Category="Myceland Collectible", meta=(Tooltip="Will clear the bHasCollectible from the tile it was on, and then destroy this actor !"))
	void AddEnergy(AML_PlayerController* MycelandController, AML_PlayerCharacter* MycelandCharacter);
	
	UFUNCTION(BlueprintPure, Category="Myceland Collectible")
	bool CheckIsOwningTile(AML_PlayerCharacter* MycelandCharacter);
	
	UFUNCTION(BlueprintCallable, Category="Myceland Collectible")
	AML_Tile* GetOwningTile() const { return OwningTile; }
	
	UFUNCTION(BlueprintCallable, Category="Myceland Collectible")
	void SetOwningTile(AML_Tile* InOwningTile) { OwningTile = InOwningTile; }

	void InitOwningAxial(const FIntPoint& InAxial) { OwningAxial = InAxial; }
	const FIntPoint& GetOwningAxial() const { return OwningAxial; }
};
