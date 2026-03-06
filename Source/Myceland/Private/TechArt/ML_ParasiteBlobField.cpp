// Copyright Myceland Team, All Rights Reserved.

#include "TechArt/ML_ParasiteBlobField.h"

#include "Components/SceneComponent.h"
#include "ProceduralMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"
#include "Tiles/ML_Tile.h"
#include "Tiles/ML_BoardSpawner.h"
#include "Core/ML_CoreData.h"
#include "EngineUtils.h"

AML_ParasiteBlobField::AML_ParasiteBlobField()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
	ProceduralMesh->SetupAttachment(SceneRoot);
	ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProceduralMesh->SetGenerateOverlapEvents(false);
	ProceduralMesh->bUseComplexAsSimpleCollision = false;
	ProceduralMesh->CastShadow = true;
}

void AML_ParasiteBlobField::BeginPlay()
{
	Super::BeginPlay();

	RefreshTargetsFromWorld();
	SnapToCurrentTargets();
	RebuildMeshFromState();

	bInitializedRuntime = true;
}

void AML_ParasiteBlobField::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (!bGenerateInEditor)
	{
		return;
	}

	RefreshTargetsFromWorld();
	SnapToCurrentTargets();
	RebuildMeshFromState();
}

void AML_ParasiteBlobField::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bAnimateAtRuntime || !GetWorld() || !GetWorld()->IsGameWorld())
	{
		return;
	}

	ScanAccumulator += DeltaSeconds;
	if (ScanAccumulator >= ScanInterval)
	{
		ScanAccumulator = 0.f;
		RefreshTargetsFromWorld();
	}

	const bool bChanged = UpdateAnimation(DeltaSeconds);
	if (bChanged)
	{
		RebuildMeshFromState();
	}
}

void AML_ParasiteBlobField::ClearBlobMesh()
{
	if (ProceduralMesh)
	{
		ProceduralMesh->ClearAllMeshSections();
	}
}

void AML_ParasiteBlobField::RebuildParasiteBlobs()
{
	RefreshTargetsFromWorld();
	SnapToCurrentTargets();
	RebuildMeshFromState();
}

void AML_ParasiteBlobField::RefreshTargetsFromWorld()
{
	for (TPair<AML_Tile*, FTileAnimState>& Pair : TileStates)
	{
		Pair.Value.TargetAlpha = 0.f;
	}

	for (TPair<FML_ParasiteBridgeKey, FBridgeAnimState>& Pair : BridgeStates)
	{
		Pair.Value.TargetAlpha = 0.f;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AML_Tile*> CurrentParasites;
	CurrentParasites.Reserve(128);

	for (TActorIterator<AML_Tile> It(World); It; ++It)
	{
		AML_Tile* Tile = *It;
		if (!IsParasiteTile(Tile))
		{
			continue;
		}

		CurrentParasites.Add(Tile);

		FTileAnimState& State = TileStates.FindOrAdd(Tile);
		State.Tile = Tile;
		State.TargetAlpha = 1.f;
	}

	for (AML_Tile* Tile : CurrentParasites)
	{
		if (!IsValid(Tile))
		{
			continue;
		}

		AML_BoardSpawner* Board = Tile->GetBoardSpawnerFromTile();
		if (!IsValid(Board))
		{
			continue;
		}

		const TArray<AML_Tile*> Neighbors = Board->GetNeighbors(Tile);

		for (AML_Tile* Neighbor : Neighbors)
		{
			if (!IsParasiteTile(Neighbor))
			{
				continue;
			}

			if (Neighbor->GetBoardSpawnerFromTile() != Board)
			{
				continue;
			}

			if (!ShouldCreateBridge(Tile, Neighbor))
			{
				continue;
			}

			const FML_ParasiteBridgeKey Key = MakeBridgeKey(Tile, Neighbor);
			FBridgeAnimState& BridgeState = BridgeStates.FindOrAdd(Key);
			BridgeState.Key = Key;
			BridgeState.TargetAlpha = 1.f;
		}
	}
}

bool AML_ParasiteBlobField::UpdateAnimation(float DeltaSeconds)
{
	bool bAnyChanged = false;

	TArray<AML_Tile*> TilesToRemove;
	for (TPair<AML_Tile*, FTileAnimState>& Pair : TileStates)
	{
		AML_Tile* Tile = Pair.Key;
		FTileAnimState& State = Pair.Value;

		if (!IsValid(Tile))
		{
			TilesToRemove.Add(Tile);
			bAnyChanged = true;
			continue;
		}

		const float PrevAlpha = State.Alpha;
		const float Speed = (State.TargetAlpha > State.Alpha) ? BlobGrowSpeed : BlobShrinkSpeed;
		State.Alpha = MoveAlphaTowards(State.Alpha, State.TargetAlpha, Speed, DeltaSeconds);

		if (!FMath::IsNearlyEqual(PrevAlpha, State.Alpha, KINDA_SMALL_NUMBER))
		{
			bAnyChanged = true;
		}

		if (State.TargetAlpha <= KINDA_SMALL_NUMBER && State.Alpha <= 0.001f)
		{
			TilesToRemove.Add(Tile);
			bAnyChanged = true;
		}
	}

	for (AML_Tile* Tile : TilesToRemove)
	{
		TileStates.Remove(Tile);
	}

	TArray<FML_ParasiteBridgeKey> BridgesToRemove;
	for (TPair<FML_ParasiteBridgeKey, FBridgeAnimState>& Pair : BridgeStates)
	{
		const FML_ParasiteBridgeKey Key = Pair.Key;
		FBridgeAnimState& State = Pair.Value;

		if (!IsValid(Key.A) || !IsValid(Key.B))
		{
			BridgesToRemove.Add(Key);
			bAnyChanged = true;
			continue;
		}

		const float PrevAlpha = State.Alpha;
		const float Speed = (State.TargetAlpha > State.Alpha) ? BridgeGrowSpeed : BridgeShrinkSpeed;
		State.Alpha = MoveAlphaTowards(State.Alpha, State.TargetAlpha, Speed, DeltaSeconds);

		if (!FMath::IsNearlyEqual(PrevAlpha, State.Alpha, KINDA_SMALL_NUMBER))
		{
			bAnyChanged = true;
		}

		if (State.TargetAlpha <= KINDA_SMALL_NUMBER && State.Alpha <= 0.001f)
		{
			BridgesToRemove.Add(Key);
			bAnyChanged = true;
		}
	}

	for (const FML_ParasiteBridgeKey& Key : BridgesToRemove)
	{
		BridgeStates.Remove(Key);
	}

	return bAnyChanged;
}

void AML_ParasiteBlobField::SnapToCurrentTargets()
{
	TArray<AML_Tile*> TilesToRemove;
	for (TPair<AML_Tile*, FTileAnimState>& Pair : TileStates)
	{
		if (!IsValid(Pair.Key))
		{
			TilesToRemove.Add(Pair.Key);
			continue;
		}

		Pair.Value.Alpha = Pair.Value.TargetAlpha;
	}

	for (AML_Tile* Tile : TilesToRemove)
	{
		TileStates.Remove(Tile);
	}

	TArray<FML_ParasiteBridgeKey> BridgesToRemove;
	for (TPair<FML_ParasiteBridgeKey, FBridgeAnimState>& Pair : BridgeStates)
	{
		if (!IsValid(Pair.Key.A) || !IsValid(Pair.Key.B))
		{
			BridgesToRemove.Add(Pair.Key);
			continue;
		}

		Pair.Value.Alpha = Pair.Value.TargetAlpha;
	}

	for (const FML_ParasiteBridgeKey& Key : BridgesToRemove)
	{
		BridgeStates.Remove(Key);
	}
}

void AML_ParasiteBlobField::RebuildMeshFromState()
{
	if (!ProceduralMesh)
	{
		return;
	}

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector2D> UVs;
	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;
	TArray<FColor> VertexColors;

	TMap<AML_Tile*, FTileBlobData> BlobDataByTile;
	BlobDataByTile.Reserve(TileStates.Num());

	for (const TPair<AML_Tile*, FTileAnimState>& Pair : TileStates)
	{
		AML_Tile* Tile = Pair.Key;
		const FTileAnimState& State = Pair.Value;

		if (!IsValid(Tile) || State.Alpha <= 0.001f)
		{
			continue;
		}

		FTileBlobData BlobData;
		AddBlobForTile(Tile, State.Alpha, Vertices, Triangles, UVs, BlobData);
		BlobDataByTile.Add(Tile, BlobData);
	}

	for (const TPair<FML_ParasiteBridgeKey, FBridgeAnimState>& Pair : BridgeStates)
	{
		const FML_ParasiteBridgeKey& Key = Pair.Key;
		const FBridgeAnimState& BridgeState = Pair.Value;

		if (BridgeState.Alpha <= 0.001f)
		{
			continue;
		}

		AML_Tile* TileA = Key.A;
		AML_Tile* TileB = Key.B;

		if (!IsValid(TileA) || !IsValid(TileB))
		{
			continue;
		}

		const FTileBlobData* BlobA = BlobDataByTile.Find(TileA);
		const FTileBlobData* BlobB = BlobDataByTile.Find(TileB);
		if (!BlobA || !BlobB)
		{
			continue;
		}

		const int32 SideA = FindSideIndexToNeighbor(TileA, TileB);
		const int32 SideB = FindSideIndexToNeighbor(TileB, TileA);
		if (SideA == INDEX_NONE || SideB == INDEX_NONE)
		{
			continue;
		}

		AddBridgeBetweenTiles(*BlobA, TileA, SideA, *BlobB, TileB, SideB, BridgeState.Alpha, Vertices, Triangles, UVs);
	}

	if (Vertices.Num() == 0 || Triangles.Num() == 0)
	{
		ProceduralMesh->ClearAllMeshSections();
		return;
	}

	Normals.SetNumZeroed(Vertices.Num());
	Tangents.SetNumZeroed(Vertices.Num());
	VertexColors.Init(FColor::White, Vertices.Num());

	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UVs, Normals, Tangents);

	ProceduralMesh->ClearAllMeshSections();
	ProceduralMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, false);

	if (BlobMaterial)
	{
		ProceduralMesh->SetMaterial(0, BlobMaterial);
	}
}

void AML_ParasiteBlobField::AddBlobForTile(
	AML_Tile* Tile,
	float TileAlpha,
	TArray<FVector>& Vertices,
	TArray<int32>& Triangles,
	TArray<FVector2D>& UVs,
	FTileBlobData& OutBlobData
) const
{
	if (!IsValid(Tile))
	{
		return;
	}

	AML_BoardSpawner* Board = Tile->GetBoardSpawnerFromTile();
	if (!IsValid(Board))
	{
		return;
	}

	const FIntPoint Axial = Tile->GetAxialCoord();
	const FVector TileLocation = Tile->GetActorLocation();
	const FVector BaseCenterWS = TileLocation + FVector(0.f, 0.f, ZOffset);

	OutBlobData.Tile = Tile;
	OutBlobData.Board = Board;
	OutBlobData.CenterWS = BaseCenterWS;

	const TArray<AML_Tile*> Neighbors = Board->GetNeighbors(Tile);

	FVector SideDirs[6];
	for (int32 Side = 0; Side < 6; ++Side)
	{
		SideDirs[Side] = GetDirectionVectorForSide(Tile, Side);
	}

	const float RadiusSpreadScale = FMath::Lerp(DeadSpreadFactor, 1.0f, TileAlpha);
	const float EffectiveBlobRadius = BlobRadius * RadiusSpreadScale;
	const float EffectiveCenterHeight =
		(CenterHeight + FMath::Lerp(-RandomHeightJitter, RandomHeightJitter, StableRand01(Axial.X, Axial.Y, 1001)))
		* FMath::Pow(FMath::Max(TileAlpha, 0.001f), 0.72f);

	const float EffectiveInnerHeight =
		(InnerRingHeight + FMath::Lerp(-RandomHeightJitter * 0.5f, RandomHeightJitter * 0.5f, StableRand01(Axial.X, Axial.Y, 1002)))
		* FMath::Pow(FMath::Max(TileAlpha, 0.001f), 0.78f);

	const FVector CenterTopWS = BaseCenterWS + FVector(0.f, 0.f, EffectiveCenterHeight);

	const int32 CenterIndex = AddVertex(
		Vertices,
		UVs,
		WorldToLocalPosition(CenterTopWS),
		FVector2D(BaseCenterWS.X / UVScale, BaseCenterWS.Y / UVScale)
	);

	static constexpr int32 SamplesPerSide = 3;
	static constexpr int32 TotalSamples = 18;

	FVector OuterWS[TotalSamples];
	FVector InnerWS[TotalSamples];
	int32 OuterIndices[TotalSamples];
	int32 InnerIndices[TotalSamples];

	for (int32 Side = 0; Side < 6; ++Side)
	{
		const int32 PrevSide = (Side + 5) % 6;
		const int32 NextSide = (Side + 1) % 6;

		const FVector PrevBlendDir = (SideDirs[PrevSide] + SideDirs[Side]).GetSafeNormal();
		const FVector MidDir = SideDirs[Side];
		const FVector NextBlendDir = (SideDirs[Side] + SideDirs[NextSide]).GetSafeNormal();

		const FVector SampleDirs[SamplesPerSide] =
		{
			PrevBlendDir,
			MidDir,
			NextBlendDir
		};

		const float SampleShapeWeights[SamplesPerSide] =
		{
			0.84f,
			1.00f,
			0.84f
		};

		AML_Tile* NeighborTile = Neighbors.IsValidIndex(Side) ? Neighbors[Side] : nullptr;
		const float BridgeAlpha = GetBridgeAlpha(Tile, NeighborTile);

		const float ArmExtra = FMath::Lerp(UnconnectedArmExtraLength, ConnectedArmExtraLength, BridgeAlpha);

		for (int32 LocalSample = 0; LocalSample < SamplesPerSide; ++LocalSample)
		{
			const int32 GlobalIndex = Side * SamplesPerSide + LocalSample;

			const float ShapeWeight = SampleShapeWeights[LocalSample];

			const float Wave1 = FMath::Sin((GlobalIndex * 0.95f) + StableRand01(Axial.X, Axial.Y, 200 + GlobalIndex) * 6.283185f);
			const float Wave2 = FMath::Sin((GlobalIndex * 2.35f) + StableRand01(Axial.X, Axial.Y, 500 + GlobalIndex) * 6.283185f);

			const float RadiusNoise =
				Wave1 * EdgeUndulationAmplitude +
				Wave2 * EdgeUndulationSecondary +
				FMath::Lerp(-RandomRadiusJitter, RandomRadiusJitter, StableRand01(Axial.X, Axial.Y, 800 + GlobalIndex));

			float SampleRadius = EffectiveBlobRadius * ShapeWeight + RadiusNoise;

			if (LocalSample == 1)
			{
				SampleRadius += ArmExtra * FMath::Lerp(0.35f, 1.0f, TileAlpha);
			}

			const FVector SampleDir = SampleDirs[LocalSample];
			const float OuterZ = BaseCenterWS.Z - (EdgeDrop * FMath::Lerp(1.25f, 1.0f, TileAlpha));
			const FVector OuterPointWS = FVector(
				BaseCenterWS.X + SampleDir.X * SampleRadius,
				BaseCenterWS.Y + SampleDir.Y * SampleRadius,
				OuterZ
			);

			const float InnerRadius = SampleRadius * InnerRingRadiusFactor * MidInsetFactor / 0.66f;
			const float InnerLocalZ = BaseCenterWS.Z + EffectiveInnerHeight * (LocalSample == 1 ? 1.0f : 0.82f);
			const FVector InnerPointWS = FVector(
				BaseCenterWS.X + SampleDir.X * InnerRadius,
				BaseCenterWS.Y + SampleDir.Y * InnerRadius,
				InnerLocalZ
			);

			OuterWS[GlobalIndex] = OuterPointWS;
			InnerWS[GlobalIndex] = InnerPointWS;

			if (LocalSample == 1)
			{
				OutBlobData.TipWS[Side] = OuterPointWS;
			}
		}
	}

	for (int32 i = 0; i < TotalSamples; ++i)
	{
		InnerIndices[i] = AddVertex(
			Vertices,
			UVs,
			WorldToLocalPosition(InnerWS[i]),
			FVector2D(InnerWS[i].X / UVScale, InnerWS[i].Y / UVScale)
		);

		OuterIndices[i] = AddVertex(
			Vertices,
			UVs,
			WorldToLocalPosition(OuterWS[i]),
			FVector2D(OuterWS[i].X / UVScale, OuterWS[i].Y / UVScale)
		);
	}

	for (int32 i = 0; i < TotalSamples; ++i)
	{
		const int32 Next = (i + 1) % TotalSamples;

		AddTriangle(Triangles, CenterIndex, InnerIndices[Next], InnerIndices[i]);
		AddTriangle(Triangles, InnerIndices[i], InnerIndices[Next], OuterIndices[i]);
		AddTriangle(Triangles, InnerIndices[Next], OuterIndices[Next], OuterIndices[i]);
	}
}

void AML_ParasiteBlobField::AddBridgeBetweenTiles(
	const FTileBlobData& A,
	AML_Tile* TileA,
	int32 SideA,
	const FTileBlobData& B,
	AML_Tile* TileB,
	int32 SideB,
	float BridgeAlpha,
	TArray<FVector>& Vertices,
	TArray<int32>& Triangles,
	TArray<FVector2D>& UVs
) const
{
	if (!IsValid(TileA) || !IsValid(TileB))
	{
		return;
	}

	const FVector DirA = GetDirectionVectorForSide(TileA, SideA);
	const FVector DirB = GetDirectionVectorForSide(TileB, SideB);

	const FVector RootA = A.CenterWS + DirA * (BlobRadius * 0.38f);
	const FVector RootB = B.CenterWS + DirB * (BlobRadius * 0.38f);

	const FVector StartWS = FMath::Lerp(RootA, A.TipWS[SideA] + FVector(0.f, 0.f, BridgeEndLift), BridgeAlpha);
	const FVector EndWS = FMath::Lerp(RootB, B.TipWS[SideB] + FVector(0.f, 0.f, BridgeEndLift), BridgeAlpha);

	FVector Axis = EndWS - StartWS;
	const float Length = Axis.Length();
	if (Length <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	Axis /= Length;

	FVector Side = FVector::CrossProduct(FVector::UpVector, Axis).GetSafeNormal();
	if (Side.IsNearlyZero())
	{
		Side = FVector::RightVector;
	}

	const FIntPoint AxialA = TileA->GetAxialCoord();
	const FIntPoint AxialB = TileB->GetAxialCoord();
	const float WavePhase = StableRand01(AxialA.X + AxialB.X, AxialA.Y + AxialB.Y, 3100) * 6.283185f;

	TArray<int32> LeftIndices;
	TArray<int32> RightIndices;
	LeftIndices.Reserve(BridgeSegments + 1);
	RightIndices.Reserve(BridgeSegments + 1);

	for (int32 i = 0; i <= BridgeSegments; ++i)
	{
		const float T = static_cast<float>(i) / static_cast<float>(BridgeSegments);
		const float Arc = FMath::Sin(T * PI);
		const float RetractProfile = FMath::Pow(Arc, 0.85f);

		const FVector BaseCenterWS = FMath::Lerp(StartWS, EndWS, T);
		const float LateralWave = FMath::Sin((T * PI * 2.0f) + WavePhase) * BridgeWaveAmplitude * RetractProfile * BridgeAlpha;
		const float HeightWave = Arc * BridgeArchHeight * BridgeAlpha;

		const FVector CenterWS = BaseCenterWS + Side * LateralWave + FVector(0.f, 0.f, HeightWave);

		const float WidthNoise = 1.0f + FMath::Sin((T * PI * 3.0f) + WavePhase * 1.71f) * BridgeWidthNoiseAmplitude;
		const float Width = FMath::Lerp(BridgeEndWidth, BridgeMidWidth, Arc) * WidthNoise * FMath::Lerp(0.35f, 1.0f, BridgeAlpha);

		const FVector LeftWS = CenterWS - Side * Width;
		const FVector RightWS = CenterWS + Side * Width;

		LeftIndices.Add(AddVertex(
			Vertices,
			UVs,
			WorldToLocalPosition(LeftWS),
			FVector2D(0.f, (Length * T) / UVScale)
		));

		RightIndices.Add(AddVertex(
			Vertices,
			UVs,
			WorldToLocalPosition(RightWS),
			FVector2D(1.f, (Length * T) / UVScale)
		));
	}

	for (int32 i = 0; i < BridgeSegments; ++i)
	{
		const int32 L0 = LeftIndices[i];
		const int32 R0 = RightIndices[i];
		const int32 L1 = LeftIndices[i + 1];
		const int32 R1 = RightIndices[i + 1];

		AddTriangle(Triangles, L0, R0, L1);
		AddTriangle(Triangles, R0, R1, L1);
	}
}

int32 AML_ParasiteBlobField::FindSideIndexToNeighbor(AML_Tile* FromTile, AML_Tile* ToTile) const
{
	if (!IsValid(FromTile) || !IsValid(ToTile))
	{
		return INDEX_NONE;
	}

	AML_BoardSpawner* Board = FromTile->GetBoardSpawnerFromTile();
	if (!IsValid(Board))
	{
		return INDEX_NONE;
	}

	const TArray<AML_Tile*> Neighbors = Board->GetNeighbors(FromTile);

	for (int32 i = 0; i < Neighbors.Num(); ++i)
	{
		if (Neighbors[i] == ToTile)
		{
			return i;
		}
	}

	return INDEX_NONE;
}

FVector AML_ParasiteBlobField::GetDirectionVectorForSide(AML_Tile* Tile, int32 SideIndex) const
{
	if (!IsValid(Tile))
	{
		return FVector::ForwardVector;
	}

	AML_BoardSpawner* Board = Tile->GetBoardSpawnerFromTile();
	if (IsValid(Board))
	{
		const TArray<AML_Tile*> Neighbors = Board->GetNeighbors(Tile);
		if (Neighbors.IsValidIndex(SideIndex) && IsValid(Neighbors[SideIndex]))
		{
			FVector Delta = Neighbors[SideIndex]->GetActorLocation() - Tile->GetActorLocation();
			Delta.Z = 0.f;

			if (!Delta.IsNearlyZero())
			{
				return Delta.GetSafeNormal();
			}
		}
	}

	const float AngleDeg = 60.f * static_cast<float>(SideIndex);
	const float AngleRad = FMath::DegreesToRadians(AngleDeg);
	return FVector(FMath::Cos(AngleRad), FMath::Sin(AngleRad), 0.f).GetSafeNormal();
}

float AML_ParasiteBlobField::GetBridgeAlpha(AML_Tile* TileA, AML_Tile* TileB) const
{
	if (!IsValid(TileA) || !IsValid(TileB))
	{
		return 0.f;
	}

	const FML_ParasiteBridgeKey Key = MakeBridgeKey(TileA, TileB);
	if (const FBridgeAnimState* State = BridgeStates.Find(Key))
	{
		return State->Alpha;
	}

	return 0.f;
}

FML_ParasiteBridgeKey AML_ParasiteBlobField::MakeBridgeKey(AML_Tile* A, AML_Tile* B) const
{
	return FML_ParasiteBridgeKey(A, B);
}

bool AML_ParasiteBlobField::ShouldCreateBridge(AML_Tile* A, AML_Tile* B) const
{
	return IsValid(A) && IsValid(B) && A < B;
}

bool AML_ParasiteBlobField::IsParasiteTile(AML_Tile* Tile) const
{
	return IsValid(Tile)
		&& Tile->GetCurrentType() == EML_TileType::Parasite
		&& IsValid(Tile->GetBoardSpawnerFromTile());
}

float AML_ParasiteBlobField::StableRand01(int32 X, int32 Y, int32 Salt) const
{
	uint32 H = static_cast<uint32>(X) * 374761393u;
	H += static_cast<uint32>(Y) * 668265263u;
	H ^= static_cast<uint32>(Salt) * 2246822519u;
	H ^= static_cast<uint32>(NoiseSeed) * 3266489917u;
	H = (H ^ (H >> 13u)) * 1274126177u;
	H ^= (H >> 16u);

	return static_cast<float>(H & 0x00FFFFFF) / static_cast<float>(0x00FFFFFF);
}

float AML_ParasiteBlobField::MoveAlphaTowards(float Current, float Target, float Speed, float DeltaSeconds) const
{
	if (FMath::IsNearlyEqual(Current, Target, KINDA_SMALL_NUMBER))
	{
		return Target;
	}

	const float Step = Speed * DeltaSeconds;
	return FMath::FInterpConstantTo(Current, Target, DeltaSeconds, Speed);
}

FVector AML_ParasiteBlobField::WorldToLocalPosition(const FVector& WorldPos) const
{
	return GetActorTransform().InverseTransformPosition(WorldPos);
}

int32 AML_ParasiteBlobField::AddVertex(
	TArray<FVector>& Vertices,
	TArray<FVector2D>& UVs,
	const FVector& Position,
	const FVector2D& UV
) const
{
	const int32 Index = Vertices.Num();
	Vertices.Add(Position);
	UVs.Add(UV);
	return Index;
}

void AML_ParasiteBlobField::AddTriangle(TArray<int32>& Triangles, int32 A, int32 B, int32 C) const
{
	Triangles.Add(A);
	Triangles.Add(C);
	Triangles.Add(B);
}