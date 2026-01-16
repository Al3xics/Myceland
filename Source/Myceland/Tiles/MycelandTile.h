#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SphereComponent.h"
#include "MycelandTile.generated.h"

UCLASS(Blueprintable)
class MYCELAND_API AMycelandTile : public AActor
{
	GENERATED_BODY()

public:
	AMycelandTile();

protected:
	// Appelé quand l’acteur est construit ou modifié dans l’éditeur
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Case")
	UStaticMeshComponent* CaseMesh;

	// Collision "mur invisible" qui bloque le Pawn à la frontière
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tile|Block")
	USphereComponent* Blocker;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tile|Block")
	bool bBlocked = false;

	// Recalcule le rayon du blocker en fonction du mesh
	UFUNCTION(BlueprintCallable, Category="Tile")
	void SetBlockRadius();

public:
	UFUNCTION(BlueprintCallable, Category="Tile")
	void SetBlocked(bool bNewBlocked);

	UFUNCTION(BlueprintPure, Category="Tile")
	bool IsBlocked() const { return bBlocked; }
};
