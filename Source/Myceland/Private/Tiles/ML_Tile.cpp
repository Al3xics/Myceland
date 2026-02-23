// Copyright Myceland Team, All Rights Reserved.


#include "Tiles/ML_Tile.h"

#include "Tiles/ML_TileBase.h"
#include "Tiles/TileBase/ML_TileDirt.h"
#include "Tiles/TileBase/ML_TileGrass.h"
#include "Tiles/TileBase/ML_TileObstacle.h"
#include "Tiles/TileBase/ML_TileParasite.h"
#include "Tiles/TileBase/ML_TileTree.h"
#include "Tiles/TileBase/ML_TileWater.h"


AML_Tile::AML_Tile()
{
	PrimaryActorTick.bCanEverTick = false;
	
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
	
	TileChildActor = CreateDefaultSubobject<UChildActorComponent>(TEXT("TileChildActor"));
	TileChildActor->SetupAttachment(RootComponent);

	HighlightTileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TileMesh"));
	HighlightTileMesh->SetupAttachment(RootComponent);
	HighlightTileMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	HighlightTileMesh->SetCollisionObjectType(ECC_WorldStatic);
	HighlightTileMesh->SetCollisionResponseToAllChannels(ECR_Block);
	HighlightTileMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	HighlightTileMesh->SetGenerateOverlapEvents(false);
	
	HexagonCollision = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HexagonCollision"));
	HexagonCollision->SetupAttachment(RootComponent);
	HexagonCollision->SetRelativeScale3D(FVector(0.9f, 0.9f, 0.9f));
	HexagonCollision->SetCollisionObjectType(ECC_WorldStatic);
	HexagonCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	HexagonCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	HexagonCollision->SetGenerateOverlapEvents(false);
	HexagonCollision->SetHiddenInGame(true);
	HexagonCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AML_Tile::SetBlocked(bool bNewBlocked)
{
	bBlocked = bNewBlocked;

	if (!HexagonCollision) return;

	HexagonCollision->SetCollisionEnabled(
		bBlocked
		? ECollisionEnabled::QueryOnly
		: ECollisionEnabled::NoCollision
	);
}

bool AML_Tile::IsTileTypeBlocking(const EML_TileType Type)
{
	switch (Type)
	{
		case EML_TileType::Water:
		case EML_TileType::Parasite:
		case EML_TileType::Obstacle:
		case EML_TileType::Tree:
			return true;

		case EML_TileType::Grass:
		case EML_TileType::Dirt:
		default:
			return false;
	}
}

#if WITH_EDITOR
void AML_Tile::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!PropertyChangedEvent.Property) return;

	const FName PropertyName = PropertyChangedEvent.Property->GetFName();

	const bool bTypeChanged =
		PropertyName == GET_MEMBER_NAME_CHECKED(AML_Tile, CurrentType);

	const bool bClassChanged =
		PropertyName == GET_MEMBER_NAME_CHECKED(AML_Tile, DirtClass) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AML_Tile, GrassClass) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AML_Tile, ParasiteClass) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AML_Tile, WaterClass) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AML_Tile, ObstacleClass) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AML_Tile, TreeClass);

	if (bTypeChanged || bClassChanged)
	{
		UpdateClassInEditor(CurrentType);
	}
}

void AML_Tile::UpdateClassInEditor(const EML_TileType NewTileType)
{
	CurrentType = NewTileType;
	TSubclassOf<AML_TileBase> TileBase;
	
	switch (CurrentType)
	{
		case EML_TileType::Dirt:		TileBase = DirtClass; break;
		case EML_TileType::Grass:		TileBase = GrassClass; break;
		case EML_TileType::Parasite:	TileBase = ParasiteClass; break;
		case EML_TileType::Water:		TileBase = WaterClass; break;
		case EML_TileType::Obstacle:	TileBase = ObstacleClass; break;
		case EML_TileType::Tree:		TileBase = TreeClass; break;
	}
	
	TileChildActor->SetChildActorClass(TileBase);
	SetBlocked(IsTileTypeBlocking(NewTileType));
}
#endif

void AML_Tile::UpdateClassAtRuntime(const EML_TileType NewTileType, const TSubclassOf<AML_TileBase> NewClass)
{
	if (!NewClass) return;
	
	const EML_TileType OldType = CurrentType;
	if (OldType == NewTileType) return;
	
	CurrentType = NewTileType;
	
	// If there is a change from grass to parasite
	bConsumedGrass = (OldType == EML_TileType::Grass && NewTileType == EML_TileType::Parasite);
	
	TileChildActor->SetChildActorClass(NewClass);
	SetBlocked(IsTileTypeBlocking(NewTileType));
	OnTileTypeChanged(OldType, NewTileType);
}

void AML_Tile::Initialize(UML_BiomeTileSet* InBiomeTileSet)
{
#if WITH_EDITOR
	UpdateClassInEditor(CurrentType);
#else
	TSubclassOf<AML_TileBase> RuntimeClass = nullptr;
	if (InBiomeTileSet)
		RuntimeClass = InBiomeTileSet->GetClassFromTileType(CurrentType);

	if (RuntimeClass)
		UpdateClassAtRuntime(CurrentType, RuntimeClass);
#endif
}
