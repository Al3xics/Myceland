// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CoreData.generated.h"

// ==================== ENUM ====================

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

USTRUCT()
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