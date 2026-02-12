// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Core/CoreData.h"
#include "ML_Tile.generated.h"

enum class EML_TileType : uint8;

UCLASS(Blueprintable)
class MYCELAND_API AML_Tile : public AActor
{
	GENERATED_BODY()

public:
	AML_Tile();

private:
	UPROPERTY(VisibleAnywhere, Category="Myceland Tile")
	FIntPoint AxialCoord = FIntPoint(0, 0);

	UPROPERTY(VisibleAnywhere, Category="Myceland Tile")
	EML_TileType CurrentType = EML_TileType::Dirt;

	UPROPERTY(VisibleAnywhere, Category="Myceland Tile")
	bool bBlocked = false;
	
	// todo
	// delete those methods because they should be in AML_BoardSpawner
	// I'm leaving them here for now because they are used in the blueprint for now
	
	UPROPERTY(VisibleAnywhere, Category="Myceland Tile")
	TArray<AML_Tile*> Neighbors;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Myceland Tile")
	USceneComponent* SceneRoot;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Myceland Tile")
	UChildActorComponent* TileChildActor;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Myceland Tile")
	UStaticMeshComponent* HighlightTileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Myceland Tile")
	UStaticMeshComponent* HexagonCollision;

public:
	UFUNCTION(BlueprintCallable, Category="Myceland Tile")
	void Init(const FIntPoint AxialCoordinates, const EML_TileType InitialType) { AxialCoord = AxialCoordinates; CurrentType = InitialType; }

	UFUNCTION(BlueprintCallable, Category="Myceland Tile")
	void SetAxialCoord(const FIntPoint& InAxial) { AxialCoord = InAxial; }

	UFUNCTION(BlueprintPure, Category="Myceland Tile")
	FIntPoint GetAxialCoord() const { return AxialCoord; }

	UFUNCTION(BlueprintCallable, Category="Myceland Tile")
	void SetCurrentType(const EML_TileType NewType) { CurrentType = NewType; }

	UFUNCTION(BlueprintPure, Category="Myceland Tile")
	EML_TileType GetCurrentType() const { return CurrentType; }
	
	UFUNCTION(BlueprintCallable, Category="Myceland Tile")
	void SetBlocked(bool bNewBlocked);

	UFUNCTION(BlueprintPure, Category="Myceland Tile")
	bool IsBlocked() const { return bBlocked; }
	
	
	
	// todo
	// delete those methods because they should be in AML_BoardSpawner
	// I'm leaving them here for now because they are used in the blueprint for now
	
	UFUNCTION(BlueprintCallable, Category="Myceland Tile")
	void SetNeighbors(const TArray<class AML_Tile*>& InNeighbors) { Neighbors = InNeighbors; }

	UFUNCTION(BlueprintPure, Category="Myceland Tile")
	const TArray<AML_Tile*>& GetNeighbors() const { return Neighbors; }
};
