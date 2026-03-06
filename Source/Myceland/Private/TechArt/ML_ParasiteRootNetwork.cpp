#include "TechArt/ML_ParasiteRootNetwork.h"

#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/ChildActorComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/KismetMathLibrary.h"
#include "TimerManager.h"
#include "Tiles/ML_BoardSpawner.h"
#include "Tiles/ML_Tile.h"
#include "Tiles/TileBase/ML_TileParasite.h"
#include "Core/ML_CoreData.h"

AML_ParasiteRootNetwork::AML_ParasiteRootNetwork()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
}

void AML_ParasiteRootNetwork::BeginPlay()
{
	Super::BeginPlay();

	if (bBuildAtBeginPlay)
	{
		if (bRebuildAllOnBeginPlay)
		{
			RebuildEntireNetwork();
		}
	}
}

void AML_ParasiteRootNetwork::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearAllRoots();
	Super::EndPlay(EndPlayReason);
}

void AML_ParasiteRootNetwork::RebuildEntireNetwork()
{
	ClearAllRoots();

	TArray<AML_Tile*> ParasiteTiles;
	UWorld* World = GetWorld();
	if (!World) return;

	for (TActorIterator<AML_Tile> It(World); It; ++It)
	{
		AML_Tile* Tile = *It;
		if (!IsValid(Tile)) continue;
		if (!IsParasiteTile(Tile)) continue;
		ParasiteTiles.Add(Tile);
	}

	TSet<AML_Tile*> TileSet;
	for (AML_Tile* Tile : ParasiteTiles)
	{
		TileSet.Add(Tile);
	}

	EnqueueConnectionsForTileSet(TileSet);
	TryBuildQueuedConnections();
}

void AML_ParasiteRootNetwork::ClearAllRoots()
{
	GetWorldTimerManager().ClearTimer(BuildQueueTimerHandle);
	GetWorldTimerManager().ClearTimer(SegmentRevealTimerHandle);

	for (TPair<FGuid, FML_ParasiteConnectionRecord>& Pair : ActiveConnections)
	{
		FML_ParasiteConnectionRecord& Record = Pair.Value;

		if (IsValid(Record.SplineComponent))
		{
			Record.SplineComponent->DestroyComponent();
		}

		for (USplineMeshComponent* Segment : Record.MeshSegments)
		{
			if (IsValid(Segment))
			{
				Segment->DestroyComponent();
			}
		}
	}

	ActiveConnections.Empty();
	TileToConnections.Empty();
	UsedExtremities.Empty();
	PendingBuildRequests.Empty();
}

void AML_ParasiteRootNetwork::RegisterParasiteActor(AActor* ParasiteActor)
{
	AML_Tile* NewTile = FindTileFromParasiteActor(ParasiteActor);
	if (!IsValid(NewTile)) return;
	if (!IsParasiteTile(NewTile)) return;

	TSet<FML_ParasiteTilePairKey> ProcessedPairs;

	for (AML_Tile* OldNeighbor : GetParasiteNeighbors(NewTile))
	{
		if (!IsParasiteTile(OldNeighbor)) continue;

		FML_ParasiteTilePairKey PairKey(OldNeighbor, NewTile);
		if (ProcessedPairs.Contains(PairKey)) continue;
		ProcessedPairs.Add(PairKey);

		TArray<USceneComponent*> ExtremitiesA = GetAvailableExtremities(OldNeighbor);
		TArray<USceneComponent*> ExtremitiesB = GetAvailableExtremities(NewTile);

		if (ExtremitiesA.IsEmpty() || ExtremitiesB.IsEmpty()) continue;

		TArray<FML_ParasiteConnectionBuildRequest> Requests;
		GatherBestConnectionsForPair(
			OldNeighbor,
			NewTile,
			ExtremitiesA,
			ExtremitiesB,
			Requests
		);

		for (const FML_ParasiteConnectionBuildRequest& Request : Requests)
		{
			PendingBuildRequests.Add(Request);

			if (Request.ExtremityA.IsValid())
			{
				MarkExtremityUsed(Request.ExtremityA.Get());
			}

			if (Request.ExtremityB.IsValid())
			{
				MarkExtremityUsed(Request.ExtremityB.Get());
			}
		}
	}

	TryBuildQueuedConnections();
}

void AML_ParasiteRootNetwork::UnregisterParasiteActor(AActor* ParasiteActor)
{
	AML_Tile* Tile = FindTileFromParasiteActor(ParasiteActor);
	if (!IsValid(Tile)) return;

	TSet<AML_Tile*> TilesToRemove;
	TilesToRemove.Add(Tile);

	RemoveConnectionsForTileSet(TilesToRemove);
}

AML_Tile* AML_ParasiteRootNetwork::FindTileFromParasiteActor(AActor* ParasiteActor) const
{
	if (!IsValid(ParasiteActor)) return nullptr;

	UWorld* World = GetWorld();
	if (!World) return nullptr;

	for (TActorIterator<AML_Tile> It(World); It; ++It)
	{
		AML_Tile* Tile = *It;
		if (!IsValid(Tile)) continue;

		UChildActorComponent* ChildComp = Tile->GetTileChildActor();
		if (!IsValid(ChildComp)) continue;

		if (ChildComp->GetChildActor() == ParasiteActor)
		{
			return Tile;
		}
	}

	return nullptr;
}

AML_TileParasite* AML_ParasiteRootNetwork::GetParasiteChildFromTile(AML_Tile* Tile) const
{
	if (!IsValid(Tile)) return nullptr;
	if (Tile->GetCurrentType() != EML_TileType::Parasite) return nullptr;

	UChildActorComponent* ChildComp = Tile->GetTileChildActor();
	if (!IsValid(ChildComp)) return nullptr;

	return Cast<AML_TileParasite>(ChildComp->GetChildActor());
}

bool AML_ParasiteRootNetwork::IsParasiteTile(AML_Tile* Tile) const
{
	return IsValid(GetParasiteChildFromTile(Tile));
}

TArray<AML_Tile*> AML_ParasiteRootNetwork::GetParasiteNeighbors(AML_Tile* Tile) const
{
	TArray<AML_Tile*> Result;
	if (!IsValid(Tile)) return Result;

	AML_BoardSpawner* Board = Tile->GetBoardSpawnerFromTile();
	if (!IsValid(Board)) return Result;

	const TArray<AML_Tile*> Neighbors = Board->GetNeighbors(Tile);
	for (AML_Tile* Neighbor : Neighbors)
	{
		if (IsParasiteTile(Neighbor))
		{
			Result.Add(Neighbor);
		}
	}

	return Result;
}

TArray<USceneComponent*> AML_ParasiteRootNetwork::GetAvailableExtremities(AML_Tile* Tile) const
{
	TArray<USceneComponent*> Result;

	AML_TileParasite* Parasite = GetParasiteChildFromTile(Tile);
	if (!IsValid(Parasite)) return Result;

	for (USceneComponent* Extremity : Parasite->GetExtremities())
	{
		if (!IsValid(Extremity)) continue;
		if (UsedExtremities.Contains(Extremity)) continue;
		Result.Add(Extremity);
	}

	return Result;
}

void AML_ParasiteRootNetwork::RebuildAroundTile(AML_Tile* ChangedTile)
{
	if (!IsValid(ChangedTile)) return;

	if (IsParasiteTile(ChangedTile))
	{
		TSet<AML_Tile*> TilesToBuild;
		TilesToBuild.Add(ChangedTile);
		EnqueueConnectionsForTileSet(TilesToBuild);
		TryBuildQueuedConnections();
	}
	else
	{
		TSet<AML_Tile*> TilesToRemove;
		TilesToRemove.Add(ChangedTile);
		RemoveConnectionsForTileSet(TilesToRemove);
	}
}

void AML_ParasiteRootNetwork::RemoveConnectionsForTileSet(const TSet<AML_Tile*>& TilesToClear)
{
	TSet<FGuid> ConnectionsToRemove;

	for (AML_Tile* Tile : TilesToClear)
	{
		const TArray<FGuid>* FoundSet = TileToConnections.Find(Tile);
		if (!FoundSet) continue;

		for (const FGuid& Id : *FoundSet)
		{
			ConnectionsToRemove.Add(Id);
		}
	}

	for (const FGuid& Id : ConnectionsToRemove)
	{
		FML_ParasiteConnectionRecord* Record = ActiveConnections.Find(Id);
		if (!Record) continue;

		if (Record->ExtremityA.IsValid())
		{
			MarkExtremityFree(Record->ExtremityA.Get());
		}

		if (Record->ExtremityB.IsValid())
		{
			MarkExtremityFree(Record->ExtremityB.Get());
		}

		if (IsValid(Record->SplineComponent))
		{
			Record->SplineComponent->DestroyComponent();
		}

		for (USplineMeshComponent* Segment : Record->MeshSegments)
		{
			if (IsValid(Segment))
			{
				Segment->DestroyComponent();
			}
		}

		if (Record->TileA.IsValid())
		{
			UnregisterConnectionOnTile(Record->TileA.Get(), Id);
		}

		if (Record->TileB.IsValid())
		{
			UnregisterConnectionOnTile(Record->TileB.Get(), Id);
		}
	}

	for (const FGuid& Id : ConnectionsToRemove)
	{
		ActiveConnections.Remove(Id);
	}
}

void AML_ParasiteRootNetwork::EnqueueConnectionsForTileSet(const TSet<AML_Tile*>& TilesToBuild)
{
	TSet<FML_ParasiteTilePairKey> ProcessedPairs;

	for (AML_Tile* Tile : TilesToBuild)
	{
		if (!IsParasiteTile(Tile)) continue;

		for (AML_Tile* Neighbor : GetParasiteNeighbors(Tile))
		{
			if (!IsParasiteTile(Neighbor)) continue;

			FML_ParasiteTilePairKey PairKey(Tile, Neighbor);
			if (ProcessedPairs.Contains(PairKey)) continue;
			ProcessedPairs.Add(PairKey);

			TArray<USceneComponent*> ExtremitiesA = GetAvailableExtremities(PairKey.TileA.Get());
			TArray<USceneComponent*> ExtremitiesB = GetAvailableExtremities(PairKey.TileB.Get());

			if (ExtremitiesA.IsEmpty() || ExtremitiesB.IsEmpty()) continue;

			TArray<FML_ParasiteConnectionBuildRequest> Requests;
			GatherBestConnectionsForPair(
				PairKey.TileA.Get(),
				PairKey.TileB.Get(),
				ExtremitiesA,
				ExtremitiesB,
				Requests
			);

			for (const FML_ParasiteConnectionBuildRequest& Request : Requests)
			{
				PendingBuildRequests.Add(Request);

				if (Request.ExtremityA.IsValid())
				{
					MarkExtremityUsed(Request.ExtremityA.Get());
				}

				if (Request.ExtremityB.IsValid())
				{
					MarkExtremityUsed(Request.ExtremityB.Get());
				}
			}
		}
	}
}

void AML_ParasiteRootNetwork::TryBuildQueuedConnections()
{
	if (PendingBuildRequests.IsEmpty()) return;

	if (DelayBetweenNewConnections <= 0.f)
	{
		while (!PendingBuildRequests.IsEmpty())
		{
			const FML_ParasiteConnectionBuildRequest Request = PendingBuildRequests[0];
			PendingBuildRequests.RemoveAt(0);
			BuildConnectionNow(Request);
		}
		return;
	}

	if (!GetWorldTimerManager().IsTimerActive(BuildQueueTimerHandle))
	{
		GetWorldTimerManager().SetTimer(
			BuildQueueTimerHandle,
			this,
			&AML_ParasiteRootNetwork::TryBuildQueuedConnections,
			DelayBetweenNewConnections,
			true
		);
	}

	if (!PendingBuildRequests.IsEmpty())
	{
		const FML_ParasiteConnectionBuildRequest Request = PendingBuildRequests[0];
		PendingBuildRequests.RemoveAt(0);
		BuildConnectionNow(Request);
	}

	if (PendingBuildRequests.IsEmpty())
	{
		GetWorldTimerManager().ClearTimer(BuildQueueTimerHandle);
	}
}

void AML_ParasiteRootNetwork::BuildConnectionNow(const FML_ParasiteConnectionBuildRequest& Request)
{
	AML_Tile* TileA = Request.TileA.Get();
	AML_Tile* TileB = Request.TileB.Get();
	USceneComponent* ExtremityA = Request.ExtremityA.Get();
	USceneComponent* ExtremityB = Request.ExtremityB.Get();

	if (!IsValid(TileA) || !IsValid(TileB) || !IsValid(ExtremityA) || !IsValid(ExtremityB))
	{
		if (IsValid(ExtremityA)) MarkExtremityFree(ExtremityA);
		if (IsValid(ExtremityB)) MarkExtremityFree(ExtremityB);
		return;
	}

	if (IsPairAlreadyConnected(TileA, TileB, ExtremityA, ExtremityB))
	{
		return;
	}

	USplineComponent* Spline = NewObject<USplineComponent>(this);
	if (!IsValid(Spline)) return;

	Spline->SetupAttachment(SceneRoot);
	Spline->RegisterComponent();
	Spline->SetMobility(EComponentMobility::Movable);
	Spline->ClearSplinePoints(false);

	const FVector Start = ExtremityA->GetComponentLocation();
	const FVector End = ExtremityB->GetComponentLocation();
	const FVector Direction = (End - Start).GetSafeNormal();

	FVector RightVector = FVector::CrossProduct(Direction, WorldUpVector).GetSafeNormal();
	if (RightVector.IsNearlyZero())
	{
		RightVector = FVector::RightVector;
	}

	const float UniquePhase = RandomSeedOffset + FMath::FRandRange(-PI, PI);

	const int32 PointCount = FMath::Max(2, SplinePointCount);
	for (int32 i = 0; i < PointCount; ++i)
	{
		const float Alpha = (PointCount == 1) ? 0.f : (static_cast<float>(i) / static_cast<float>(PointCount - 1));
		const FVector Point = ComputeSplinePoint(Start, End, RightVector, Alpha, UniquePhase);
		Spline->AddSplinePoint(Point, ESplineCoordinateSpace::World, false);
	}
	Spline->UpdateSpline();

	const int32 SegmentCount = FMath::Max(1, MeshSegmentsPerConnection);
	TArray<USplineMeshComponent*> Segments;
	Segments.Reserve(SegmentCount);

	UStaticMesh* MeshToUse = ChooseRandomMesh();

	for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
	{
		USplineMeshComponent* SplineMesh = NewObject<USplineMeshComponent>(this);
		if (!IsValid(SplineMesh)) continue;

		SplineMesh->CreationMethod = EComponentCreationMethod::Instance;
		SplineMesh->SetupAttachment(Spline);
		SplineMesh->SetMobility(EComponentMobility::Movable);
		SplineMesh->SetForwardAxis(ForwardAxis);

		if (IsValid(MeshToUse))
		{
			SplineMesh->SetStaticMesh(MeshToUse);
		}

		if (IsValid(RootMaterial))
		{
			SplineMesh->SetMaterial(0, RootMaterial);
		}

		SplineMesh->RegisterComponent();
		SplineMesh->SetVisibility(false, true);

		const float StartDistance = Spline->GetSplineLength() * (static_cast<float>(SegmentIndex) / static_cast<float>(SegmentCount));
		const float EndDistance = Spline->GetSplineLength() * (static_cast<float>(SegmentIndex + 1) / static_cast<float>(SegmentCount));

		FVector StartPos = Spline->GetLocationAtDistanceAlongSpline(
			StartDistance,
			ESplineCoordinateSpace::Local
		);

		FVector StartTangent = Spline->GetTangentAtDistanceAlongSpline(
			StartDistance,
			ESplineCoordinateSpace::Local
		);

		FVector EndPos = Spline->GetLocationAtDistanceAlongSpline(
			EndDistance,
			ESplineCoordinateSpace::Local
		);

		FVector EndTangent = Spline->GetTangentAtDistanceAlongSpline(
			EndDistance,
			ESplineCoordinateSpace::Local
		);

		SplineMesh->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent, true);
		SplineMesh->SetStartScale(MeshForwardAxisScale, true);
		SplineMesh->SetEndScale(MeshForwardAxisScale, true);

		if (bUseRoll)
		{
			const float A = static_cast<float>(SegmentIndex) / static_cast<float>(SegmentCount);
			const float B = static_cast<float>(SegmentIndex + 1) / static_cast<float>(SegmentCount);
			SplineMesh->SetStartRoll(FMath::DegreesToRadians(FMath::Lerp(StartRollDegrees, EndRollDegrees, A)), true);
			SplineMesh->SetEndRoll(FMath::DegreesToRadians(FMath::Lerp(StartRollDegrees, EndRollDegrees, B)), true);
		}

		Segments.Add(SplineMesh);
	}

	FML_ParasiteConnectionRecord Record;
	Record.ConnectionId = FGuid::NewGuid();
	Record.TileA = TileA;
	Record.TileB = TileB;
	Record.ExtremityA = ExtremityA;
	Record.ExtremityB = ExtremityB;
	Record.SplineComponent = Spline;
	Record.MeshSegments = Segments;
	Record.VisibleSegments = 0;
	Record.TotalSegments = Segments.Num();
	Record.ChosenMesh = MeshToUse;

	ActiveConnections.Add(Record.ConnectionId, Record);
	RegisterConnectionOnTile(TileA, Record.ConnectionId);
	RegisterConnectionOnTile(TileB, Record.ConnectionId);

	if (DelayBetweenSegmentReveal <= 0.f)
	{
		for (USplineMeshComponent* Segment : Segments)
		{
			if (IsValid(Segment))
			{
				Segment->SetVisibility(true, true);
			}
		}
	}
	else
	{
		if (!GetWorldTimerManager().IsTimerActive(SegmentRevealTimerHandle))
		{
			GetWorldTimerManager().SetTimer(
				SegmentRevealTimerHandle,
				this,
				&AML_ParasiteRootNetwork::RevealNextSegments,
				DelayBetweenSegmentReveal,
				true
			);
		}
	}
}

void AML_ParasiteRootNetwork::RevealNextSegments()
{
	bool bAnyPendingVisibility = false;

	for (TPair<FGuid, FML_ParasiteConnectionRecord>& Pair : ActiveConnections)
	{
		FML_ParasiteConnectionRecord& Record = Pair.Value;

		if (Record.VisibleSegments < Record.TotalSegments)
		{
			if (Record.MeshSegments.IsValidIndex(Record.VisibleSegments) && IsValid(Record.MeshSegments[Record.VisibleSegments]))
			{
				Record.MeshSegments[Record.VisibleSegments]->SetVisibility(true, true);
			}

			Record.VisibleSegments++;
		}

		if (Record.VisibleSegments < Record.TotalSegments)
		{
			bAnyPendingVisibility = true;
		}
	}

	if (!bAnyPendingVisibility)
	{
		GetWorldTimerManager().ClearTimer(SegmentRevealTimerHandle);
	}
}

bool AML_ParasiteRootNetwork::IsPairAlreadyConnected(AML_Tile* TileA, AML_Tile* TileB, USceneComponent* ExtremityA, USceneComponent* ExtremityB) const
{
	for (const TPair<FGuid, FML_ParasiteConnectionRecord>& Pair : ActiveConnections)
	{
		const FML_ParasiteConnectionRecord& Record = Pair.Value;

		const bool bSameTiles =
			(Record.TileA.Get() == TileA && Record.TileB.Get() == TileB) ||
			(Record.TileA.Get() == TileB && Record.TileB.Get() == TileA);

		if (!bSameTiles) continue;

		const bool bSameExtremities =
			(Record.ExtremityA.Get() == ExtremityA && Record.ExtremityB.Get() == ExtremityB) ||
			(Record.ExtremityA.Get() == ExtremityB && Record.ExtremityB.Get() == ExtremityA);

		if (bSameExtremities)
		{
			return true;
		}
	}

	return false;
}

int32 AML_ParasiteRootNetwork::GetDesiredConnectionCountForPair(
	const TArray<USceneComponent*>& ExtremitiesA,
	const TArray<USceneComponent*>& ExtremitiesB
) const
{
	const int32 LocalMin = FMath::Clamp(MinConnectionsPerNeighbor, 1, 3);
	const int32 LocalMax = FMath::Clamp(MaxConnectionsPerNeighbor, LocalMin, 3);
	const int32 AllowedByAvailability = FMath::Min(ExtremitiesA.Num(), ExtremitiesB.Num());

	if (AllowedByAvailability <= 0) return 0;

	const int32 RandomWanted = FMath::RandRange(LocalMin, LocalMax);
	return FMath::Clamp(RandomWanted, 1, AllowedByAvailability);
}

void AML_ParasiteRootNetwork::GatherBestConnectionsForPair(
	AML_Tile* TileA,
	AML_Tile* TileB,
	const TArray<USceneComponent*>& ExtremitiesA,
	const TArray<USceneComponent*>& ExtremitiesB,
	TArray<FML_ParasiteConnectionBuildRequest>& OutRequests
) const
{
	struct FLocalCandidate
	{
		USceneComponent* A = nullptr;
		USceneComponent* B = nullptr;
		float DistanceSq = 0.f;
	};

	TArray<FLocalCandidate> Candidates;
	for (USceneComponent* A : ExtremitiesA)
	{
		if (!IsValid(A)) continue;

		for (USceneComponent* B : ExtremitiesB)
		{
			if (!IsValid(B)) continue;
			if (IsPairAlreadyConnected(TileA, TileB, A, B)) continue;

			FLocalCandidate Candidate;
			Candidate.A = A;
			Candidate.B = B;
			Candidate.DistanceSq = FVector::DistSquared(A->GetComponentLocation(), B->GetComponentLocation());
			Candidates.Add(Candidate);
		}
	}

	Candidates.Sort([](const FLocalCandidate& Lhs, const FLocalCandidate& Rhs)
		{
			return Lhs.DistanceSq < Rhs.DistanceSq;
		});

	TSet<TWeakObjectPtr<USceneComponent>> LocallyUsedA;
	TSet<TWeakObjectPtr<USceneComponent>> LocallyUsedB;

	int32 ExistingConnectionsForPair = 0;
	for (const TPair<FGuid, FML_ParasiteConnectionRecord>& Pair : ActiveConnections)
	{
		const FML_ParasiteConnectionRecord& Record = Pair.Value;

		const bool bSameTiles =
			(Record.TileA.Get() == TileA && Record.TileB.Get() == TileB) ||
			(Record.TileA.Get() == TileB && Record.TileB.Get() == TileA);

		if (bSameTiles)
		{
			++ExistingConnectionsForPair;
		}
	}

	const int32 DesiredCount = GetDesiredConnectionCountForPair(ExtremitiesA, ExtremitiesB);
	const int32 MissingCount = FMath::Max(0, DesiredCount - ExistingConnectionsForPair);

	for (const FLocalCandidate& Candidate : Candidates)
	{
		if (OutRequests.Num() >= MissingCount) break;
		if (LocallyUsedA.Contains(Candidate.A)) continue;
		if (LocallyUsedB.Contains(Candidate.B)) continue;

		FML_ParasiteConnectionBuildRequest Request;
		Request.TileA = TileA;
		Request.TileB = TileB;
		Request.ExtremityA = Candidate.A;
		Request.ExtremityB = Candidate.B;
		OutRequests.Add(Request);

		LocallyUsedA.Add(Candidate.A);
		LocallyUsedB.Add(Candidate.B);
	}
}

UStaticMesh* AML_ParasiteRootNetwork::ChooseRandomMesh() const
{
	TArray<UStaticMesh*> ValidMeshes;
	for (UStaticMesh* Mesh : RootMeshes)
	{
		if (IsValid(Mesh))
		{
			ValidMeshes.Add(Mesh);
		}
	}

	if (ValidMeshes.IsEmpty()) return nullptr;
	return ValidMeshes[FMath::RandRange(0, ValidMeshes.Num() - 1)];
}

void AML_ParasiteRootNetwork::MarkExtremityUsed(USceneComponent* Extremity)
{
	if (IsValid(Extremity))
	{
		UsedExtremities.Add(Extremity);
	}
}

void AML_ParasiteRootNetwork::MarkExtremityFree(USceneComponent* Extremity)
{
	if (IsValid(Extremity))
	{
		UsedExtremities.Remove(Extremity);
	}
}

void AML_ParasiteRootNetwork::RegisterConnectionOnTile(AML_Tile* Tile, const FGuid& ConnectionId)
{
	if (!IsValid(Tile)) return;
	TArray<FGuid>& Array = TileToConnections.FindOrAdd(Tile);
	Array.AddUnique(ConnectionId);
}

void AML_ParasiteRootNetwork::UnregisterConnectionOnTile(AML_Tile* Tile, const FGuid& ConnectionId)
{
	if (!IsValid(Tile)) return;

	TArray<FGuid>* Found = TileToConnections.Find(Tile);
	if (!Found) return;

	Found->Remove(ConnectionId);

	if (Found->Num() == 0)
	{
		TileToConnections.Remove(Tile);
	}
}

FVector AML_ParasiteRootNetwork::ComputeSplinePoint(
	const FVector& Start,
	const FVector& End,
	const FVector& RightVector,
	float Alpha,
	float UniquePhase
) const
{
	const FVector StraightPoint = FMath::Lerp(Start, End, Alpha);
	const float CurveAmplitude = OndulationAmplitude * GetAmplitudeAlpha(Alpha);
	const float Wave = FMath::Sin((Alpha * static_cast<float>(OndulationCount) * 2.f * PI) + UniquePhase);
	const FVector Offset = RightVector * Wave * CurveAmplitude;
	return StraightPoint + Offset;
}

float AML_ParasiteRootNetwork::GetAmplitudeAlpha(float Alpha) const
{
	if (IsValid(AmplitudeAlongSplineCurve))
	{
		return AmplitudeAlongSplineCurve->GetFloatValue(Alpha);
	}

	const float Bell = FMath::Sin(Alpha * PI);
	return FMath::Max(0.f, Bell);
}