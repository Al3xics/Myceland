// RootGeneratorActor.cpp

#include "RootGeneratorActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

ARootGeneratorActor::ARootGeneratorActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ARootGeneratorActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearAll(false);
	Super::EndPlay(EndPlayReason);
}

FVector ARootGeneratorActor::FlattenXY(const FVector& V, float Z)
{
	return FVector(V.X, V.Y, Z);
}

FVector2D ARootGeneratorActor::SafeNormal2D(const FVector2D& V)
{
	return V.IsNearlyZero() ? FVector2D::ZeroVector : V.GetSafeNormal();
}

bool ARootGeneratorActor::GetTileEnumValue(AActor* Tile, UEnum*& OutEnum, int64& OutValue) const
{
	OutEnum = nullptr;
	OutValue = 0;
	if (!Tile) return false;

	FProperty* Prop = Tile->GetClass()->FindPropertyByName(TileTypeVarName);
	if (!Prop) return false;

	if (FEnumProperty* EP = CastField<FEnumProperty>(Prop))
	{
		OutEnum = EP->GetEnum();
		if (!OutEnum) return false;

		FNumericProperty* Under = EP->GetUnderlyingProperty();
		const void* ValuePtr = EP->ContainerPtrToValuePtr<void>(Tile);
		OutValue = Under->GetSignedIntPropertyValue(ValuePtr);
		return true;
	}

	if (FByteProperty* BP = CastField<FByteProperty>(Prop))
	{
		OutEnum = BP->Enum;
		if (!OutEnum) return false;

		const uint8* Ptr = BP->ContainerPtrToValuePtr<uint8>(Tile);
		OutValue = (int64)(*Ptr);
		return true;
	}

	return false;
}

bool ARootGeneratorActor::IsTargetTile(AActor* Tile) const
{
	UEnum* Enum = nullptr;
	int64 Val = 0;
	if (!GetTileEnumValue(Tile, Enum, Val)) return false;

	const FString Display = Enum->GetDisplayNameTextByValue(Val).ToString();
	const FString Internal = Enum->GetNameStringByValue(Val);

	return Display.Equals(TargetTileTypeDisplayName, ESearchCase::IgnoreCase) ||
		Internal.Equals(TargetTileTypeDisplayName, ESearchCase::IgnoreCase);
}

bool ARootGeneratorActor::GetNeighbors(AActor* Tile, TArray<AActor*>& OutNeighbors) const
{
	OutNeighbors.Reset();
	if (!Tile) return false;

	FProperty* Prop = Tile->GetClass()->FindPropertyByName(NeighborsVarName);
	if (!Prop) return false;

	const FArrayProperty* Arr = CastField<FArrayProperty>(Prop);
	if (!Arr) return false;

	const FObjectProperty* InnerObj = CastField<FObjectProperty>(Arr->Inner);
	if (!InnerObj) return false;

	FScriptArrayHelper Helper(Arr, Arr->ContainerPtrToValuePtr<void>(Tile));
	for (int32 i = 0; i < Helper.Num(); ++i)
	{
		UObject* Obj = InnerObj->GetObjectPropertyValue(Helper.GetRawPtr(i));
		if (AActor* A = Cast<AActor>(Obj))
		{
			OutNeighbors.Add(A);
		}
	}
	return true;
}

void ARootGeneratorActor::GenerateRoots()
{
	ClearAll(false);
	if (!GetWorld()) return;

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Tile = *It;
		if (IsTargetTile(Tile))
		{
			ActiveTiles.Add(Tile);
		}
	}

	for (TWeakObjectPtr<AActor> WT : ActiveTiles)
	{
		if (AActor* Tile = WT.Get())
		{
			AddEdgesForTile(Tile);
		}
	}
}

void ARootGeneratorActor::NotifyTileChanged(AActor* Tile)
{
	if (!Tile) return;

	const bool Was = ActiveTiles.Contains(Tile);
	const bool Is = IsTargetTile(Tile);

	if (!Was && Is)
	{
		AddTile(Tile);
		return;
	}

	if (Was && !Is)
	{
		RemoveTile(Tile, true);
		return;
	}

	if (Was && Is)
	{
		RemoveEdgesForTile(Tile, true);
		AddEdgesForTile(Tile);
	}
}

void ARootGeneratorActor::ClearAll(bool bAnimated)
{
	TArray<FTileEdgeKey> Keys;
	EdgeMap.GetKeys(Keys);

	for (const FTileEdgeKey& K : Keys)
	{
		DestroyEdge(K, bAnimated);
	}

	ActiveTiles.Empty();
}

void ARootGeneratorActor::AddTile(AActor* Tile)
{
	if (!Tile) return;
	if (!IsTargetTile(Tile)) return;
	if (ActiveTiles.Contains(Tile)) return;

	ActiveTiles.Add(Tile);
	AddEdgesForTile(Tile);
}

void ARootGeneratorActor::RemoveTile(AActor* Tile, bool bAnimated)
{
	if (!Tile) return;
	if (!ActiveTiles.Contains(Tile)) return;

	RemoveEdgesForTile(Tile, bAnimated);
	ActiveTiles.Remove(Tile);
}

void ARootGeneratorActor::AddEdgesForTile(AActor* Tile)
{
	TArray<AActor*> Neigh;
	if (!GetNeighbors(Tile, Neigh)) return;

	for (AActor* N : Neigh)
	{
		if (!N) continue;
		if (!IsTargetTile(N)) continue;

		const FTileEdgeKey Key(Tile, N);
		if (EdgeMap.Contains(Key)) continue;

		CreateEdge(Tile, N);
	}
}

void ARootGeneratorActor::RemoveEdgesForTile(AActor* Tile, bool bAnimated)
{
	TArray<FTileEdgeKey> Keys;
	EdgeMap.GetKeys(Keys);

	for (const FTileEdgeKey& K : Keys)
	{
		FEdgeRuntime* Edge = EdgeMap.Find(K);
		if (!Edge) continue;

		AActor* A = Edge->TileA.Get();
		AActor* B = Edge->TileB.Get();

		if (A == Tile || B == Tile)
		{
			DestroyEdge(K, bAnimated);
		}
	}
}

void ARootGeneratorActor::BuildCrackPoints2D(
	const FVector& Start,
	const FVector& End,
	float Z,
	int32 NumPoints,
	float Width,
	float Ondulations,
	float Phase,
	TArray<FVector>& OutPoints) const
{
	OutPoints.Reset();
	NumPoints = FMath::Max(NumPoints, 2);

	const FVector S = FlattenXY(Start, Z);
	const FVector E = FlattenXY(End, Z);

	const FVector2D Delta(E.X - S.X, E.Y - S.Y);
	if (Delta.Size() < KINDA_SMALL_NUMBER)
	{
		OutPoints.Add(S);
		OutPoints.Add(E);
		return;
	}

	const FVector2D Dir = SafeNormal2D(Delta);
	const FVector2D Perp(-Dir.Y, Dir.X);

	const float NoiseFreq1 = 6.0f;
	const float NoiseFreq2 = 13.0f;

	for (int32 i = 0; i < NumPoints; ++i)
	{
		const float t = (float)i / (float)(NumPoints - 1);
		FVector Base = FMath::Lerp(S, E, t);

		const float Envelope = FMath::Sin(PI * t);

		const float Wave =
			FMath::Sin(Phase + t * Ondulations * 2.f * PI) +
			0.6f * FMath::Sin(Phase * 1.7f + t * NoiseFreq1) +
			0.35f * FMath::Sin(Phase * 2.3f + t * NoiseFreq2);

		const float Offset = Width * Envelope * Wave;

		Base.X += Perp.X * Offset;
		Base.Y += Perp.Y * Offset;
		Base.Z = Z;

		OutPoints.Add(Base);
	}
}

void ARootGeneratorActor::FillSplineFromWorldPoints(USplineComponent* Spline, const TArray<FVector>& WorldPoints) const
{
	if (!Spline) return;

	Spline->ClearSplinePoints(false);

	// Convert world points to spline-local, then add as Local (no space mismatch)
	const FTransform ST = Spline->GetComponentTransform();
	for (const FVector& WP : WorldPoints)
	{
		const FVector LP = ST.InverseTransformPosition(WP);
		Spline->AddSplinePoint(LP, ESplineCoordinateSpace::Local, false);
	}

	Spline->UpdateSpline();
}

int32 ARootGeneratorActor::ComputeBranchCount(float MainSplineLen) const
{
	if (!bEnableBranches) return 0;
	if (BranchDensityPer1000 <= 0.0f) return 0;
	if (MainSplineLen <= KINDA_SMALL_NUMBER) return 0;

	const float Expected = (MainSplineLen / 1000.0f) * BranchDensityPer1000;
	int32 Count = FMath::FloorToInt(Expected);
	const float Frac = Expected - (float)Count;
	if (FMath::FRand() < Frac) Count++;

	if (BranchCountJitter > 0)
	{
		Count += FMath::RandRange(-BranchCountJitter, BranchCountJitter);
	}
	return FMath::Max(0, Count);
}
static uint32 HashEdge(const AActor* A, const AActor* B)
{
	// Order-independent hash
	const uint32 HA = ::GetTypeHash(A);
	const uint32 HB = ::GetTypeHash(B);
	return (HA < HB) ? HashCombine(HA, HB) : HashCombine(HB, HA);
}

FVector ARootGeneratorActor::MakeEndpointOffsetXY(const AActor* A, const AActor* B, bool bForA) const
{
	if (!bEnableEndpointOffset || EndpointOffsetRadius <= KINDA_SMALL_NUMBER) return FVector::ZeroVector;

	// Deterministic random per edge and per end
	const uint32 Seed = HashCombine(HashEdge(A, B), bForA ? 0xA1B2C3D4u : 0x4D3C2B1Au);
	FRandomStream RS((int32)Seed);

	const float Angle = RS.FRandRange(0.0f, 2.0f * PI);
	const float R = RS.FRandRange(0.0f, EndpointOffsetRadius);

	return FVector(FMath::Cos(Angle) * R, FMath::Sin(Angle) * R, 0.0f);
}

void ARootGeneratorActor::CreateEdge(AActor* A, AActor* B)
{
	if (!A || !B) return;

	const FTileEdgeKey Key(A, B);
	if (EdgeMap.Contains(Key)) return;

	FEdgeRuntime Edge;
	Edge.TileA = A;
	Edge.TileB = B;

	USplineComponent* Spline = NewObject<USplineComponent>(this, USplineComponent::StaticClass());
	Spline->SetupAttachment(GetRootComponent());
	Spline->RegisterComponent();

	Spline->SetRelativeLocation(FVector::ZeroVector);
	Spline->SetRelativeRotation(FRotator::ZeroRotator);
	Spline->SetRelativeScale3D(FVector::OneVector);

	FVector Start = A->GetActorLocation();
	FVector End = B->GetActorLocation();

	// Small stable offsets so endpoints are not always tile centers
	Start += MakeEndpointOffsetXY(A, B, true);
	End += MakeEndpointOffsetXY(A, B, false);

	const float Z = (bForce2D && bUseStartZAsFixedZ) ? Start.Z : FixedZ;

	TArray<FVector> Points;
	BuildCrackPoints2D(Start, End, Z, PointsPerEdge, CrackWidth, CrackOndulations, FMath::FRand(), Points);
	FillSplineFromWorldPoints(Spline, Points);

	Edge.Spline = Spline;

	EdgeMap.Add(Key, Edge);

	FEdgeRuntime& Stored = EdgeMap[Key];
	BuildSplineMeshesHidden(Stored);

	if (GrowStepSeconds <= 0.0f)
	{
		for (USplineMeshComponent* M : Stored.Meshes) if (M) M->SetVisibility(true);
		return;
	}

	Stored.GrowIndex = 0;
	GetWorldTimerManager().SetTimer(
		Stored.GrowTimer,
		FTimerDelegate::CreateUObject(this, &ARootGeneratorActor::TickGrow, Key),
		GrowStepSeconds,
		true
	);
}

void ARootGeneratorActor::DestroyEdge(const FTileEdgeKey& Key, bool bAnimated)
{
	FEdgeRuntime* Edge = EdgeMap.Find(Key);
	if (!Edge) return;
	if (Edge->bDestroying) return;

	Edge->bDestroying = true;
	GetWorldTimerManager().ClearTimer(Edge->GrowTimer);

	if (!bAnimated || UngrowStepSeconds <= 0.0f)
	{
		for (USplineMeshComponent* M : Edge->Meshes) if (M) M->DestroyComponent();
		Edge->Meshes.Empty();

		for (USplineComponent* BS : Edge->BranchSplines) if (BS) BS->DestroyComponent();
		Edge->BranchSplines.Empty();

		if (Edge->Spline) Edge->Spline->DestroyComponent();
		EdgeMap.Remove(Key);
		return;
	}

	Edge->UngrowIndex = Edge->Meshes.Num() - 1;
	GetWorldTimerManager().SetTimer(
		Edge->UngrowTimer,
		FTimerDelegate::CreateUObject(this, &ARootGeneratorActor::TickUngrow, Key),
		UngrowStepSeconds,
		true
	);
}

void ARootGeneratorActor::TickGrow(FTileEdgeKey Key)
{
	FEdgeRuntime* Edge = EdgeMap.Find(Key);
	if (!Edge || Edge->bDestroying) return;

	if (Edge->GrowIndex >= Edge->Meshes.Num())
	{
		GetWorldTimerManager().ClearTimer(Edge->GrowTimer);
		return;
	}

	if (USplineMeshComponent* M = Edge->Meshes[Edge->GrowIndex].Get())
	{
		M->SetVisibility(true);
	}
	Edge->GrowIndex++;
}

void ARootGeneratorActor::TickUngrow(FTileEdgeKey Key)
{
	FEdgeRuntime* Edge = EdgeMap.Find(Key);
	if (!Edge) return;

	if (Edge->UngrowIndex < 0)
	{
		GetWorldTimerManager().ClearTimer(Edge->UngrowTimer);

		for (USplineComponent* BS : Edge->BranchSplines) if (BS) BS->DestroyComponent();
		Edge->BranchSplines.Empty();

		if (Edge->Spline) Edge->Spline->DestroyComponent();
		EdgeMap.Remove(Key);
		return;
	}

	if (Edge->Meshes.IsValidIndex(Edge->UngrowIndex))
	{
		if (USplineMeshComponent* M = Edge->Meshes[Edge->UngrowIndex].Get())
		{
			M->DestroyComponent();
		}
	}
	Edge->UngrowIndex--;
}

void ARootGeneratorActor::BuildSplineMeshesHidden(FEdgeRuntime& Edge)
{
	Edge.Meshes.Empty();
	for (USplineComponent* BS : Edge.BranchSplines) if (BS) BS->DestroyComponent();
	Edge.BranchSplines.Empty();

	if (!Edge.Spline || !SegmentMesh) return;

	const float MainSplineLen = Edge.Spline->GetSplineLength();
	if (MainSplineLen < KINDA_SMALL_NUMBER) return;

	// Main spline meshes (ALL LOCAL)
	{
		const float SegLen = FMath::Max(1.0f, SegmentLength);
		const int32 SegCount = FMath::Max(1, FMath::CeilToInt(MainSplineLen / SegLen));
		const float Step = MainSplineLen / (float)SegCount;

		for (int32 i = 0; i < SegCount; ++i)
		{
			const float D0 = i * Step;
			const float D1 = (i + 1) * Step;

			const FVector P0 = Edge.Spline->GetLocationAtDistanceAlongSpline(D0, ESplineCoordinateSpace::Local);
			const FVector P1 = Edge.Spline->GetLocationAtDistanceAlongSpline(D1, ESplineCoordinateSpace::Local);

			const FVector T0 = Edge.Spline->GetTangentAtDistanceAlongSpline(D0, ESplineCoordinateSpace::Local);
			const FVector T1 = Edge.Spline->GetTangentAtDistanceAlongSpline(D1, ESplineCoordinateSpace::Local);

			USplineMeshComponent* SM = NewObject<USplineMeshComponent>(this, USplineMeshComponent::StaticClass());
			SM->SetupAttachment(Edge.Spline);
			SM->RegisterComponent();
			SM->SetMobility(EComponentMobility::Movable);

			SM->SetStaticMesh(SegmentMesh);
			if (SegmentMaterial) SM->SetMaterial(0, SegmentMaterial);

			SM->SetForwardAxis(ForwardAxis, true);

			// Pivot correction in mesh-local
			SM->SetRelativeLocation(MeshOffsetLocal);

			// Start/End in parent-local (Edge.Spline local)
			SM->SetStartAndEnd(P0, T0, P1, T1);

			SM->SetStartScale(FVector2D(MeshScaleXY, MeshScaleXY));
			SM->SetEndScale(FVector2D(MeshScaleXY, MeshScaleXY));

			SM->SetVisibility(false);

			Edge.Meshes.Add(SM);
		}
	}

	// Branch splines + meshes
	const int32 BranchCount = ComputeBranchCount(MainSplineLen);
	if (BranchCount <= 0) return;

	for (int32 bi = 0; bi < BranchCount; ++bi)
	{
		const float Margin = FMath::Clamp(BranchStartMargin, 0.0f, 0.49f);
		const float t = FMath::FRandRange(Margin, 1.0f - Margin);
		const float D = t * MainSplineLen;

		// Sample main spline in LOCAL space
		const FVector P = Edge.Spline->GetLocationAtDistanceAlongSpline(D, ESplineCoordinateSpace::Local);
		const FVector Tan3 = Edge.Spline->GetTangentAtDistanceAlongSpline(D, ESplineCoordinateSpace::Local);

		FVector2D Tan2(Tan3.X, Tan3.Y);
		Tan2 = SafeNormal2D(Tan2);
		if (Tan2.IsNearlyZero()) continue;

		const float AngleDeg = FMath::FRandRange(BranchAngleMinDeg, BranchAngleMaxDeg) * (FMath::FRand() < 0.5f ? -1.0f : 1.0f);
		const float AngleRad = FMath::DegreesToRadians(AngleDeg);

		const float c = FMath::Cos(AngleRad);
		const float s = FMath::Sin(AngleRad);

		const FVector2D Dir2(
			Tan2.X * c - Tan2.Y * s,
			Tan2.X * s + Tan2.Y * c
		);

		const float BranchLen = FMath::FRandRange(FMath::Max(0.0f, BranchLengthMin), FMath::Max(BranchLengthMin, BranchLengthMax));
		if (BranchLen <= KINDA_SMALL_NUMBER) continue;

		// Keep branches consistent with your 2D rules
		const float Z = (bForce2D && bUseStartZAsFixedZ) ? P.Z : FixedZ;

		// Start/End in LOCAL (of main spline)
		const FVector StartL = FlattenXY(P, Z);
		const FVector EndL = FlattenXY(P + FVector(Dir2.X, Dir2.Y, 0.0f) * BranchLen, Z);
		USplineComponent* BranchSpline = NewObject<USplineComponent>(this, USplineComponent::StaticClass());

		// Attach branch spline to the MAIN spline so "local" space matches
		BranchSpline->SetupAttachment(Edge.Spline);
		BranchSpline->RegisterComponent();

		BranchSpline->SetRelativeLocation(FVector::ZeroVector);
		BranchSpline->SetRelativeRotation(FRotator::ZeroRotator);
		BranchSpline->SetRelativeScale3D(FVector::OneVector);

		// Build branch points in LOCAL (of main spline), then write them directly as LOCAL
		TArray<FVector> BranchPointsLocal;
		BuildCrackPoints2D(
			StartL,
			EndL,
			Z,
			BranchPointsPerEdge,
			CrackWidth * BranchWidthScale,
			CrackOndulations * BranchOndulationsScale,
			FMath::FRand(),
			BranchPointsLocal
		);

		if (BranchPointsLocal.Num() > 0) BranchPointsLocal[0] = StartL;

		// Fill branch spline directly in LOCAL space (no world conversion)
		BranchSpline->ClearSplinePoints(false);
		for (const FVector& LP : BranchPointsLocal)
		{
			BranchSpline->AddSplinePoint(LP, ESplineCoordinateSpace::Local, false);
		}
		BranchSpline->UpdateSpline();


		Edge.BranchSplines.Add(BranchSpline);

		const float BranchSplineLen = BranchSpline->GetSplineLength();
		if (BranchSplineLen < KINDA_SMALL_NUMBER) continue;

		const float SegLen = FMath::Max(1.0f, SegmentLength);
		const int32 SegCount = FMath::Max(1, FMath::CeilToInt(BranchSplineLen / SegLen));
		const float Step = BranchSplineLen / (float)SegCount;

		for (int32 i = 0; i < SegCount; ++i)
		{
			const float D0 = i * Step;
			const float D1 = (i + 1) * Step;

			const FVector P0 = BranchSpline->GetLocationAtDistanceAlongSpline(D0, ESplineCoordinateSpace::Local);
			const FVector P1 = BranchSpline->GetLocationAtDistanceAlongSpline(D1, ESplineCoordinateSpace::Local);

			const FVector T0 = BranchSpline->GetTangentAtDistanceAlongSpline(D0, ESplineCoordinateSpace::Local);
			const FVector T1 = BranchSpline->GetTangentAtDistanceAlongSpline(D1, ESplineCoordinateSpace::Local);

			USplineMeshComponent* SM = NewObject<USplineMeshComponent>(this, USplineMeshComponent::StaticClass());
			SM->SetupAttachment(BranchSpline);
			SM->RegisterComponent();
			SM->SetMobility(EComponentMobility::Movable);

			SM->SetStaticMesh(SegmentMesh);
			if (SegmentMaterial) SM->SetMaterial(0, SegmentMaterial);

			SM->SetForwardAxis(ForwardAxis, true);

			SM->SetRelativeLocation(MeshOffsetLocal);

			SM->SetStartAndEnd(P0, T0, P1, T1);

			SM->SetStartScale(FVector2D(MeshScaleXY * BranchMeshScaleMul, MeshScaleXY * BranchMeshScaleMul));
			SM->SetEndScale(FVector2D(MeshScaleXY * BranchMeshScaleMul, MeshScaleXY * BranchMeshScaleMul));

			SM->SetVisibility(false);

			Edge.Meshes.Add(SM);
		}
	}
}
