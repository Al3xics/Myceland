#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyceliumCase.generated.h"

UCLASS(Blueprintable)
class MYCELIUM_API AMyceliumCase : public AActor
{
	GENERATED_BODY()

public:
	AMyceliumCase();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Case")
	UStaticMeshComponent* CaseMesh;
};
