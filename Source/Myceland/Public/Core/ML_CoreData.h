// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ML_CoreData.generated.h"

// ==================== STATIC ====================

// static const FIntPoint Directions[6];

static const FIntPoint Directions[6] = {
	FIntPoint(1, 0),
	FIntPoint(1, -1),
	FIntPoint(0, -1),
	FIntPoint(-1, 0),
	FIntPoint(-1, 1),
	FIntPoint(0, 1)
};


// ==================== ENUM ====================

class UInputMappingContext;
class AML_Collectible;
class UML_PropagationWaves;
class AML_Tile;

UENUM(BlueprintType)
enum class EML_HexGridLayout : uint8
{
	HexagonRadius UMETA(DisplayName="Hexagon (Radius)"),
	RectangleWH   UMETA(DisplayName="Rectangle (Width/Height)")
};

UENUM(BlueprintType)
enum class EML_HexOffsetLayout : uint8
{
	OddR,   // PointyTop le plus fréquent
	EvenR,
	OddQ,   // FlatTop le plus fréquent
	EvenQ
};

UENUM(BlueprintType)
enum class EML_HexOrientation : uint8
{
	FlatTop,
	PointyTop
};

UENUM(BlueprintType)
enum class EML_TileType : uint8
{
	Dirt,
	Grass,
	Parasite,
	Water,
	Obstacle,
	Tree
};

UENUM(BlueprintType)
enum class EML_WinLose : uint8
{
	None,
	Win,
	Lose
};

UENUM(BlueprintType)
enum class EML_WidgetInputMode : uint8
{
	GameOnly,
	UIOnly,
	GameAndUI
};


// ==================== STRUCT ====================

USTRUCT(BlueprintType)
struct FML_GameResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	EML_WinLose Result = EML_WinLose::None;

	UPROPERTY(BlueprintReadOnly)
	bool bIsGameOver = false;
};

USTRUCT(BlueprintType)
struct FML_WidgetRegistryKey
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag RootTag;

	UPROPERTY()
	FGameplayTag WidgetTag;

	bool operator==(const FML_WidgetRegistryKey& Other) const
	{
		return RootTag == Other.RootTag && WidgetTag == Other.WidgetTag;
	}

	friend uint32 GetTypeHash(const FML_WidgetRegistryKey& Key)
	{
		return HashCombine(GetTypeHash(Key.RootTag), GetTypeHash(Key.WidgetTag));
	}
};

USTRUCT(BlueprintType)
struct FML_WaveChange
{
	GENERATED_BODY()
	
	UPROPERTY()
	AML_Tile* Tile = nullptr; // Tile
	
	UPROPERTY()
	EML_TileType TargetType = EML_TileType::Dirt; // Tile
	
	UPROPERTY()
	FVector SpawnLocation = FVector::ZeroVector; // Collectible
	
	UPROPERTY()
	TSubclassOf<AML_Collectible> CollectibleClass = nullptr; // Collectible
	
	UPROPERTY()
	int32 DistanceFromOrigin = 0;
	
	FML_WaveChange() = default;
	FML_WaveChange(AML_Tile* InTile, const EML_TileType InType, const int32 InDistance) : Tile(InTile), TargetType(InType), DistanceFromOrigin(InDistance) {}
	FML_WaveChange(const FVector& InLocation, const TSubclassOf<AML_Collectible> Class, const int32 InDistance) : SpawnLocation(InLocation), CollectibleClass(Class), DistanceFromOrigin(InDistance)  {}
};

USTRUCT(BlueprintType)
struct FML_InputMappingEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, config)
	TSoftObjectPtr<UInputMappingContext> Mapping;

	UPROPERTY(EditAnywhere, config)
	int32 Priority = 0;
};

USTRUCT(BlueprintType)
struct FML_TileGroup
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TArray<AML_Tile*> Tiles;
	
	UPROPERTY(BlueprintReadOnly)
	TArray<AML_Tile*> Goals;
};
