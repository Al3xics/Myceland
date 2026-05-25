// Copyright Myceland Team, All Rights Reserved.

#include "TechArt/ML_LakeFishManager.h"

#include "Components/SplineComponent.h"
#include "Core/ML_CoreData.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Tiles/ML_BoardSpawner.h"
#include "Tiles/ML_Tile.h"
#include "Tiles/TileBase/ML_TileGrass.h"
#include "Tiles/TileBase/ML_TileWater.h"
#include "Tiles/TileBase/ML_TileTree.h"

AML_LakeFishManager::AML_LakeFishManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
}

void AML_LakeFishManager::BeginPlay()
{
	Super::BeginPlay();

	GatherBoardsAndTiles();
	BindTileEvents();
	RebuildLakes();
}

void AML_LakeFishManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (FML_LakeRuntime& Lake : Lakes)
	{
		DestroyLake(Lake);
	}
	Lakes.Empty();

	Super::EndPlay(EndPlayReason);
}

void AML_LakeFishManager::AssignClosestSplineWithoutTeleport(const FML_LakeRuntime& Lake, FML_LakeFishRuntime& FishData)
{
	AActor* Fish = FishData.Fish.Get();
	if (!IsValid(Fish) || Lake.Splines.IsEmpty()) return;

	const FVector FishLocation = Fish->GetActorLocation();

	USplineComponent* BestSpline = nullptr;
	float BestDistanceAlongSpline = 0.f;
	float BestDistanceSq = TNumericLimits<float>::Max();

	for (USplineComponent* Spline : Lake.Splines)
	{
		if (!IsValid(Spline)) continue;

		const FVector ClosestLocation = Spline->FindLocationClosestToWorldLocation(
			FishLocation,
			ESplineCoordinateSpace::World
		);

		const float DistanceSq = FVector::DistSquared(FishLocation, ClosestLocation);

		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			BestSpline = Spline;

			BestDistanceAlongSpline = Spline->FindInputKeyClosestToWorldLocation(FishLocation);
			BestDistanceAlongSpline = Spline->GetDistanceAlongSplineAtSplineInputKey(BestDistanceAlongSpline);
		}
	}

	if (!IsValid(BestSpline)) return;

	FishData.Spline = BestSpline;
	FishData.Distance = BestDistanceAlongSpline;
}

void AML_LakeFishManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	bool bHasValidFish = false;

	for (FML_LakeRuntime& Lake : Lakes)
	{
		for (FML_LakeFishRuntime& FishData : Lake.Fish)
		{
			AActor* Fish = FishData.Fish.Get();
			if (!IsValid(Fish)) continue;

			bHasValidFish = true;

			const FVector PreviousLocation = Fish->GetActorLocation();

			if (FishData.CurrentTargetLocation.IsNearlyZero())
			{
				AssignNewFishTarget(FishData, Lake);
			}

			const FVector ToTarget = FishData.CurrentTargetLocation - PreviousLocation;
			const float DistanceToTarget = ToTarget.Size();

			if (DistanceToTarget <= FishTargetAcceptanceRadius)
			{
				AssignNewFishTarget(FishData, Lake);
			}

			const FVector Direction = (FishData.CurrentTargetLocation - PreviousLocation).GetSafeNormal();
			if (Direction.IsNearlyZero()) continue;

			const FVector Right = FVector::CrossProduct(FVector::UpVector, Direction).GetSafeNormal();
			const float Wave = FMath::Sin(GetWorld()->GetTimeSeconds() * UndulationFrequency + FishData.WavePhase) * UndulationAmplitude;

			const FVector DesiredLocation = PreviousLocation + Direction * FishSpeed * FishData.SpeedMultiplier * DeltaSeconds + Right * Wave * DeltaSeconds;

			const FVector NewLocation = FMath::VInterpConstantTo(
				PreviousLocation,
				DesiredLocation,
				DeltaSeconds,
				FishSpeed * FishData.SpeedMultiplier
			);

			Fish->SetActorLocation(NewLocation);

			const FVector RealMoveDirection = (NewLocation - PreviousLocation).GetSafeNormal();
			if (!RealMoveDirection.IsNearlyZero())
			{
				FishData.LastMoveDirection = RealMoveDirection;
			}

			if (bRotateFishAlongSpline && !FishData.LastMoveDirection.IsNearlyZero())
			{
				const FRotator TargetRotation = FishData.LastMoveDirection.Rotation();

				const FRotator NewRotation = FMath::RInterpTo(
					Fish->GetActorRotation(),
					TargetRotation,
					DeltaSeconds,
					FishRotationInterpSpeed
				);

				Fish->SetActorRotation(NewRotation);
			}
		}
	}

	SetActorTickEnabled(bHasValidFish);
}

FVector AML_LakeFishManager::PickRandomLakeTarget(const FML_LakeRuntime& Lake, const FML_LakeFishRuntime& FishData) const
{
	TArray<FVector> ValidPoints;

	for (USplineComponent* Spline : Lake.Splines)
	{
		if (!IsValid(Spline)) continue;

		const int32 NumPoints = Spline->GetNumberOfSplinePoints();

		for (int32 i = 0; i < NumPoints; ++i)
		{
			ValidPoints.Add(
			Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World) + FVector(0.f, 0.f, FishZOffset + FishData.FishZOffsetRandom)
			);
		}
	}

	if (ValidPoints.IsEmpty())
	{
		return FVector::ZeroVector;
	}

	return ValidPoints[FMath::RandRange(0, ValidPoints.Num() - 1)];
}

void AML_LakeFishManager::AssignNewFishTarget(FML_LakeFishRuntime& FishData, const FML_LakeRuntime& Lake)
{
	AActor* Fish = FishData.Fish.Get();
	USplineComponent* Spline = FishData.Spline.Get();

	if (!IsValid(Fish) || !IsValid(Spline)) return;

	const int32 NumPoints = Spline->GetNumberOfSplinePoints();
	if (NumPoints < 2) return;

	const FVector CurrentLocation = Fish->GetActorLocation();

	int32 ClosestIndex = 0;
	float ClosestDistSq = TNumericLimits<float>::Max();

	for (int32 i = 0; i < NumPoints; ++i)
	{
		const FVector PointLocation =
			Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World)
			+ FVector(0.f, 0.f, FishZOffset + FishData.FishZOffsetRandom);

		const float DistSq = FVector::DistSquared(CurrentLocation, PointLocation);

		if (DistSq < ClosestDistSq)
		{
			ClosestDistSq = DistSq;
			ClosestIndex = i;
		}
	}

	TArray<int32> CandidateIndices;

	if (ClosestIndex > 0)
	{
		CandidateIndices.Add(ClosestIndex - 1);
	}

	if (ClosestIndex < NumPoints - 1)
	{
		CandidateIndices.Add(ClosestIndex + 1);
	}

	if (CandidateIndices.IsEmpty())
	{
		return;
	}

	const int32 TargetIndex = CandidateIndices[FMath::RandRange(0, CandidateIndices.Num() - 1)];

	FishData.CurrentTargetLocation =
		Spline->GetLocationAtSplinePoint(TargetIndex, ESplineCoordinateSpace::World)
		+ FVector(0.f, 0.f, FishZOffset + FishData.FishZOffsetRandom);
}

AML_TileWater* AML_LakeFishManager::GetWaterActorFromTile(const AML_Tile* Tile) const
{
	if (!IsValid(Tile)) return nullptr;

	const UChildActorComponent* ChildComponent = Tile->GetTileChildActor();
	return ChildComponent ? Cast<AML_TileWater>(ChildComponent->GetChildActor()) : nullptr;
}

void AML_LakeFishManager::GatherBoardsAndTiles()
{
	BoardSpawners.Empty();
	CachedTiles.Empty();

	TArray<AActor*> FoundBoards;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AML_BoardSpawner::StaticClass(), FoundBoards);

	for (AActor* Actor : FoundBoards)
	{
		AML_BoardSpawner* Board = Cast<AML_BoardSpawner>(Actor);
		if (!IsValid(Board)) continue;

		BoardSpawners.Add(Board);

		TArray<AML_Tile*> BoardTiles = Board->GetGridTiles();
		for (AML_Tile* Tile : BoardTiles)
		{
			if (IsValid(Tile))
			{
				CachedTiles.AddUnique(Tile);
			}
		}
	}
}

void AML_LakeFishManager::BindTileEvents()
{
	for (AML_Tile* Tile : CachedTiles)
	{
		if (!IsValid(Tile)) continue;

		Tile->OnTileChangedNative.RemoveDynamic(this, &AML_LakeFishManager::HandleTileTypeChanged);
		Tile->OnTileChangedNative.AddDynamic(this, &AML_LakeFishManager::HandleTileTypeChanged);
	}
}

void AML_LakeFishManager::HandleTileTypeChanged(AML_Tile* Tile)
{
	RebuildLakes();
}

void AML_LakeFishManager::CleanupInvalidCachedTiles()
{
	CachedTiles.RemoveAll([](const TObjectPtr<AML_Tile>& Tile)
	{
		return !IsValid(Tile.Get());
	});
}

bool AML_LakeFishManager::IsWaterTile(const AML_Tile* Tile) const
{
	if (!IsValid(Tile)) return false;

	if (Tile->GetCurrentType() != EML_TileType::Water && Tile->GetCurrentType() != EML_TileType::WaterPath)
	{
		return false;
	}

	const UChildActorComponent* ChildComponent = Tile->GetTileChildActor();
	return ChildComponent && Cast<AML_TileWater>(ChildComponent->GetChildActor()) != nullptr;
}

bool AML_LakeFishManager::IsGrassTile(const AML_Tile* Tile) const
{
	if (!IsValid(Tile)) return false;

	if (Tile->GetCurrentType() != EML_TileType::Grass)
	{
		return false;
	}

	const UChildActorComponent* ChildComponent = Tile->GetTileChildActor();
	return ChildComponent && Cast<AML_TileGrass>(ChildComponent->GetChildActor()) != nullptr;
}

bool AML_LakeFishManager::IsTreeTile(const AML_Tile* Tile) const
{
	if (!IsValid(Tile)) return false;

	if (Tile->GetCurrentType() != EML_TileType::Tree)
	{
		return false;
	}

	return true;
}

bool AML_LakeFishManager::IsTileAliveWater(const AML_Tile* Tile) const
{
	if (!IsWaterTile(Tile)) return false;

	AML_BoardSpawner* Board = Tile->GetBoardSpawnerFromTile();
	if (!IsValid(Board)) return false;

	int32 TreeNeighborCount = 0;

	for (AML_Tile* Neighbor : Board->GetNeighbors(const_cast<AML_Tile*>(Tile)))
	{
		if (IsGrassTile(Neighbor))
		{
			return true;
		}

		if (IsTreeTile(Neighbor))
		{
			TreeNeighborCount++;

			if (TreeNeighborCount >= 2)
			{
				return true;
			}
		}
	}

	return false;
}

void AML_LakeFishManager::FloodFillWaterLake(AML_Tile* StartTile, TSet<AML_Tile*>& Visited, TArray<AML_Tile*>& OutTiles) const
{
	if (!IsValid(StartTile) || !IsWaterTile(StartTile)) return;

	TArray<AML_Tile*> Stack;
	Stack.Add(StartTile);

	while (!Stack.IsEmpty())
	{
		AML_Tile* Tile = Stack.Pop(EAllowShrinking::No);
		if (!IsValid(Tile) || Visited.Contains(Tile) || !IsWaterTile(Tile)) continue;

		Visited.Add(Tile);
		OutTiles.Add(Tile);

		AML_BoardSpawner* Board = Tile->GetBoardSpawnerFromTile();
		if (!IsValid(Board)) continue;

		for (AML_Tile* Neighbor : Board->GetNeighbors(Tile))
		{
			if (IsValid(Neighbor) && !Visited.Contains(Neighbor) && IsWaterTile(Neighbor))
			{
				Stack.Add(Neighbor);
			}
		}
	}
}

bool AML_LakeFishManager::DoesLakeTouchGrass(const TArray<AML_Tile*>& LakeTiles) const
{
	for (AML_Tile* Tile : LakeTiles)
	{
		if (IsTileAliveWater(Tile))
		{
			return true;
		}
	}

	return false;
}

void AML_LakeFishManager::RebuildLakes()
{
	CleanupInvalidCachedTiles();

	GatherBoardsAndTiles();
	BindTileEvents();

	TArray<FML_LakeRuntime> OldLakes = MoveTemp(Lakes);
	Lakes.Empty();

	TSet<AML_Tile*> Visited;
	TSet<AML_Tile*> AliveWaterTiles;

	for (AML_Tile* Tile : CachedTiles)
	{
		if (!IsValid(Tile) || Visited.Contains(Tile) || !IsWaterTile(Tile)) continue;

		TArray<AML_Tile*> LakeTiles;
		FloodFillWaterLake(Tile, Visited, LakeTiles);

		if (LakeTiles.IsEmpty() || !DoesLakeTouchGrass(LakeTiles))
		{
			continue;
		}

		for (AML_Tile* LakeTile : LakeTiles)
		{
			AliveWaterTiles.Add(LakeTile);
		}

		FML_LakeRuntime NewLake;
		for (AML_Tile* LakeTile : LakeTiles)
		{
			NewLake.Tiles.Add(LakeTile);
		}

		TArray<FML_LakeFishRuntime> ReusableFish;

		for (int32 Index = OldLakes.Num() - 1; Index >= 0; --Index)
		{
			if (!LakesOverlap(OldLakes[Index], LakeTiles)) continue;

			ReusableFish.Append(OldLakes[Index].Fish);
			OldLakes[Index].Fish.Empty();

			for (USplineComponent* Spline : OldLakes[Index].Splines)
			{
				DestroySpline(Spline);
			}
			OldLakes[Index].Splines.Empty();

			OldLakes.RemoveAtSwap(Index);
		}

		BuildLakeSplines(NewLake);
		ReconcileFish(NewLake, ReusableFish);

		for (FML_LakeFishRuntime& ExtraFish : ReusableFish)
		{
			DestroyFish(ExtraFish);
		}

		Lakes.Add(MoveTemp(NewLake));
	}

	for (AML_Tile* Tile : CachedTiles)
	{
		if (!IsWaterTile(Tile)) continue;

		if (AML_TileWater* Water = GetWaterActorFromTile(Tile))
		{
			const EML_WaterState NewState =
				AliveWaterTiles.Contains(Tile)
				? EML_WaterState::Alive
				: EML_WaterState::Dead;

			Water->SetWaterState(NewState);
		}
	}

	for (FML_LakeRuntime& DeadLake : OldLakes)
	{
		DestroyLake(DeadLake);
	}

	bool bAnyFish = false;
	for (const FML_LakeRuntime& Lake : Lakes)
	{
		if (!Lake.Fish.IsEmpty())
		{
			bAnyFish = true;
			break;
		}
	}

	SetActorTickEnabled(bAnyFish);
}

void AML_LakeFishManager::BuildLakeSplines(FML_LakeRuntime& Lake)
{
	if (Lake.Tiles.Num() <= 1)
	{
		return;
	}

	TSet<AML_Tile*> LakeTileSet;
	for (AML_Tile* Tile : Lake.Tiles)
	{
		if (IsValid(Tile))
		{
			LakeTileSet.Add(Tile);
		}
	}

	TSet<AML_Tile*> Visited;

	for (AML_Tile* StartTile : Lake.Tiles)
	{
		if (!IsValid(StartTile) || Visited.Contains(StartTile))
		{
			continue;
		}

		USplineComponent* Spline = NewObject<USplineComponent>(this);
		Spline->SetupAttachment(RootComponent);
		Spline->RegisterComponent();
		Spline->SetClosedLoop(false);
		Spline->ClearSplinePoints(false);

		AML_Tile* CurrentTile = StartTile;
		AML_Tile* PreviousTile = nullptr;

		int32 PointIndex = 0;

		while (IsValid(CurrentTile))
		{
			Visited.Add(CurrentTile);

			const FVector PointLocation = CurrentTile->GetActorLocation() + FVector(0.f, 0.f, SplineZOffset);

			Spline->AddSplinePoint(PointLocation, ESplineCoordinateSpace::World, false);
			Spline->SetSplinePointType(PointIndex, ESplinePointType::CurveClamped, false);
			PointIndex++;

			AML_BoardSpawner* Board = CurrentTile->GetBoardSpawnerFromTile();
			if (!IsValid(Board))
			{
				break;
			}

			AML_Tile* NextTile = nullptr;

			for (AML_Tile* Neighbor : Board->GetNeighbors(CurrentTile))
			{
				if (!IsValid(Neighbor)) continue;
				if (Neighbor == PreviousTile) continue;
				if (!LakeTileSet.Contains(Neighbor)) continue;
				if (Visited.Contains(Neighbor)) continue;

				NextTile = Neighbor;
				break;
			}

			if (!NextTile)
			{
				break;
			}

			PreviousTile = CurrentTile;
			CurrentTile = NextTile;
		}

		if (Spline->GetNumberOfSplinePoints() >= 2)
		{
			Spline->UpdateSpline();
			Spline->SetHiddenInGame(!bDebugDrawSplines);
			Lake.Splines.Add(Spline);
		}
		else
		{
			DestroySpline(Spline);
		}
	}
}

USplineComponent* AML_LakeFishManager::CreateSplineForTilePair(AML_Tile* A, AML_Tile* B)
{
	if (!IsValid(A) || !IsValid(B)) return nullptr;

	USplineComponent* Spline = NewObject<USplineComponent>(this);
	Spline->SetupAttachment(RootComponent);
	Spline->RegisterComponent();
	Spline->SetClosedLoop(false);
	Spline->ClearSplinePoints(false);

	const FVector ALoc = A->GetActorLocation() + FVector(0.f, 0.f, SplineZOffset);
	const FVector BLoc = B->GetActorLocation() + FVector(0.f, 0.f, SplineZOffset);
	const FVector Mid = (ALoc + BLoc) * 0.5f;
	const FVector Dir = (BLoc - ALoc).GetSafeNormal();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Dir).GetSafeNormal();
	const float SideOffset = FVector::Dist(ALoc, BLoc) * 0.15f;

	Spline->AddSplinePoint(ALoc, ESplineCoordinateSpace::World, false);
	Spline->AddSplinePoint(Mid + Right * SideOffset, ESplineCoordinateSpace::World, false);
	Spline->AddSplinePoint(BLoc, ESplineCoordinateSpace::World, false);
	Spline->AddSplinePoint(Mid - Right * SideOffset, ESplineCoordinateSpace::World, false);
	Spline->UpdateSpline();

	if (bDebugDrawSplines)
	{
		Spline->SetHiddenInGame(false);
	}
	else
	{
		Spline->SetHiddenInGame(true);
	}

	return Spline;
}

void AML_LakeFishManager::ReconcileFish(FML_LakeRuntime& Lake, TArray<FML_LakeFishRuntime>& ReusableFish)
{
	const int32 TargetFish = GetTargetFishCount(Lake);
	Lake.Fish.Reserve(TargetFish);

	while (Lake.Fish.Num() < TargetFish && !ReusableFish.IsEmpty())
	{
		FML_LakeFishRuntime FishData = ReusableFish.Pop(EAllowShrinking::No);

		AssignClosestSplineWithoutTeleport(Lake, FishData);
		AssignNewFishTarget(FishData, Lake);
		Lake.Fish.Add(FishData);
	}

	while (Lake.Fish.Num() < TargetFish)
	{
		if (!FishClass || Lake.Splines.IsEmpty()) break;

		USplineComponent* Spline = PickSpline(Lake);
		if (!IsValid(Spline)) break;

		FML_LakeFishRuntime FishData;
		FishData.Spline = Spline;
		FishData.SpeedMultiplier = FMath::Max(0.05f, 1.f + FMath::FRandRange(-FishSpeedRandomRange, FishSpeedRandomRange));
		FishData.WavePhase = FMath::FRandRange(0.f, TWO_PI);
		FishData.FishZOffsetRandom = FMath::FRandRange(-FishZOffsetRandomRange, FishZOffsetRandomRange);
		FishData.bForward = FMath::RandBool();

		const FVector SpawnLocation =
			Spline->GetLocationAtDistanceAlongSpline(0.f, ESplineCoordinateSpace::World)
			+ FVector(0.f, 0.f, FishZOffset + FishData.FishZOffsetRandom);

		AActor* FishActor = GetWorld()->SpawnActor<AActor>(FishClass, SpawnLocation, FRotator::ZeroRotator);
		if (!IsValid(FishActor)) break;

		FishData.Fish = FishActor;

		PlaceFishOnSpline(FishData, true);
		AssignNewFishTarget(FishData, Lake);

		Lake.Fish.Add(FishData);
	}
}

int32 AML_LakeFishManager::GetTargetFishCount(const FML_LakeRuntime& Lake) const
{
	if (!FishClass || Lake.Tiles.Num() <= 1) return 0;

	const int32 RawCount = FMath::RoundToInt(static_cast<float>(Lake.Tiles.Num()) * FishPerWaterTile);
	return FMath::Clamp(RawCount, MinFishPerLake, MaxFishPerLake);
}

USplineComponent* AML_LakeFishManager::PickSpline(const FML_LakeRuntime& Lake) const
{
	if (Lake.Splines.IsEmpty()) return nullptr;
	return Lake.Splines[FMath::RandRange(0, Lake.Splines.Num() - 1)].Get();
}

void AML_LakeFishManager::PlaceFishOnSpline(FML_LakeFishRuntime& FishData, bool bRandomizeDistance)
{
	AActor* Fish = FishData.Fish.Get();
	USplineComponent* Spline = FishData.Spline.Get();
	if (!IsValid(Fish) || !IsValid(Spline)) return;

	const float SplineLength = Spline->GetSplineLength();
	if (SplineLength <= KINDA_SMALL_NUMBER) return;

	if (bRandomizeDistance)
	{
		FishData.Distance = FMath::FRandRange(0.f, SplineLength);
		FishData.bForward = FMath::RandBool();
	}
	else
	{
		FishData.Distance = FMath::Fmod(FishData.Distance, SplineLength);
	}

	const FVector Location = Spline->GetLocationAtDistanceAlongSpline(
		FishData.Distance,
		ESplineCoordinateSpace::World
	);

	Fish->SetActorLocation(Location + FVector(0.f, 0.f, FishZOffset + FishData.FishZOffsetRandom));

	if (bRotateFishAlongSpline)
	{
		const FVector Tangent = Spline->GetTangentAtDistanceAlongSpline(
			FishData.Distance,
			ESplineCoordinateSpace::World
		).GetSafeNormal();

		if (!Tangent.IsNearlyZero())
		{
			Fish->SetActorRotation(Tangent.Rotation());
		}
	}
}

void AML_LakeFishManager::DestroyLake(FML_LakeRuntime& Lake)
{
	for (FML_LakeFishRuntime& FishData : Lake.Fish)
	{
		DestroyFish(FishData);
	}
	Lake.Fish.Empty();

	for (USplineComponent* Spline : Lake.Splines)
	{
		DestroySpline(Spline);
	}
	Lake.Splines.Empty();

	Lake.Tiles.Empty();
}

void AML_LakeFishManager::DestroyFish(FML_LakeFishRuntime& FishData)
{
	if (AActor* Fish = FishData.Fish.Get())
	{
		Fish->Destroy();
	}

	FishData.Fish = nullptr;
	FishData.Spline = nullptr;
}

void AML_LakeFishManager::DestroySpline(USplineComponent* Spline)
{
	if (IsValid(Spline))
	{
		Spline->DestroyComponent();
	}
}

FString AML_LakeFishManager::MakeLakeKey(const TArray<AML_Tile*>& Tiles)
{
	TArray<FString> Parts;
	Parts.Reserve(Tiles.Num());

	for (AML_Tile* Tile : Tiles)
	{
		if (IsValid(Tile))
		{
			Parts.Add(FString::Printf(TEXT("%p"), Tile));
		}
	}

	Parts.Sort();
	return FString::Join(Parts, TEXT("|"));
}

bool AML_LakeFishManager::LakesOverlap(const FML_LakeRuntime& ExistingLake, const TArray<AML_Tile*>& NewTiles)
{
	for (AML_Tile* Tile : NewTiles)
	{
		if (ExistingLake.Tiles.Contains(Tile))
		{
			return true;
		}
	}

	return false;
}  