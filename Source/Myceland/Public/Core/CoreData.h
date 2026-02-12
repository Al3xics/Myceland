// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CoreData.generated.h"


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

USTRUCT(BlueprintType)
struct FML_GameResult
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	EML_WinLose Result;
	
	UPROPERTY(BlueprintReadOnly)
	bool bIsGameOver;
};