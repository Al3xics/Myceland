// Copyright Myceland Team, All Rights Reserved.


#include "Data Asset/ML_BiomeTileSet.h"

#include "Tiles/ML_Tile.h"
#include "Tiles/TileBase/ML_TileDirt.h"
#include "Tiles/TileBase/ML_TileGrass.h"
#include "Tiles/TileBase/ML_TileParasite.h"
#include "Tiles/TileBase/ML_TileWater.h"

TSubclassOf<AML_TileBase> UML_BiomeTileSet::GetClassFromTileType(EML_TileType Type) const
{
	switch(Type)
	{
		case EML_TileType::Grass: return GrassClass;
		case EML_TileType::Parasite: return ParasiteClass;
		case EML_TileType::Water: return WaterClass;
		default: return DirtClass;
	}
}
