// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ML_BiomeTileSet.generated.h"

class AML_TileBase;
enum class EML_TileType : uint8;
class AML_TileWater;
class AML_Collectible;
class AML_TileParasite;
class AML_TileGrass;
class AML_TileDirt;

UCLASS()
class MYCELAND_API UML_BiomeTileSet : public UDataAsset
{
	GENERATED_BODY()
	
private:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AML_TileDirt> DirtClass;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AML_TileGrass> GrassClass;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AML_TileParasite> ParasiteClass;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AML_Collectible> CollectibleClass;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AML_TileWater> WaterClass;
	
public:
	TSubclassOf<AML_TileBase> GetClassFromTileType(EML_TileType Type) const;
	TSubclassOf<AML_Collectible> GetCollectibleClass() const { return CollectibleClass; }
};
