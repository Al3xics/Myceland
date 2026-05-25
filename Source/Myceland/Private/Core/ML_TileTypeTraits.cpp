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
