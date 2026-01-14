#include "MycelandBoardSpawner.h"
#include "MycelandTile.h"
#include "Engine/World.h"


AMycelandBoardSpawner::AMycelandBoardSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AMycelandBoardSpawner::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ClearTiles();

	if (bAutoDetectHexSize)
	{
		const float Detected = DetectSizeFromMesh();
		if (Detected > 0.f)
		{
			HexSize = Detected + 1.f;
		}
	}

	switch (GridLayout)
	{
	case EHexGridLayout::HexagonRadius:
		SpawnHexagonRadius();
		break;

	case EHexGridLayout::RectangleWH:
		SpawnRectangleWH();
		break;
	}
}


void AMycelandBoardSpawner::ClearTiles()
{
	for (AActor* Tile : SpawnedTiles)
	{
		if (IsValid(Tile))
		{
			Tile->Destroy();
		}
	}
	SpawnedTiles.Empty();
	TilesByAxial.Empty();
}

FVector AMycelandBoardSpawner::AxialToWorld(int32 Q, int32 R) const
{
	// Axial -> 2D (x,y), puis -> UE (X,Y)
	const float Sqrt3 = 1.73205080757f;

	float X2D = 0.f;
	float Y2D = 0.f;

	if (Orientation == EHexOrientation::FlatTop)
	{
		// x = size * (3/2 q)
		// y = size * (sqrt(3) * (r + q/2))
		X2D = HexSize * (1.5f * Q);
		Y2D = HexSize * (Sqrt3 * (R + 0.5f * Q));
	}
	else // PointyTop
	{
		// x = size * (sqrt(3) * (q + r/2))
		// y = size * (3/2 r)
		X2D = HexSize * (Sqrt3 * (Q + 0.5f * R));
		Y2D = HexSize * (1.5f * R);
	}

	// Dans UE: X,Y sur le plan, Z=0
	return GetActorLocation() + FVector(X2D, Y2D, 0.f);
}

void AMycelandBoardSpawner::SpawnHexagonRadius()
{
	if (!CaseClass) return;
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
			const FTransform TileTransform(TileRotation, Location);

			AMycelandTile* Tile = World->SpawnActor<AMycelandTile>(CaseClass, TileTransform, Params);
			if (!Tile) continue;

			SpawnedTiles.Add(Tile);
			TilesByAxial.Add(FIntPoint(Q, R), Tile);
		}
	}
}

float AMycelandBoardSpawner::DetectSizeFromMesh() const
{
	if (!CaseClass) return HexSize;

	// On récupère le CDO pour accéder au mesh sans spawn réel
	const AMycelandTile* DefaultCase = CaseClass->GetDefaultObject<AMycelandTile>();
	if (!DefaultCase) return HexSize;

	const UStaticMeshComponent* MeshComp = DefaultCase->FindComponentByClass<UStaticMeshComponent>();
	if (!MeshComp || !MeshComp->GetStaticMesh()) return HexSize;

	const FBoxSphereBounds Bounds = MeshComp->GetStaticMesh()->GetBounds();
	const FVector Extent = Bounds.BoxExtent;

	// UE units : BoxExtent = demi-taille
	if (Orientation == EHexOrientation::FlatTop)
	{
		// largeur sommet → sommet = 2 * Extent.X
		return Extent.X * CaseClass.GetDefaultObject()->GetActorScale().X;
	}
	else // PointyTop
	{
		// hauteur sommet → sommet = 2 * Extent.Y
		return Extent.Y * CaseClass.GetDefaultObject()->GetActorScale().Y;
	}
}

FIntPoint AMycelandBoardSpawner::OffsetToAxial(int32 Col, int32 Row) const
{
	// Retourne (q,r) dans FIntPoint(q,r)
	switch (OffsetLayout)
	{
	case EHexOffsetLayout::OddR:
		{
			// q = col - (row - (row&1))/2 ; r = row
			const int32 Q = Col - ((Row - (Row & 1)) / 2);
			return FIntPoint(Q, Row);
		}
	case EHexOffsetLayout::EvenR:
		{
			// q = col - (row + (row&1))/2 ; r = row
			const int32 Q = Col - ((Row + (Row & 1)) / 2);
			return FIntPoint(Q, Row);
		}
	case EHexOffsetLayout::OddQ:
		{
			// q = col ; r = row - (col - (col&1))/2
			const int32 R = Row - ((Col - (Col & 1)) / 2);
			return FIntPoint(Col, R);
		}
	case EHexOffsetLayout::EvenQ:
	default:
		{
			// q = col ; r = row - (col + (col&1))/2
			const int32 R = Row - ((Col + (Col & 1)) / 2);
			return FIntPoint(Col, R);
		}
	}
}

void AMycelandBoardSpawner::SpawnRectangleWH()
{
	if (!CaseClass) return;
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
			const FTransform TileTransform(TileRotation, Location);

			AMycelandTile* Tile = World->SpawnActor<AMycelandTile>(CaseClass, TileTransform, Params);
			if (!Tile) continue;

			SpawnedTiles.Add(Tile);
			TilesByAxial.Add(FIntPoint(Q, R), Tile);
		}
	}
}



