#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TileBase.generated.h"

UCLASS()
class MYCELAND_API ATileBase : public AActor
{
	GENERATED_BODY()

public:
	ATileBase();

protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tile", meta=(AllowPrivateAccess = "true"))
	USceneComponent* SceneRoot;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tile", meta=(AllowPrivateAccess = "true"))
	UStaticMeshComponent* GroundBase;

public:
	virtual void Tick(float DeltaTime) override;
};
