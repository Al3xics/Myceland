#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyceliumBoardSpawner.generated.h"

class AMyceliumCase;

UENUM(BlueprintType)
enum class EHexGridLayout : uint8
{
	HexagonRadius UMETA(DisplayName="Hexagon (Radius)"),
	RectangleWH   UMETA(DisplayName="Rectangle (Width/Height)")
};

UENUM(BlueprintType)
enum class EHexOffsetLayout : uint8
{
	OddR,   // PointyTop le plus fréquent
	EvenR,
	OddQ,   // FlatTop le plus fréquent
	EvenQ
};

UENUM(BlueprintType)
enum class EHexOrientation : uint8
{
	FlatTop,
	PointyTop
};

UCLASS()
class MYCELIUM_API AMyceliumBoardSpawner : public AActor
{
	GENERATED_BODY()

public:
	AMyceliumBoardSpawner();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

public:
	UPROPERTY(EditAnywhere, Category="Hex Grid")
	TSubclassOf<class AMyceliumCase> CaseClass;

	// Choix du mode de génération
	UPROPERTY(EditAnywhere, Category="Hex Grid")
	EHexGridLayout GridLayout = EHexGridLayout::HexagonRadius;

	// --- Mode Radius (Hexagon) ---
	UPROPERTY(EditAnywhere, Category="Hex Grid|Radius", meta=(ClampMin="0", EditCondition="GridLayout==EHexGridLayout::HexagonRadius", EditConditionHides))
	int32 Radius = 2;

	// --- Mode Rectangle (W/H) ---
	UPROPERTY(EditAnywhere, Category="Hex Grid|Rectangle", meta=(ClampMin="1", EditCondition="GridLayout==EHexGridLayout::RectangleWH", EditConditionHides))
	int32 GridWidth = 5;

	UPROPERTY(EditAnywhere, Category="Hex Grid|Rectangle", meta=(ClampMin="1", EditCondition="GridLayout==EHexGridLayout::RectangleWH", EditConditionHides))
	int32 GridHeight = 5;

	// Orientation & layout offset (utilisé surtout en Rectangle)
	UPROPERTY(EditAnywhere, Category="Hex Grid")
	EHexOrientation Orientation = EHexOrientation::FlatTop;

	UPROPERTY(EditAnywhere, Category="Hex Grid", meta=(EditCondition="GridLayout==EHexGridLayout::RectangleWH", EditConditionHides))
	EHexOffsetLayout OffsetLayout = EHexOffsetLayout::OddR;

	// Taille auto / manuelle
	UPROPERTY(EditAnywhere, Category="Hex Grid|Size")
	bool bAutoDetectHexSize = true;

	UPROPERTY(EditAnywhere, Category="Hex Grid|Size", meta=(ClampMin="1.0", EditCondition="!bAutoDetectHexSize", EditConditionHides))
	float HexSize = 100.f;

	UPROPERTY(EditAnywhere, Category="Hex Grid")
	FRotator TileRotation = FRotator::ZeroRotator;

	UPROPERTY(VisibleAnywhere, Category="Hex Grid")
	TMap<FIntPoint, TObjectPtr<AMyceliumCase>> TilesByAxial;


private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> SpawnedTiles;

	void ClearTiles();

	// Générateurs
	void SpawnHexagonRadius();
	void SpawnRectangleWH();

	// Conversions
	FVector AxialToWorld(int32 Q, int32 R) const;
	FIntPoint OffsetToAxial(int32 Col, int32 Row) const;

	// Auto-size
	float DetectSizeFromMesh() const;
};
