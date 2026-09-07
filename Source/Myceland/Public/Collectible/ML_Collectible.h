// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ML_Collectible.generated.h"

class AML_Tile;
class AML_PlayerCharacter;
class AML_PlayerController;
class USphereComponent;

UCLASS()
class MYCELAND_API AML_Collectible : public AActor
{
	GENERATED_BODY()
	
private:
	UPROPERTY()
	AML_Tile* OwningTile = nullptr;

	// The parasite tile that caused this collectible to spawn.
	UPROPERTY()
	AML_Tile* SourceParasite = nullptr;

	bool bSpawnSequenceStarted = false;
	bool bHiddenUntilSpawnSequence = false;
	FTimerHandle SpawnSequenceFallbackTimer;

	UFUNCTION()
	void HandleSourceParasiteReady(AML_Tile* Tile);

	void StopWaitingForSourceParasite();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Implemented in the collectible Blueprint: the flight from the source parasite to this tile.
	// Never called on the rollback spawn path, where the collectible is simply restored in place.
	UFUNCTION(BlueprintImplementableEvent, Category="Myceland Collectible")
	void StartSpawnAnimation();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Myceland Tile")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Myceland Tile")
	USphereComponent* Collision;

public:
	UPROPERTY()
	FIntPoint OwningAxial = FIntPoint::ZeroValue;

	AML_Collectible();

	UFUNCTION(BlueprintCallable, Category="Myceland Collectible", meta=(Tooltip="Will clear the bHasCollectible from the tile it was on, and then destroy this actor !"))
	void AddEnergy(AML_PlayerController* MycelandController, AML_PlayerCharacter* MycelandCharacter);
	
	// UFUNCTION(BlueprintPure, Category="Myceland Collectible")
	// bool CheckIsOwningTile(AML_PlayerCharacter* MycelandCharacter);
	
	UFUNCTION(BlueprintCallable, Category="Myceland Collectible")
	AML_Tile* GetOwningTile() const { return OwningTile; }
	
	UFUNCTION(BlueprintCallable, Category="Myceland Collectible")
	void SetOwningTile(AML_Tile* InOwningTile) { OwningTile = InOwningTile; }
	
	UFUNCTION(BlueprintCallable, Category="Myceland Collectible")
	AML_Tile* GetSourceParasite() const { return SourceParasite; }
	
	UFUNCTION(BlueprintCallable, Category="Myceland Collectible")
	void SetSourceParasite(AML_Tile* InSourceParasite) { SourceParasite = InSourceParasite; }
	
	UFUNCTION(BlueprintImplementableEvent, Category="Myceland Collectible")
	void BeforeDestroyCollectible(const AML_Tile* Tile);

	// Call between SpawnActorDeferred and FinishSpawning. A collectible spawning on the tile the player
	// already stands on used to overlap him during BeginPlay and be collected on its very first frame,
	// before it was ever drawn: it is hidden and non-collectible until its flight lands.
	void PrepareForSpawnSequence();

	// The collectible wave resolves faster than the grass -> parasite transformation it reacts to, so the
	// actor is spawned now but only shown once its own source parasite reports being done. Each collectible
	// waits on its own parasite, which is what makes the energies cascade with the propagation instead of
	// appearing in one batch. TimeoutSeconds is a safety net: if the parasite Blueprint never reports, the
	// collectible starts anyway rather than staying invisible forever.
	void WaitForSourceParasite(float TimeoutSeconds);

	// Shows the collectible and runs StartSpawnAnimation. Idempotent.
	UFUNCTION(BlueprintCallable, Category="Myceland Collectible")
	void BeginSpawnSequence();

	UFUNCTION(BlueprintPure, Category="Myceland Collectible")
	bool HasSpawnSequenceStarted() const { return bSpawnSequenceStarted; }

	void InitOwningAxial(const FIntPoint& InAxial) { OwningAxial = InAxial; }
	const FIntPoint& GetOwningAxial() const { return OwningAxial; }
	void DestroyCollectible();
};
