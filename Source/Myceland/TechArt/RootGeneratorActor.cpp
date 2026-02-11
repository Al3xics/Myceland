#include "RootGeneratorActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "Materials/MaterialInstanceDynamic.h"

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

		if (bOneSided)
			Off = FMath::Abs(Off);

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

	if (C <= KINDA_SMALL_NUMBER)
	{
		for (int32 i = 0; i < N; ++i)
			Spline->SetSplinePointType(i, ESplinePointType::Linear, false);

		Spline->UpdateSpline();
		return;
	}

	for (int32 i = 0; i < N; ++i)
		Spline->SetSplinePointType(i, ESplinePointType::CurveCustomTangent, false);

	for (int32 i = 0; i < N; ++i)
	{
		const FVector Pm1 = (i > 0) ? Spline->GetLocationAtSplinePoint(i - 1, ESplineCoordinateSpace::Local)
			: Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
		const FVector Pp1 = (i < N - 1) ? Spline->GetLocationAtSplinePoint(i + 1, ESplineCoordinateSpace::Local)
			: Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);

		FVector T = (Pp1 - Pm1) * 0.5f * C;

		if (bForce2D)
			T.Z = 0.0f;

		Spline->SetTangentsAtSplinePoint(i, T, T, ESplineCoordinateSpace::Local, false);
	}

	Spline->UpdateSpline();
}

int32 ARootGeneratorActor::GetTileRootDegree(AActor* Tile) const
{
	if (!Tile) return 0;

	TSet<int32> UniqueNeighborIds;

	for (const TPair<FTileEdgeKey, FEdgeRuntime>& It : EdgeMap)
	{
		const FEdgeRuntime& E = It.Value;

		AActor* A = E.TileA.Get();
		AActor* B = E.TileB.Get();
		if (!A || !B) continue;

		if (A == Tile) UniqueNeighborIds.Add(B->GetUniqueID());
		else if (B == Tile) UniqueNeighborIds.Add(A->GetUniqueID());
	}

	return UniqueNeighborIds.Num();
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

/* ===================== DECAL HELPERS ===================== */

UMaterialInterface* ARootGeneratorActor::PickDecalMaterial(bool bIsLast, FRandomStream& RS) const
{
	const TArray<TObjectPtr<UMaterialInterface>>& List = bIsLast ? EndDecalMaterials : LineDecalMaterials;

	if (List.Num() > 0)
	{
		const int32 Idx = RS.RandRange(0, List.Num() - 1);
		return List[Idx].Get();
	}

	if (bIsLast && LineDecalMaterials.Num() > 0)
	{
		const int32 Idx = RS.RandRange(0, LineDecalMaterials.Num() - 1);
		return LineDecalMaterials[Idx].Get();
	}

	return nullptr;
}
FRotator ARootGeneratorActor::MakeGroundDecalRotationFromTangent(const FVector& WorldTangent) const
{
	// Projection direction (down onto ground)
	const FVector Up = FVector::UpVector;
	const FVector ProjX = -Up; // decal local +X points down (projection axis)

	FVector T = WorldTangent;
	if (bDecalFlattenTangentToXY)
		T.Z = 0.0f;

	if (T.SizeSquared() < KINDA_SMALL_NUMBER)
		T = FVector(1, 0, 0);

	T.Normalize();

	// Build a basis where X=projection direction, and either Y or Z aligns to tangent.
	FVector X = ProjX;
	FVector Y, Z;

	if (DecalAlongAxis == EDecalAlongAxis::AlongY)
	{
		// Y along tangent projected on ground
		Y = (T - FVector::DotProduct(T, X) * X).GetSafeNormal();
		if (Y.IsNearlyZero()) Y = FVector(0, 1, 0);

		Z = FVector::CrossProduct(X, Y).GetSafeNormal();
	}
	else
	{
		// Z along tangent
		Z = (T - FVector::DotProduct(T, X) * X).GetSafeNormal();
		if (Z.IsNearlyZero()) Z = FVector(0, 0, 1);

		Y = FVector::CrossProduct(Z, X).GetSafeNormal();
	}

	// Orthonormalize final basis
	if (DecalAlongAxis == EDecalAlongAxis::AlongY)
	{
		Y = (Y - FVector::DotProduct(Y, X) * X).GetSafeNormal();
		Z = FVector::CrossProduct(X, Y).GetSafeNormal();
	}
	else
	{
		Z = (Z - FVector::DotProduct(Z, X) * X).GetSafeNormal();
		Y = FVector::CrossProduct(Z, X).GetSafeNormal();
	}

	const FMatrix M(X, Y, Z, FVector::ZeroVector);
	FRotator R = M.Rotator();

	// Roll tweak around projection axis (local X)
	R.Roll += DecalRollOffsetDeg;
	return R;
}

float ARootGeneratorActor::GetDecalStepLength(float LocalScaleMul) const
{
	if (DecalSegmentLength > 0.0f)
		return DecalSegmentLength;

	return FMath::Max(1.0f, DecalSize.Y * LocalScaleMul);
}

/* ===================== EDGE CREATION ===================== */
void ARootGeneratorActor::RebuildDecalsForEdgesTouching(AActor* Tile)
{
	if (!Tile) return;

	TArray<FTileEdgeKey> Keys;
	EdgeMap.GetKeys(Keys);

	for (const FTileEdgeKey& K : Keys)
	{
		FEdgeRuntime* E = EdgeMap.Find(K);
		if (!E || E->bDestroying) continue;

		AActor* A = E->TileA.Get();
		AActor* B = E->TileB.Get();
		if (!A || !B) continue;

		if (A != Tile && B != Tile) continue;

		// kill old decals/components
		for (USceneComponent* C : E->Segments)
			if (C) C->DestroyComponent();
		E->Segments.Empty();

		// rebuild segments (end decals will now be correct)
		BuildSplineSegmentsHidden(*E);

		// show immediately (keep it simple; you can re-add grow if you want)
		for (USceneComponent* C : E->Segments)
			if (C) C->SetVisibility(true);
	}
}


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
	BuildSplineSegmentsHidden(Stored);

	if (GrowStepSeconds <= 0.0f)
	{
		for (USceneComponent* C : Stored.Segments) if (C) C->SetVisibility(true);
		return;
	}

	Stored.GrowIndex = 0;
	GetWorldTimerManager().SetTimer(
		Stored.GrowTimer,
		FTimerDelegate::CreateUObject(this, &ARootGeneratorActor::TickGrow, Key),
		GrowStepSeconds,
		true
	);
	RebuildDecalsForEdgesTouching(A);
	RebuildDecalsForEdgesTouching(B);
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

/* ===================== SEGMENTS (MESH OR DECAL) + BRANCHES ===================== */
void ARootGeneratorActor::BuildSplineSegmentsHidden(FEdgeRuntime& Edge)
{
	Edge.Segments.Empty();

	for (USplineComponent* BS : Edge.BranchSplines)
		if (BS) BS->DestroyComponent();
	Edge.BranchSplines.Empty();

	if (!Edge.Spline) return;

	const float MainLen = Edge.Spline->GetSplineLength();
	if (MainLen < KINDA_SMALL_NUMBER) return;

	/* ================= MAIN ================= */

	if (bUseDecals)
	{
		const float ScaleMul = 1.0f;
		const float Step = FMath::Max(1.0f, DecalTileLength * ScaleMul);

		const bool bStartIsEnd = IsTileExtremity(Edge.TileA.Get());
		const bool bEndIsEnd = IsTileExtremity(Edge.TileB.Get());

		const int32 Slots = FMath::FloorToInt(MainLen / Step);
		if (Slots > 0)
		{
			const int32 StartReserved = bStartIsEnd ? 1 : 0;
			const int32 EndReserved = bEndIsEnd ? 1 : 0;

			const int32 FirstLineSlot = StartReserved;
			const int32 LastLineSlotExclusive = Slots - EndReserved;

			// LINE DECALS (interior slots only)
			for (int32 s = FirstLineSlot; s < LastLineSlotExclusive; ++s)
			{
				const float CenterD = (s + 0.5f) * Step;

				FRandomStream RS = MakeEdgeRng(Edge.TileA.Get(), Edge.TileB.Get(), 1000u + (uint32)s);
				UMaterialInterface* Mat = PickDecalMaterial(false, RS);
				if (!Mat) continue;

				const FVector BaseP = Edge.Spline->GetLocationAtDistanceAlongSpline(CenterD, ESplineCoordinateSpace::World);
				FVector T = Edge.Spline->GetTangentAtDistanceAlongSpline(CenterD, ESplineCoordinateSpace::World);
				T.Z = 0.0f;
				T = T.GetSafeNormal();
				if (T.IsNearlyZero()) T = FVector(1, 0, 0);

				FVector Right = FVector::CrossProduct(FVector::UpVector, T).GetSafeNormal();
				if (Right.IsNearlyZero()) Right = FVector(0, 1, 0);

				const FVector Offset =
					T * DecalLineForwardOffset +
					Right * DecalLineRightOffset +
					FVector(0, 0, DecalZOffset);

				const FVector P = BaseP + Offset;
				const FRotator Rot = MakeGroundDecalRotationFromTangent(T);


				UDecalComponent* DC = NewObject<UDecalComponent>(this);
				DC->SetupAttachment(Edge.Spline);
				DC->RegisterComponent();
				DC->SetMobility(EComponentMobility::Movable);

				DC->SetWorldLocationAndRotation(P, Rot);
				DC->DecalSize = DecalSize * ScaleMul;
				DC->SortOrder = DecalSortOrder;

				if (bSetDecalColor)
				{
					UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
					if (MID)
					{
						MID->SetVectorParameterValue(DecalColorParamName, DecalColor);
						DC->SetDecalMaterial(MID);
					}
					else DC->SetDecalMaterial(Mat);
				}
				else
				{
					DC->SetDecalMaterial(Mat);
				}

				DC->SetVisibility(false);
				Edge.Segments.Add(DC);
			}

			// START END DECAL (only if start tile is an extremity)
			if (bStartIsEnd)
			{
				const float CenterD = 0.5f * Step;

				FRandomStream RSEnd = MakeEdgeRng(Edge.TileA.Get(), Edge.TileB.Get(), 2001u);
				UMaterialInterface* EndMat = PickDecalMaterial(true, RSEnd);

				if (EndMat)
				{
					const FVector BaseP = Edge.Spline->GetLocationAtDistanceAlongSpline(CenterD, ESplineCoordinateSpace::World);
					FVector T = Edge.Spline->GetTangentAtDistanceAlongSpline(CenterD, ESplineCoordinateSpace::World);
					T.Z = 0.0f; T = T.GetSafeNormal();
					if (T.IsNearlyZero()) T = FVector(1, 0, 0);

					T *= -1.0f; 

					FVector Right = FVector::CrossProduct(FVector::UpVector, T).GetSafeNormal();
					if (Right.IsNearlyZero()) Right = FVector(0, 1, 0);

					const FVector P = BaseP + (T * DecalEndForwardOffset) + (Right * DecalEndRightOffset) + FVector(0, 0, DecalZOffset);
					const FRotator Rot = MakeGroundDecalRotationFromTangent(T);

					UDecalComponent* DC = NewObject<UDecalComponent>(this);
					DC->SetupAttachment(Edge.Spline);
					DC->RegisterComponent();
					DC->SetMobility(EComponentMobility::Movable);

					FRotator FixedRot = Rot;
					FixedRot.Yaw += 180.0f;
					DC->SetWorldLocationAndRotation(P, FixedRot);
					DC->DecalSize = DecalSize * ScaleMul;
					DC->SortOrder = DecalSortOrder;

					if (bSetDecalColor)
					{
						UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(EndMat, this);
						if (MID)
						{
							MID->SetVectorParameterValue(DecalColorParamName, DecalColor);
							DC->SetDecalMaterial(MID);
						}
						else
						{
							DC->SetDecalMaterial(EndMat);
						}
					}
					else
					{
						DC->SetDecalMaterial(EndMat);
					}

					DC->SetVisibility(false);
					Edge.Segments.Add(DC);
				}
			}

			// END END DECAL (only if end tile is an extremity)
			if (bEndIsEnd)
			{
				const float CenterD = (Slots - 0.5f) * Step;

				FRandomStream RSEnd = MakeEdgeRng(Edge.TileA.Get(), Edge.TileB.Get(), 2002u);
				UMaterialInterface* EndMat = PickDecalMaterial(true, RSEnd);

				if (EndMat)
				{
					const FVector BaseP = Edge.Spline->GetLocationAtDistanceAlongSpline(CenterD, ESplineCoordinateSpace::World);
					FVector T = Edge.Spline->GetTangentAtDistanceAlongSpline(CenterD, ESplineCoordinateSpace::World);
					T.Z = 0.0f; T = T.GetSafeNormal();
					if (T.IsNearlyZero()) T = FVector(1, 0, 0);

					FVector Right = FVector::CrossProduct(FVector::UpVector, T).GetSafeNormal();
					if (Right.IsNearlyZero()) Right = FVector(0, 1, 0);

					const FVector P = BaseP + (T * DecalEndForwardOffset) + (Right * DecalEndRightOffset) + FVector(0, 0, DecalZOffset);
					const FRotator Rot = MakeGroundDecalRotationFromTangent(T);

					UDecalComponent* DC = NewObject<UDecalComponent>(this);
					DC->SetupAttachment(Edge.Spline);
					DC->RegisterComponent();
					DC->SetMobility(EComponentMobility::Movable);

					FRotator FixedRot = Rot;
					FixedRot.Yaw += 180.0f;
					DC->SetWorldLocationAndRotation(P, FixedRot);
					DC->DecalSize = DecalSize * ScaleMul;
					DC->SortOrder = DecalSortOrder;

					if (bSetDecalColor)
					{
						UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(EndMat, this);
						if (MID)
						{
							MID->SetVectorParameterValue(DecalColorParamName, DecalColor);
							DC->SetDecalMaterial(MID);
						}
						else
						{
							DC->SetDecalMaterial(EndMat);
						}
					}
					else
					{
						DC->SetDecalMaterial(EndMat);
					}

					DC->SetVisibility(false);
					Edge.Segments.Add(DC);
				}
			}


			
		}
	}
	else
	{
		// your mesh mode stays unchanged
	}

	/* ================= BRANCHES ================= */

	const int32 BranchCount = ComputeBranchCount(MainLen);
	if (BranchCount <= 0) return;

	for (int32 bi = 0; bi < BranchCount; ++bi)
	{
		// your branch spline generation stays unchanged above this point
		// after BS spline is built and BranchLen is known:

		// -------- DECAL MODE --------

		if (bUseDecals)
		{
			const float BranchLen = Edge.BranchSplines.Last()->GetSplineLength();
			const float ScaleMul = FMath::Max(0.001f, BranchDecalScaleMul);
			const float Step = FMath::Max(1.0f, DecalTileLength * ScaleMul);
			const bool bStartIsEnd = IsTrueEndTile(Edge.TileA.Get());
			const bool bEndIsEnd = IsTrueEndTile(Edge.TileB.Get());
			const int32 Slots = FMath::FloorToInt(BranchLen / Step);
			if (Slots <= 0) continue;

			USplineComponent* BS = Edge.BranchSplines.Last();

			const int32 LineCount = FMath::Max(0, Slots - 1);

			for (int32 i = 0; i < LineCount; ++i)
			{
				const float CenterD = (i + 0.5f) * Step;

				FRandomStream RS = MakeEdgeRng(Edge.TileA.Get(), Edge.TileB.Get(), 3000 + bi * 100 + i);
				UMaterialInterface* Mat = PickDecalMaterial(false, RS);
				if (!Mat) continue;

				const FVector BaseP = BS->GetLocationAtDistanceAlongSpline(CenterD, ESplineCoordinateSpace::World);
				FVector T = BS->GetTangentAtDistanceAlongSpline(CenterD, ESplineCoordinateSpace::World);
				T.Z = 0.0f;
				T = T.GetSafeNormal();
				if (T.IsNearlyZero()) T = FVector(1, 0, 0);

				FVector Right = FVector::CrossProduct(FVector::UpVector, T).GetSafeNormal();
				if (Right.IsNearlyZero()) Right = FVector(0, 1, 0);

				const FVector Offset =
					T * DecalLineForwardOffset +
					Right * DecalLineRightOffset +
					FVector(0, 0, DecalZOffset);

				const FVector P = BaseP + Offset;
				const FRotator Rot = MakeGroundDecalRotationFromTangent(T);


				UDecalComponent* DC = NewObject<UDecalComponent>(this);
				DC->SetupAttachment(BS);
				DC->RegisterComponent();
				DC->SetMobility(EComponentMobility::Movable);

				DC->SetWorldLocationAndRotation(P, Rot);
				DC->DecalSize = DecalSize * ScaleMul;
				DC->SortOrder = DecalSortOrder;

				DC->SetDecalMaterial(Mat);
				DC->SetVisibility(false);

				Edge.Segments.Add(DC);
			}

			// END
			const float EndCenter = (Slots - 0.5f) * Step;

			FRandomStream RSEnd = MakeEdgeRng(Edge.TileA.Get(), Edge.TileB.Get(), 3999 + bi);
			UMaterialInterface* EndMat = PickDecalMaterial(true, RSEnd);

			if (EndMat)
			{
				const FVector BaseP = BS->GetLocationAtDistanceAlongSpline(EndCenter, ESplineCoordinateSpace::World);
				FVector T = BS->GetTangentAtDistanceAlongSpline(EndCenter, ESplineCoordinateSpace::World);
				T.Z = 0.0f;
				T = T.GetSafeNormal();
				if (T.IsNearlyZero()) T = FVector(1, 0, 0);

				FVector Right = FVector::CrossProduct(FVector::UpVector, T).GetSafeNormal();
				if (Right.IsNearlyZero()) Right = FVector(0, 1, 0);

				const FVector Offset =
					T * DecalEndForwardOffset +
					Right * DecalEndRightOffset +
					FVector(0, 0, DecalZOffset);

				const FVector P = BaseP + Offset;
				const FRotator Rot = MakeGroundDecalRotationFromTangent(T);


				UDecalComponent* DC = NewObject<UDecalComponent>(this);
				DC->SetupAttachment(BS);
				DC->RegisterComponent();
				DC->SetMobility(EComponentMobility::Movable);

				DC->SetWorldLocationAndRotation(P, Rot);
				DC->DecalSize = DecalSize * ScaleMul;
				DC->SortOrder = DecalSortOrder;

				DC->SetDecalMaterial(EndMat);
				DC->SetVisibility(false);

				Edge.Segments.Add(DC);
			}
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
		AActor* A = Edge->TileA.Get();
		AActor* B = Edge->TileB.Get();

		for (USceneComponent* C : Edge->Segments) if (C) C->DestroyComponent();
		Edge->Segments.Empty();

		for (USplineComponent* BS : Edge->BranchSplines) if (BS) BS->DestroyComponent();
		Edge->BranchSplines.Empty();

		if (Edge->Spline) Edge->Spline->DestroyComponent();
		EdgeMap.Remove(Key);

		RebuildDecalsForEdgesTouching(A);
		RebuildDecalsForEdgesTouching(B);
		return;
	}


	Edge->UngrowIndex = Edge->Segments.Num() - 1;
	GetWorldTimerManager().SetTimer(
		Edge->UngrowTimer,
		FTimerDelegate::CreateUObject(this, &ARootGeneratorActor::TickUngrow, Key),
		UngrowStepSeconds,
		true
	);
}

/* ===================== TYPE NAME FILTERING ===================== */

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

	if (A == B) return true;

	if (!bAllowCrossTypeConnections) return false;

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

	if (Edge->GrowIndex >= Edge->Segments.Num())
	{
		GetWorldTimerManager().ClearTimer(Edge->GrowTimer);
		return;
	}

	if (Edge->Segments.IsValidIndex(Edge->GrowIndex))
	{
		if (USceneComponent* C = Edge->Segments[Edge->GrowIndex])
			C->SetVisibility(true);
	}
	Edge->GrowIndex++;
}
bool ARootGeneratorActor::IsTrueEndTile(AActor* Tile) const
{
	return GetTileRootDegree(Tile) == 1;
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

	if (Edge->Segments.IsValidIndex(Edge->UngrowIndex))
	{
		if (USceneComponent* C = Edge->Segments[Edge->UngrowIndex])
			C->DestroyComponent();
	}
	Edge->UngrowIndex--;
}
