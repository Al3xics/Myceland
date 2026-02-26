// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ML_Collectible.generated.h"

class AML_PlayerController;
class AML_PlayerCharacter;
class USphereComponent;

UCLASS()
class MYCELAND_API AML_Collectible : public AActor
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Myceland Tile")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Myceland Tile")
	USphereComponent* Collision;

public:
	AML_Collectible();
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category="Myceland Collectible")
	void AddEnergy(AML_PlayerController* MycelandController, AML_PlayerCharacter* MycelandCharacter);

	UPROPERTY() FIntPoint OwningAxial = FIntPoint::ZeroValue;

	void InitOwningAxial(const FIntPoint& InAxial) { OwningAxial = InAxial; }
	const FIntPoint& GetOwningAxial() const { return OwningAxial; }
};