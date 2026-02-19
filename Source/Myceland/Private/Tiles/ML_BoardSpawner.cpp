// Copyright Myceland Team, All Rights Reserved.


#include "Tiles/ML_BoardSpawner.h"
#include "Tiles/ML_Tile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Tiles/TileBase/ML_TileGrass.h"
#include "Tiles/TileBase/ML_TileParasite.h"
#include "Tiles/TileBase/ML_TileWater.h"


AML_BoardSpawner::AML_BoardSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AML_BoardSpawner::Destroyed()
{
	ClearTiles();
	Super::Destroyed();
}

void AML_BoardSpawner::BeginPlay()
{
	Super::BeginPlay();
	
	ensureMsgf(BiomeTileSet, TEXT("BiomeTileSet is not set for board : %s"), *GetName());
	if (!BiomeTileSet) return;
	
	UpdateCurrentGrid();
}

void AML_BoardSpawner::RebuildGrid()
{
	ClearTiles();

	switch (GridLayout)
	{
	case EML_HexGridLayout::HexagonRadius: SpawnHexagonRadius(); break;
	case EML_HexGridLayout::RectangleWH:   SpawnRectangleWH();   break;
	}
}

void AML_BoardSpawner::UpdateCurrentGrid()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Rebuild map from existing tiles owned by this spawner.
	GridMap.Empty();
	SpawnedTiles.Empty();

	for (TActorIterator<AML_Tile> It(World); It; ++It)
	{
		AML_Tile* Tile = *It;
		if (!IsValid(Tile)) continue;
		if (Tile->GetOwner() != this) continue;

		FIntPoint Axial = Tile->GetAxialCoord();
		if (Axial == FIntPoint(0, 0))
		{
			const FIntPoint Derived = WorldToAxial(Tile->GetActorLocation());
			if (Derived != Axial)
			{
				Axial = Derived;
				Tile->SetAxialCoord(Axial);
			}
		}

		if (GridMap.Contains(Axial))
		{
			const FIntPoint Derived = WorldToAxial(Tile->GetActorLocation());
			if (!GridMap.Contains(Derived))
			{
				Axial = Derived;
				Tile->SetAxialCoord(Axial);
			}
		}

		if (GridMap.Contains(Axial))
		{
			Tile->Destroy();
			continue;
		}

		GridMap.Add(Axial, Tile);
		SpawnedTiles.Add(Tile);
	}

	// Determine all desired axial coordinates
	TSet<FIntPoint> DesiredAxials;
	DesiredAxials.Reserve(GridLayout == EML_HexGridLayout::HexagonRadius
		? (1 + 3 * Radius * (Radius + 1))
		: (GridWidth * GridHeight));

	switch (GridLayout)
	{
	case EML_HexGridLayout::HexagonRadius:
		{
			for (int32 Q = -Radius; Q <= Radius; ++Q)
			{
				const int32 RMin = FMath::Max(-Radius, -Q - Radius);
				const int32 RMax = FMath::Min(Radius, -Q + Radius);
				for (int32 R = RMin; R <= RMax; ++R)
				{
					DesiredAxials.Add(FIntPoint(Q, R));
				}
			}
			break;
		}
	case EML_HexGridLayout::RectangleWH:
		{
			for (int32 Row = 0; Row < GridHeight; ++Row)
			{
				for (int32 Col = 0; Col < GridWidth; ++Col)
				{
					DesiredAxials.Add(OffsetToAxial(Col, Row));
				}
			}
			break;
		}
	}

	// Remove tiles that are no longer part of the desired grid
	for (const TPair<FIntPoint, TObjectPtr<AML_Tile>>& Pair : GridMap)
	{
		if (DesiredAxials.Contains(Pair.Key)) continue;
		if (Pair.Value) Pair.Value->Destroy();
	}

	// Spawn new tiles or reattach existing
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	TMap<FIntPoint, TObjectPtr<AML_Tile>> NewTilesByAxial;
	NewTilesByAxial.Reserve(DesiredAxials.Num());

	for (const FIntPoint& Axial : DesiredAxials)
	{
		AML_Tile* Tile = nullptr;
		
		// Tile exists
		if (const TObjectPtr<AML_Tile>* Found = GridMap.Find(Axial))
		{
			Tile = Found->Get();
			if (!Tile) continue;

			// Reattach to the board spawner without changing scale/rotation
			Tile->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			Tile->SetAxialCoord(Axial);

			Tile->Initialize(BiomeTileSet);
		}
		// New tile
		else if (TileClass)
		{
			const FVector Location = AxialToWorld(Axial.X, Axial.Y);
			const FTransform SpawnTransform(FRotator::ZeroRotator, Location, TileScale);
			Tile = World->SpawnActor<AML_Tile>(TileClass, SpawnTransform, Params);
			if (!Tile) continue;

			Tile->AttachToActor(this, FAttachmentTransformRules::KeepRelativeTransform);
			Tile->SetAxialCoord(Axial);
			
			Tile->Initialize(BiomeTileSet);
		}

		if (Tile) NewTilesByAxial.Add(Axial, Tile);
	}

	GridMap = MoveTemp(NewTilesByAxial);
	SpawnedTiles.Empty();
	SpawnedTiles.Reserve(GridMap.Num());
	for (const TPair<FIntPoint, TObjectPtr<AML_Tile>>& Pair : GridMap)
	{
		SpawnedTiles.Add(Pair.Value);
	}
}

void AML_BoardSpawner::ClearTiles()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Détruit toute les tiles ou owner=this
	for (TActorIterator<AML_Tile> It(World); It; ++It)
	{
		AML_Tile* Tile = *It;
		if (!IsValid(Tile)) continue;

		if (Tile->GetOwner() == this)
		{
			Tile->Destroy();
		}
	}

	SpawnedTiles.Empty();
	GridMap.Empty();
}

TArray<AML_Tile*> AML_BoardSpawner::GetNeighbors(AML_Tile* CenterTile)
{
	if (!CenterTile) return TArray<AML_Tile*>();

	const FIntPoint CenterAxial = CenterTile->GetAxialCoord();
	TArray<AML_Tile*> Neighbors;
	Neighbors.Reserve(6);

	for (const FIntPoint& Dir : Directions)
	{
		const FIntPoint NeighborKey = CenterAxial + Dir;
		if (const TObjectPtr<AML_Tile>* Found = GridMap.Find(NeighborKey))
		{
			Neighbors.Add(Found->Get());
		}
		else
		{
			Neighbors.Add(nullptr);
		}
	}

	return Neighbors;
}

TMap<FIntPoint, AML_Tile*> AML_BoardSpawner::GetGridMap() const
{
	TMap<FIntPoint, AML_Tile*> Result;
	Result.Reserve(GridMap.Num());
	for (const TPair<FIntPoint, TObjectPtr<AML_Tile>>& Pair : GridMap)
	{
		Result.Add(Pair.Key, Pair.Value.Get());
	}
	return Result;
}

TArray<AML_Tile*> AML_BoardSpawner::GetGridTiles()
{
	TArray<AML_Tile*> Result;
	Result.Reserve(GridMap.Num());
	for (const TPair<FIntPoint, TObjectPtr<AML_Tile>>& Pair : GridMap)
	{
		Result.Add(Pair.Value.Get());
	}
	return Result;
}

FVector AML_BoardSpawner::AxialToWorld(int32 Q, int32 R) const
{
	// Axial -> 2D (x,y), puis -> UE (X,Y)
	const float Sqrt3 = 1.73205080757f;

	float X2D = 0.f;
	float Y2D = 0.f;

	if (Orientation == EML_HexOrientation::FlatTop)
	{
		// x = size * (3/2 q)
		// y = size * (sqrt(3) * (r + q/2))
		X2D = TileSize * (1.5f * Q);
		Y2D = TileSize * (Sqrt3 * (R + 0.5f * Q));
	}
	else // PointyTop
	{
		// x = size * (sqrt(3) * (q + r/2))
		// y = size * (3/2 r)
		X2D = TileSize * (Sqrt3 * (Q + 0.5f * R));
		Y2D = TileSize * (1.5f * R);
	}

	// Dans UE: X,Y sur le plan, Z=0
	return GetActorLocation() + FVector(X2D, Y2D, 0.f);
}

FIntPoint AML_BoardSpawner::WorldToAxial(const FVector& WorldLocation) const
{
	const float Sqrt3 = 1.73205080757f;
	const FVector Local = WorldLocation - GetActorLocation();

	float Qf = 0.f;
	float Rf = 0.f;

	if (Orientation == EML_HexOrientation::FlatTop)
	{
		Qf = (2.f / 3.f * Local.X) / TileSize;
		Rf = (-1.f / 3.f * Local.X + (Sqrt3 / 3.f) * Local.Y) / TileSize;
	}
	else
	{
		Qf = ((Sqrt3 / 3.f) * Local.X - (1.f / 3.f) * Local.Y) / TileSize;
		Rf = (2.f / 3.f * Local.Y) / TileSize;
	}

	const float Xf = Qf;
	const float Zf = Rf;
	const float Yf = -Xf - Zf;

	int32 Rx = FMath::RoundToInt(Xf);
	int32 Ry = FMath::RoundToInt(Yf);
	int32 Rz = FMath::RoundToInt(Zf);

	const float XDiff = FMath::Abs(Rx - Xf);
	const float YDiff = FMath::Abs(Ry - Yf);
	const float ZDiff = FMath::Abs(Rz - Zf);

	if (XDiff > YDiff && XDiff > ZDiff)
	{
		Rx = -Ry - Rz;
	}
	else if (YDiff > ZDiff)
	{
		Ry = -Rx - Rz;
	}
	else
	{
		Rz = -Rx - Ry;
	}

	return FIntPoint(Rx, Rz);
}

void AML_BoardSpawner::SpawnHexagonRadius()
{
	if (!TileClass) return;
	UWorld* World = GetWorld();
	if (!World) return;

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Génère un "hexagon" de radius N en axial:
	// q ∈ [-N, N]
	// r ∈ [max(-N, -q-N), min(N, -q+N)]
	for (int32 Q = -Radius; Q <= Radius; ++Q)
	{
		const int32 RMin = FMath::Max(-Radius, -Q - Radius);
		const int32 RMax = FMath::Min( Radius, -Q + Radius);

		for (int32 R = RMin; R <= RMax; ++R)
		{
			const FVector Location = AxialToWorld(Q, R);
			const FTransform TileTransform(TileRotation, Location, TileScale);

			AML_Tile* Tile = World->SpawnActor<AML_Tile>(TileClass, TileTransform, Params);
			if (!Tile) continue;
			Tile->Initialize(BiomeTileSet);

			SpawnedTiles.Add(Tile);
			Tile->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			const FIntPoint Axial(Q, R);
			Tile->SetAxialCoord(Axial);
			GridMap.Add(Axial, Tile);
		}
	}
}

FIntPoint AML_BoardSpawner::OffsetToAxial(int32 Col, int32 Row) const
{
	// Retourne (q,r) dans FIntPoint(q,r)
	switch (OffsetLayout)
	{
	case EML_HexOffsetLayout::OddR:
		{
			// q = col - (row - (row&1))/2 ; r = row
			const int32 Q = Col - ((Row - (Row & 1)) / 2);
			return FIntPoint(Q, Row);
		}
	case EML_HexOffsetLayout::EvenR:
		{
			// q = col - (row + (row&1))/2 ; r = row
			const int32 Q = Col - ((Row + (Row & 1)) / 2);
			return FIntPoint(Q, Row);
		}
	case EML_HexOffsetLayout::OddQ:
		{
			// q = col ; r = row - (col - (col&1))/2
			const int32 R = Row - ((Col - (Col & 1)) / 2);
			return FIntPoint(Col, R);
		}
	case EML_HexOffsetLayout::EvenQ:
	default:
		{
			// q = col ; r = row - (col + (col&1))/2
			const int32 R = Row - ((Col + (Col & 1)) / 2);
			return FIntPoint(Col, R);
		}
	}
}

void AML_BoardSpawner::SpawnRectangleWH()
{
	if (!TileClass) return;
	UWorld* World = GetWorld();
	if (!World) return;

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 Row = 0; Row < GridHeight; ++Row)
	{
		for (int32 Col = 0; Col < GridWidth; ++Col)
		{
			const FIntPoint Axial = OffsetToAxial(Col, Row); // (q,r)
			const int32 Q = Axial.X;
			const int32 R = Axial.Y;

			const FVector Location = AxialToWorld(Q, R);
			const FTransform TileTransform(TileRotation, Location, TileScale);

			AML_Tile* Tile = World->SpawnActor<AML_Tile>(TileClass, TileTransform, Params);
			if (!Tile) continue;
			Tile->Initialize(BiomeTileSet);

			SpawnedTiles.Add(Tile);
			Tile->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			const FIntPoint AxialQR(Q, R);
			Tile->SetAxialCoord(AxialQR);
			GridMap.Add(AxialQR, Tile);
		}
	}
}
