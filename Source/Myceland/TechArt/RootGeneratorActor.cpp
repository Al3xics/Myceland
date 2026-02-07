// RootGeneratorActor.cpp  (FULL DROP-IN, includes RootDensity + Variation + SplineCurviness)

#include "RootGeneratorActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

/* ===================== CONSTRUCTOR ===================== */

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

/* ===================== BASIC HELPERS ===================== */

FVector ARootGeneratorActor::FlattenXY(const FVector& V, float Z)
{
	return FVector(V.X, V.Y, Z);
}

FVector2D ARootGeneratorActor::SafeNormal2D(const FVector2D& V)
{
	return V.IsNearlyZero() ? FVector2D::ZeroVector : V.GetSafeNormal();
}

/* ===================== VARIATION HELPERS ===================== */

static uint32 HashEdgeStable(const AActor* A, const AActor* B)
{
	const uint32 HA = ::GetTypeHash(A);
	const uint32 HB = ::GetTypeHash(B);
	return (HA < HB) ? HashCombine(HA, HB) : HashCombine(HB, HA);
}

FRandomStream ARootGeneratorActor::MakeEdgeRng(const AActor* A, const AActor* B, uint32 Salt) const
{
	if (!bDeterministicVariation)
	{
		return FRandomStream(FMath::Rand());
	}

	const uint32 Seed = HashCombine(HashEdgeStable(A, B), Salt);
	return FRandomStream((int32)Seed);
}

float ARootGeneratorActor::VaryFloat(float Base, float Var, FRandomStream& RS, float MinClamp) const
{
	if (Var <= 0.0f) return Base;
	return FMath::Max(MinClamp, Base + RS.FRandRange(-Var, Var));
}

int32 ARootGeneratorActor::VaryInt(int32 Base, float Var, FRandomStream& RS, int32 MinClamp) const
{
	if (Var <= 0.0f) return Base;
	const int32 D = RS.RandRange(-(int32)Var, (int32)Var);
	return FMath::Max(MinClamp, Base + D);
}

/* ===================== ENDPOINT OFFSET ===================== */

FVector ARootGeneratorActor::MakeEndpointOffsetXY(const AActor* A, const AActor* B, bool bForA) const
{
	if (!bEnableEndpointOffset) return FVector::ZeroVector;
	if (EndpointOffsetRadius <= KINDA_SMALL_NUMBER) return FVector::ZeroVector;

	const uint32 Salt = bForA ? 0xE0E0E0E0u : 0x1E1E1E1Eu;
	FRandomStream RS = MakeEdgeRng(A, B, Salt);

	const float Angle = RS.FRandRange(0.0f, 2.0f * PI);
	const float R = RS.FRandRange(0.0f, EndpointOffsetRadius);

	return FVector(FMath::Cos(Angle) * R, FMath::Sin(Angle) * R, 0.0f);
}

/* ===================== REFLECTION ===================== */

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

		const void* Ptr = EP->ContainerPtrToValuePtr<void>(Tile);
		OutValue = EP->GetUnderlyingProperty()->GetSignedIntPropertyValue(Ptr);
		return true;
	}

	if (FByteProperty* BP = CastField<FByteProperty>(Prop))
	{
		OutEnum = BP->Enum;
		if (!OutEnum) return false;

		const uint8* Ptr = BP->ContainerPtrToValuePtr<uint8>(Tile);
		OutValue = *Ptr;
		return true;
	}

	return false;
}


bool ARootGeneratorActor::GetNeighbors(AActor* Tile, TArray<AActor*>& OutNeighbors) const
{
	OutNeighbors.Reset();
	if (!Tile) return false;

	FProperty* Prop = Tile->GetClass()->FindPropertyByName(NeighborsVarName);
	if (!Prop) return false;

	const FArrayProperty* Arr = CastField<FArrayProperty>(Prop);
	if (!Arr) return false;

	const FObjectProperty* Inner = CastField<FObjectProperty>(Arr->Inner);
	if (!Inner) return false;

	FScriptArrayHelper Helper(Arr, Arr->ContainerPtrToValuePtr<void>(Tile));
	for (int32 i = 0; i < Helper.Num(); ++i)
	{
		UObject* Obj = Inner->GetObjectPropertyValue(Helper.GetRawPtr(i));
		if (AActor* A = Cast<AActor>(Obj))
			OutNeighbors.Add(A);
	}
	return true;
}

/* ===================== ROOT GENERATION ===================== */

void ARootGeneratorActor::GenerateRoots()
{
	ClearAll(false);
	if (!GetWorld()) return;

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		FString T;
		if (GetTileTypeName(*It, T) && IsConnectType(T))
			ActiveTiles.Add(*It);

	}

	for (TWeakObjectPtr<AActor> WT : ActiveTiles)
	{
		if (AActor* Tile = WT.Get())
			AddEdgesForTile(Tile);
	}
}

void ARootGeneratorActor::NotifyTileChanged(AActor* Tile)
{
	if (!Tile) return;

	const bool Was = ActiveTiles.Contains(Tile);
	FString TT;
	const bool Is = (GetTileTypeName(Tile, TT) && IsConnectType(TT));


	if (!Was && Is) AddTile(Tile);
	else if (Was && !Is) RemoveTile(Tile, true);
	else if (Was && Is)
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
		DestroyEdge(K, bAnimated);

	ActiveTiles.Empty();
}

void ARootGeneratorActor::AddTile(AActor* Tile)
{
	if (!Tile) return;

	FString TT;
	if (!(GetTileTypeName(Tile, TT) && IsConnectType(TT))) return;

	if (ActiveTiles.Contains(Tile)) return;

	ActiveTiles.Add(Tile);
	AddEdgesForTile(Tile);
}


void ARootGeneratorActor::RemoveTile(AActor* Tile, bool bAnimated)
{
	if (!Tile || !ActiveTiles.Contains(Tile)) return;
	RemoveEdgesForTile(Tile, bAnimated);
	ActiveTiles.Remove(Tile);
}

/* ===================== DENSITY-AWARE EDGE ADD ===================== */

void ARootGeneratorActor::AddEdgesForTile(AActor* Tile)
{
	TArray<AActor*> Neigh;
	if (!GetNeighbors(Tile, Neigh)) return;

	const float Dens = FMath::Max(0.0f, RootDensity);

	FString TileType;
	if (!GetTileTypeName(Tile, TileType) || !IsConnectType(TileType)) return;

	for (AActor* N : Neigh)
	{
		if (!N) continue;

		FString NType;
		if (!GetTileTypeName(N, NType)) continue;

		if (!CanConnectTypes(TileType, NType)) continue;

		if (Dens < 1.0f)
		{
			FRandomStream RS = MakeEdgeRng(Tile, N, 0xD00D1234u);
			if (RS.FRand() > Dens) continue;

			const FTileEdgeKey Key(Tile, N, 0);
			if (!EdgeMap.Contains(Key))
				CreateEdge(Tile, N, 0);
			continue;
		}

		const int32 Whole = FMath::FloorToInt(Dens);
		const float Frac = Dens - (float)Whole;

		int32 Count = Whole;
		{
			FRandomStream RS = MakeEdgeRng(Tile, N, 0xD00D5678u);
			if (RS.FRand() < Frac) Count++;
		}

		for (int32 v = 0; v < Count; ++v)
		{
			const FTileEdgeKey Key(Tile, N, v);
			if (EdgeMap.Contains(Key)) continue;
			CreateEdge(Tile, N, v);
		}
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

		if (Edge->TileA == Tile || Edge->TileB == Tile)
			DestroyEdge(K, bAnimated);
	}
}

/* ===================== SPLINE SHAPE ===================== */
void ARootGeneratorActor::BuildCrackPoints2D(
	const FVector& Start,
	const FVector& End,
	float Z,
	int32 NumPoints,
	float Width,
	float Ondulations,
	float Phase,
	TArray<FVector>& OutPoints,
	const FVector2D* LateralDirOverride,
	bool bOneSided,
	float MinLateral
) const
{
	OutPoints.Reset();
	NumPoints = FMath::Max(NumPoints, 2);

	const FVector S = FlattenXY(Start, Z);
	const FVector E = FlattenXY(End, Z);

	const FVector2D Delta(E.X - S.X, E.Y - S.Y);
	if (Delta.Size() < KINDA_SMALL_NUMBER)
	{
		OutPoints = { S, E };
		return;
	}

	const FVector2D Dir = SafeNormal2D(Delta);

	// Lateral direction (the direction we offset points)
	FVector2D Lat = LateralDirOverride ? SafeNormal2D(*LateralDirOverride) : FVector2D(-Dir.Y, Dir.X);
	if (Lat.IsNearlyZero()) Lat = FVector2D(-Dir.Y, Dir.X);

	for (int32 i = 0; i < NumPoints; ++i)
	{
		const float t = (float)i / (float)(NumPoints - 1);
		FVector P = FMath::Lerp(S, E, t);

		const float Env = FMath::Sin(PI * t);
		const float Wave =
			FMath::Sin(Phase + t * Ondulations * 2.f * PI) +
			0.6f * FMath::Sin(Phase * 1.7f + t * 6.f) +
			0.35f * FMath::Sin(Phase * 2.3f + t * 13.f);

		float Off = Width * Env * Wave;

		// Force offsets to one side only (prevents “crossing back”)
		if (bOneSided)
			Off = FMath::Abs(Off);

		// Add a constant “away” bias (also one-sided)
		if (MinLateral > 0.0f)
			Off += MinLateral * Env;

		P.X += Lat.X * Off;
		P.Y += Lat.Y * Off;
		P.Z = Z;

		OutPoints.Add(P);
	}
}


/* ===================== SPLINE FILL + CURVINESS ===================== */

void ARootGeneratorActor::ApplyCurvinessToSpline(USplineComponent* Spline) const
{
	if (!Spline) return;

	const int32 N = Spline->GetNumberOfSplinePoints();
	if (N < 2) return;

	const float C = FMath::Clamp(SplineCurviness, 0.0f, 1.0f);

	// Fully straight
	if (C <= KINDA_SMALL_NUMBER)
	{
		for (int32 i = 0; i < N; ++i)
			Spline->SetSplinePointType(i, ESplinePointType::Linear, false);

		Spline->UpdateSpline();
		return;
	}

	// Smoothness by custom tangents, scaled by curviness
	for (int32 i = 0; i < N; ++i)
		Spline->SetSplinePointType(i, ESplinePointType::CurveCustomTangent, false);

	for (int32 i = 0; i < N; ++i)
	{
		const FVector Pm1 = (i > 0) ? Spline->GetLocationAtSplinePoint(i - 1, ESplineCoordinateSpace::Local)
			: Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
		const FVector P0 = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
		const FVector Pp1 = (i < N - 1) ? Spline->GetLocationAtSplinePoint(i + 1, ESplineCoordinateSpace::Local)
			: Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);

		// Classic Catmull-Rom style tangent, then scale by C
		FVector T = (Pp1 - Pm1) * 0.5f * C;

		// Keep Z tangent 0 if you force 2D
		if (bForce2D)
			T.Z = 0.0f;

		Spline->SetTangentsAtSplinePoint(i, T, T, ESplineCoordinateSpace::Local, false);
	}

	Spline->UpdateSpline();
}

void ARootGeneratorActor::FillSplineFromWorldPoints(USplineComponent* Spline, const TArray<FVector>& WorldPoints) const
{
	if (!Spline) return;

	Spline->ClearSplinePoints(false);
	const FTransform ST = Spline->GetComponentTransform();

	for (const FVector& WP : WorldPoints)
	{
		const FVector LP = ST.InverseTransformPosition(WP);
		Spline->AddSplinePoint(LP, ESplineCoordinateSpace::Local, false);
	}

	ApplyCurvinessToSpline(Spline);
}

/* ===================== EDGE CREATION ===================== */

void ARootGeneratorActor::CreateEdge(AActor* A, AActor* B, int32 Variant)
{
	if (!A || !B) return;

	const FTileEdgeKey Key(A, B, Variant);
	if (EdgeMap.Contains(Key)) return;

	FEdgeRuntime Edge;
	Edge.TileA = A;
	Edge.TileB = B;

	USplineComponent* Spline = NewObject<USplineComponent>(this);
	Spline->SetupAttachment(GetRootComponent());
	Spline->RegisterComponent();

	FVector Start = A->GetActorLocation();
	FVector End = B->GetActorLocation();

	Start += MakeEndpointOffsetXY(A, B, true);
	End += MakeEndpointOffsetXY(A, B, false);

	const float Z = (bForce2D && bUseStartZAsFixedZ) ? Start.Z : FixedZ;

	FRandomStream RS = MakeEdgeRng(A, B, HashCombine(0xABC123u, (uint32)Variant));

	const int32 Pts = VaryInt(PointsPerEdge, Variation, RS, 2);
	const float W = VaryFloat(CrackWidth, Variation, RS);
	const float O = VaryFloat(CrackOndulations, Variation, RS);
	const float Ph = RS.FRandRange(0.f, 2.f * PI);

	TArray<FVector> Points;
	BuildCrackPoints2D(Start, End, Z, Pts, W, O, Ph, Points, nullptr, false, 0.0f);
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

/* ===================== BRANCH COUNT ===================== */

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
		Count += FMath::RandRange(-BranchCountJitter, BranchCountJitter);

	return FMath::Max(0, Count);
}

/* ===================== MESH + BRANCHES ===================== */

void ARootGeneratorActor::BuildSplineMeshesHidden(FEdgeRuntime& Edge)
{
	Edge.Meshes.Empty();
	for (USplineComponent* BS : Edge.BranchSplines)
		if (BS) BS->DestroyComponent();
	Edge.BranchSplines.Empty();

	if (!Edge.Spline || !SegmentMesh) return;

	const float MainLen = Edge.Spline->GetSplineLength();
	if (MainLen < KINDA_SMALL_NUMBER) return;

	FRandomStream RS = MakeEdgeRng(Edge.TileA.Get(), Edge.TileB.Get(), 0xBEEF);

	const float SegLen = FMath::Max(1.f, VaryFloat(SegmentLength, Variation, RS, 1.f));
	const int32 SegCount = FMath::CeilToInt(MainLen / SegLen);
	const float Step = MainLen / SegCount;

	for (int32 i = 0; i < SegCount; ++i)
	{
		const float D0 = i * Step;
		const float D1 = (i + 1) * Step;

		const FVector P0 = Edge.Spline->GetLocationAtDistanceAlongSpline(D0, ESplineCoordinateSpace::Local);
		const FVector P1 = Edge.Spline->GetLocationAtDistanceAlongSpline(D1, ESplineCoordinateSpace::Local);
		const FVector T0 = Edge.Spline->GetTangentAtDistanceAlongSpline(D0, ESplineCoordinateSpace::Local);
		const FVector T1 = Edge.Spline->GetTangentAtDistanceAlongSpline(D1, ESplineCoordinateSpace::Local);

		USplineMeshComponent* SM = NewObject<USplineMeshComponent>(this);
		SM->SetupAttachment(Edge.Spline);
		SM->RegisterComponent();
		SM->SetMobility(EComponentMobility::Movable);

		SM->SetStaticMesh(SegmentMesh);
		if (SegmentMaterial) SM->SetMaterial(0, SegmentMaterial);

		SM->SetForwardAxis(ForwardAxis, true);
		SM->SetRelativeLocation(MeshOffsetLocal);
		SM->SetStartAndEnd(P0, T0, P1, T1);
		SM->SetStartScale(FVector2D(MeshScaleXY));
		SM->SetEndScale(FVector2D(MeshScaleXY));
		SM->SetVisibility(false);

		Edge.Meshes.Add(SM);
	}

	const int32 BranchCount = ComputeBranchCount(MainLen);
	if (BranchCount <= 0) return;

	for (int32 bi = 0; bi < BranchCount; ++bi)
	{
		FRandomStream RSB = MakeEdgeRng(Edge.TileA.Get(), Edge.TileB.Get(), 0xCAFE000u + (uint32)bi);

		const float Margin = FMath::Clamp(BranchStartMargin, 0.0f, 0.49f);
		const float t = RSB.FRandRange(Margin, 1.0f - Margin);
		const float D = t * MainLen;

		const FVector P = Edge.Spline->GetLocationAtDistanceAlongSpline(D, ESplineCoordinateSpace::World);
		const FVector T = Edge.Spline->GetTangentAtDistanceAlongSpline(D, ESplineCoordinateSpace::World);

		FVector2D Dir(T.X, T.Y);
		Dir = SafeNormal2D(Dir);
		if (Dir.IsNearlyZero()) continue;

		const float Angle = FMath::DegreesToRadians(
			RSB.FRandRange(BranchAngleMinDeg, BranchAngleMaxDeg) *
			(RSB.FRand() < 0.5f ? -1.f : 1.f)
		);

		const float c = FMath::Cos(Angle);
		const float s = FMath::Sin(Angle);

		Dir = FVector2D(
			Dir.X * c - Dir.Y * s,
			Dir.X * s + Dir.Y * c
		);

		const float Len = RSB.FRandRange(FMath::Max(0.0f, BranchLengthMin), FMath::Max(BranchLengthMin, BranchLengthMax));
		if (Len <= KINDA_SMALL_NUMBER) continue;

		// World tangent at spawn on main spline
		const FVector MainT = Edge.Spline->GetTangentAtDistanceAlongSpline(D, ESplineCoordinateSpace::World);
		FVector2D MainDir(MainT.X, MainT.Y);
		MainDir = SafeNormal2D(MainDir);
		if (MainDir.IsNearlyZero()) continue;

		// Main “side” directions
		FVector2D MainPerp(-MainDir.Y, MainDir.X);

		// Branch direction (your rotated Dir, ensure normalized)
		FVector2D BranchDir = SafeNormal2D(Dir);
		if (BranchDir.IsNearlyZero()) continue;

		// Pick the “away” side so that away points generally along the branch direction
		FVector2D Away = MainPerp;
		if (FVector2D::DotProduct(BranchDir, Away) < 0.0f)
			Away *= -1.0f;

		// Lateral direction for ondulations: perpendicular to branch direction,
		// forced to point “away”
		FVector2D BranchPerp(-BranchDir.Y, BranchDir.X);
		BranchPerp = SafeNormal2D(BranchPerp);
		if (FVector2D::DotProduct(BranchPerp, Away) < 0.0f)
			BranchPerp *= -1.0f;

		const float Clearance = FMath::Max(0.0f, BranchClearance);

		const float Z = (bForce2D && bUseStartZAsFixedZ) ? P.Z : FixedZ;

		// Push branch start away from main spline to avoid immediate overlap
		const FVector S = FlattenXY(P + FVector(Away.X, Away.Y, 0.0f) * Clearance, Z);
		const FVector E = FlattenXY(S + FVector(BranchDir.X, BranchDir.Y, 0.0f) * Len, Z);

		USplineComponent* BS = NewObject<USplineComponent>(this);
		BS->SetupAttachment(Edge.Spline);
		BS->RegisterComponent();

		const int32 BP = VaryInt(BranchPointsPerEdge, Variation, RSB, 2);
		const float BW = VaryFloat(CrackWidth * BranchWidthScale, Variation, RSB, 0.0f);
		const float BO = VaryFloat(CrackOndulations * BranchOndulationsScale, Variation, RSB, 0.0f);
		const float Ph = RSB.FRandRange(0.f, 2.f * PI);

		TArray<FVector> Pts;
		BuildCrackPoints2D(
			S, E, Z,
			BP, BW, BO, Ph,
			Pts,
			&BranchPerp,
			bBranchOffsetsOneSided,
			Clearance
		);

		FillSplineFromWorldPoints(BS, Pts);


		Edge.BranchSplines.Add(BS);

		const float BranchLen = BS->GetSplineLength();
		if (BranchLen < KINDA_SMALL_NUMBER) continue;

		const float BrSegLen = FMath::Max(1.f, VaryFloat(SegmentLength, Variation, RSB, 1.f));
		const int32 BrSegCount = FMath::Max(1, FMath::CeilToInt(BranchLen / BrSegLen));
		const float BrStep = BranchLen / (float)BrSegCount;

		for (int32 si = 0; si < BrSegCount; ++si)
		{
			const float BD0 = si * BrStep;
			const float BD1 = (si + 1) * BrStep;

			const FVector BP0 = BS->GetLocationAtDistanceAlongSpline(BD0, ESplineCoordinateSpace::World);
			const FVector BP1 = BS->GetLocationAtDistanceAlongSpline(BD1, ESplineCoordinateSpace::World);
			const FVector BT0 = BS->GetTangentAtDistanceAlongSpline(BD0, ESplineCoordinateSpace::World);
			const FVector BT1 = BS->GetTangentAtDistanceAlongSpline(BD1, ESplineCoordinateSpace::World);


			USplineMeshComponent* SM = NewObject<USplineMeshComponent>(this);
			SM->SetupAttachment(BS);
			SM->RegisterComponent();
			SM->SetMobility(EComponentMobility::Movable);

			SM->SetStaticMesh(SegmentMesh);
			if (SegmentMaterial) SM->SetMaterial(0, SegmentMaterial);

			SM->SetForwardAxis(ForwardAxis, true);
			SM->SetRelativeLocation(MeshOffsetLocal);
			SM->SetStartAndEnd(BP0, BT0, BP1, BT1);

			const float Scl = MeshScaleXY * BranchMeshScaleMul;
			SM->SetStartScale(FVector2D(Scl));
			SM->SetEndScale(FVector2D(Scl));
			SM->SetVisibility(false);

			Edge.Meshes.Add(SM);
		}
	}
}

/* ===================== EDGE DESTROY ===================== */

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
static FString NormalizeTypeName(const FString& S)
{
	FString R = S;
	R.TrimStartAndEndInline();
	return R.ToLower();
}

bool ARootGeneratorActor::GetTileTypeName(AActor* Tile, FString& OutTypeName) const
{
	OutTypeName.Reset();

	UEnum* Enum = nullptr;
	int64 Val = 0;
	if (!GetTileEnumValue(Tile, Enum, Val)) return false;

	// Prefer display name, fallback to internal
	const FString Display = Enum->GetDisplayNameTextByValue(Val).ToString();
	const FString Internal = Enum->GetNameStringByValue(Val);

	OutTypeName = !Display.IsEmpty() ? Display : Internal;
	OutTypeName = NormalizeTypeName(OutTypeName);
	return !OutTypeName.IsEmpty();
}

bool ARootGeneratorActor::IsConnectType(const FString& TypeName) const
{
	if (ConnectTypes.Num() == 0) return false;

	const FString N = NormalizeTypeName(TypeName);
	for (const FString& T : ConnectTypes)
	{
		if (N == NormalizeTypeName(T)) return true;
	}
	return false;
}

bool ARootGeneratorActor::CanConnectTypes(const FString& AType, const FString& BType) const
{
	const FString A = NormalizeTypeName(AType);
	const FString B = NormalizeTypeName(BType);

	if (!IsConnectType(A) || !IsConnectType(B)) return false;

	// same type always ok
	if (A == B) return true;

	// different types
	if (!bAllowCrossTypeConnections) return false;

	// if no whitelist => allow all cross type
	if (CrossTypeWhitelist.Num() == 0) return true;

	for (const FTileTypePair& P : CrossTypeWhitelist)
	{
		const FString PA = NormalizeTypeName(P.TypeA);
		const FString PB = NormalizeTypeName(P.TypeB);

		if ((A == PA && B == PB) || (A == PB && B == PA))
			return true;
	}
	return false;
}

/* ===================== TIMERS ===================== */

void ARootGeneratorActor::TickGrow(FTileEdgeKey Key)
{
	FEdgeRuntime* Edge = EdgeMap.Find(Key);
	if (!Edge || Edge->bDestroying) return;

	if (Edge->GrowIndex >= Edge->Meshes.Num())
	{
		GetWorldTimerManager().ClearTimer(Edge->GrowTimer);
		return;
	}

	if (Edge->Meshes.IsValidIndex(Edge->GrowIndex))
	{
		if (USplineMeshComponent* M = Edge->Meshes[Edge->GrowIndex])
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
		if (USplineMeshComponent* M = Edge->Meshes[Edge->UngrowIndex])
			M->DestroyComponent();
	}
	Edge->UngrowIndex--;
} 