// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ML_NavigationBridgeComponent.generated.h"

class AML_BoardSpawner;
class AML_PlayerCharacter;
class AML_PlayerController;
class AML_Tile;
class UNavigationPath;

UCLASS(ClassGroup=(Myceland), meta=(BlueprintSpawnableComponent))
class MYCELAND_API UML_NavigationBridgeComponent : public UActorComponent
{
	GENERATED_BODY()

private:
	UPROPERTY(Transient)
	AML_PlayerController* OwningController = nullptr;

	UPROPERTY(Transient)
	AML_PlayerCharacter* PlayerCharacter = nullptr;

	UPROPERTY(Transient)
	FVector PendingFreeMovementTarget = FVector::ZeroVector;

	bool bHasFreeMovementTarget = false;
	bool bIsUsingNavMeshMovement = false;
	float NavMeshAcceptanceRadius = 50.f;

	bool IsCompleteNavMeshPath(const UNavigationPath* Path, const FVector& Destination) const;

public:
	void Initialize(AML_PlayerController* Controller, AML_PlayerCharacter* Character, float InNavMeshAcceptanceRadius);

	void StartNavMeshMovement(const FVector& WorldLocation);
	void StopNavMeshMovement();
	bool TickNavMeshMovement(float DeltaTime);

	bool IsUsingNavMeshMovement() const { return bIsUsingNavMeshMovement; }
	AML_Tile* PredictNavMeshEntryTile(const AML_BoardSpawner* Board, const FVector& Destination) const;
	AML_Tile* FindReachableExitBorderTile(const AML_BoardSpawner* Board, const FVector& OutsideDestination) const;
};
