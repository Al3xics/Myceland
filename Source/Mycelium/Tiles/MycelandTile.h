#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MycelandTile.generated.h"

UCLASS(Blueprintable)
class MYCELIUM_API AMycelandTile : public AActor
{
	GENERATED_BODY()

public:
	AMycelandTile();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Case")
	UStaticMeshComponent* CaseMesh;
};
