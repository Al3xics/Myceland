// Copyright Myceland Team, All Rights Reserved.

#include "Core/ML_TileTypeTraits.h"

bool UML_TileTypeTraits::IsWalkable(EML_TileType Type)
{
	return Type == EML_TileType::Dirt
		|| Type == EML_TileType::Grass
		|| Type == EML_TileType::WaterPath;
}

bool UML_TileTypeTraits::IsBlocking(EML_TileType Type)
{
	return Type == EML_TileType::Water
		|| Type == EML_TileType::Parasite
		|| Type == EML_TileType::Obstacle
		|| Type == EML_TileType::Tree;
}

bool UML_TileTypeTraits::CanParasitePropagateTo(EML_TileType Type)
{
	return Type == EML_TileType::Grass;
}

bool UML_TileTypeTraits::CanGrassPropagateTo(EML_TileType Type)
{
	return Type == EML_TileType::Dirt || Type == EML_TileType::Obstacle;
}

bool UML_TileTypeTraits::IsGrassConvertible(EML_TileType Type)
{
	return Type == EML_TileType::Dirt;
}

bool UML_TileTypeTraits::CanWaterPropagateTo(EML_TileType Type)
{
	return Type == EML_TileType::Parasite;
}

bool UML_TileTypeTraits::CanSpawnCollectible(EML_TileType Type)
{
	return Type == EML_TileType::Dirt || Type == EML_TileType::Grass;
}

bool UML_TileTypeTraits::CanPlayerPlant(EML_TileType Type)
{
	return Type == EML_TileType::Dirt;
}

bool UML_TileTypeTraits::IsWinPropagationConvertible(EML_TileType Type)
{
	return Type == EML_TileType::Dirt || Type == EML_TileType::Parasite;
}

bool UML_TileTypeTraits::IsLethalTile(EML_TileType Type)
{
	return Type == EML_TileType::Water || Type == EML_TileType::Parasite;
}

bool UML_TileTypeTraits::IsWinPathTile(EML_TileType Type)
{
	return Type == EML_TileType::Grass || Type == EML_TileType::Water;
}

TSet<EML_TileType> UML_TileTypeTraits::GetWinPathTypes()
{
	// Iterate every known tile type and keep those the trait accepts.
	// This ensures CachedGoalPathAllowedSet in WinLoseSubsystem never drifts from IsWinPathTile.
	static constexpr EML_TileType AllTypes[] = {
		EML_TileType::Dirt,
		EML_TileType::Grass,
		EML_TileType::Parasite,
		EML_TileType::Water,
		EML_TileType::WaterPath,
		EML_TileType::Obstacle,
		EML_TileType::Tree
	};

	TSet<EML_TileType> Result;
	Result.Reserve(UE_ARRAY_COUNT(AllTypes));
	for (const EML_TileType Type : AllTypes)
	{
		if (IsWinPathTile(Type))
			Result.Add(Type);
	}
	return Result;
}

bool UML_TileTypeTraits::IsWaterType(EML_TileType Type)
{
	return Type == EML_TileType::Water;
}

bool UML_TileTypeTraits::IsParasiteType(EML_TileType Type)
{
	return Type == EML_TileType::Parasite;
}

bool UML_TileTypeTraits::IsTreeType(EML_TileType Type)
{
	return Type == EML_TileType::Tree;
}
