// RootSplineGeneratorComponent.cpp

#include "RootSplineGeneratorComponent.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"

URootSplineGeneratorComponent::URootSplineGeneratorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URootSplineGeneratorComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bDeterministic)
	{
		Rand.Initialize(Seed);
	}
	else
	{
		Rand.GenerateNewSeed();
	}
}

void URootSplineGeneratorComponent::ClearGenerated()
{
	if (MainSpline)
	{
		MainSpline->DestroyComponent();
		MainSpline = nullptr;
	}

	for (TObjectPtr<USplineComponent>& S : ExtrusionSplines)
	{
		if (S)
		{
			S->DestroyComponent();
		}
	}
	ExtrusionSplines.Empty();
}

USplineComponent* URootSplineGeneratorComponent::CreateSplineComponent(const FName& Name, USceneComponent* AttachParent)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	USplineComponent* Spline = NewObject<USplineComponent>(Owner, USplineComponent::StaticClass(), Name);
	if (!Spline)
	{
		return nullptr;
	}

	Spline->SetupAttachment(AttachParent ? AttachParent : Owner->GetRootComponent());
	Spline->RegisterComponent();

	Spline->SetClosedLoop(false);
	Spline->SetMobility(EComponentMobility::Movable);

	return Spline;
}

// =======================
// Strict 2D helpers
// =======================

FVector URootSplineGeneratorComponent::FlattenXY(const FVector& V, float Z)
{
	return FVector(V.X, V.Y, Z);
}

FVector2D URootSplineGeneratorComponent::SafeNormal2D(const FVector2D& V)
{
	const float S = V.Size();
	return (S > KINDA_SMALL_NUMBER) ? (V / S) : FVector2D::ZeroVector;
}

FVector URootSplineGeneratorComponent::MakePerp2D_FromDir2D(const FVector2D& DirUnit2D)
{
	// Perpendicular in XY plane, Z=0
	return FVector(-DirUnit2D.Y, DirUnit2D.X, 0.0f);
}

void URootSplineGeneratorComponent::BuildWavyPoints2D(
	const FVector& Start,
	const FVector& End,
	float ZPlane,
	int32 NumPoints,
	float Width,
	float Ondulations,
	float Phase,
	TArray<FVector>& OutPoints
)
{
	OutPoints.Reset();
	NumPoints = FMath::Max(NumPoints, 2);

	const FVector S = FlattenXY(Start, ZPlane);
	const FVector E = FlattenXY(End, ZPlane);

	const FVector2D Delta2D(E.X - S.X, E.Y - S.Y);
	const float Len2D = Delta2D.Size();

	// Degenerate
	if (Len2D < KINDA_SMALL_NUMBER)
	{
		OutPoints.Add(S);
		OutPoints.Add(E);
		return;
	}

	const FVector2D Dir2D = SafeNormal2D(Delta2D);
	const FVector Perp = MakePerp2D_FromDir2D(Dir2D); // unit perp in XY

	for (int32 i = 0; i < NumPoints; ++i)
	{
		const float t = (float)i / (float)(NumPoints - 1);

		// Base point on straight segment
		const FVector Base = FMath::Lerp(S, E, t);

		// Envelope keeps endpoints pinned
		const float Envelope = FMath::Sin(PI * t);

		// Smooth sine wave
		const float Wave = FMath::Sin((2.0f * PI) * Ondulations * t + Phase);

		const float Offset = Width * Envelope * Wave;

		FVector P = Base + Perp * Offset;
		P.Z = ZPlane; // HARD clamp to 2D plane

		OutPoints.Add(P);
	}
}

void URootSplineGeneratorComponent::FillSplineFromPoints(USplineComponent* Spline, const TArray<FVector>& Points)
{
	if (!Spline)
	{
		return;
	}

	Spline->ClearSplinePoints(false);

	for (const FVector& P : Points)
	{
		Spline->AddSplinePoint(P, ESplineCoordinateSpace::World, false);
	}

	Spline->UpdateSpline();
}

// =======================
// Main generation
// =======================

void URootSplineGeneratorComponent::GenerateRoot(
	const FVector& Start,
	const FVector& End,
	USplineComponent*& OutMainSpline,
	TArray<USplineComponent*>& OutExtrusionSplines
)
{
	OutMainSpline = nullptr;
	OutExtrusionSplines.Reset();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Seed
	if (bDeterministic)
	{
		Rand.Initialize(Seed);
	}
	else
	{
		Rand.GenerateNewSeed();
	}

	// Cleanup old
	ClearGenerated();

	// Attach parent
	USceneComponent* AttachParent = Owner->GetRootComponent();
	if (!AttachParent)
	{
		AttachParent = NewObject<USceneComponent>(Owner, USceneComponent::StaticClass(), TEXT("RootScene"));
		AttachParent->RegisterComponent();
		Owner->SetRootComponent(AttachParent);
	}

	// Choose Z plane
	const float ZPlane = (bForce2D && bUseStartZAsFixedZ) ? Start.Z : FixedZ;

	// -----------------------
	// Main spline
	// -----------------------
	MainSpline = CreateSplineComponent(TEXT("GeneratedRootSpline_Main"), AttachParent);
	if (!MainSpline)
	{
		return;
	}

	const float MainPhase = Rand.FRandRange(0.0f, 2.0f * PI);

	TArray<FVector> MainPoints;
	BuildWavyPoints2D(Start, End, ZPlane, MainNumPoints, MainWidth, MainOndulations, MainPhase, MainPoints);
	FillSplineFromPoints(MainSpline, MainPoints);

	OutMainSpline = MainSpline;

	// -----------------------
	// Extrusions (STRICT 2D)
	// -----------------------
	const float MainSplineLen = MainSpline->GetSplineLength();
	const int32 N = FMath::Max(ExtrudingRootsNumbers, 0);

	for (int32 k = 0; k < N; ++k)
	{
		if (MainSplineLen < KINDA_SMALL_NUMBER)
		{
			break;
		}

		const float Dist = Rand.FRandRange(0.0f, MainSplineLen);

		FVector SpawnPos = MainSpline->GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);
		SpawnPos.Z = ZPlane;

		// Get tangent in world then flatten to XY
		FVector Tangent = MainSpline->GetTangentAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);
		Tangent.Z = 0.0f;

		const FVector2D Tangent2D(Tangent.X, Tangent.Y);
		const FVector2D TanUnit2D = SafeNormal2D(Tangent2D);

		if (TanUnit2D.IsNearlyZero())
		{
			continue;
		}

		// Perp in XY
		const FVector Perp = MakePerp2D_FromDir2D(TanUnit2D);

		// Pick +Perp or -Perp (2D only)
		const float Side = (Rand.RandRange(0, 1) == 1) ? 1.0f : -1.0f;
		const FVector ExtrudeDir = Perp * Side; // Z=0 always

		const FVector ExtrudeEnd = FlattenXY(SpawnPos + ExtrudeDir * ExtrudingRootsLength, ZPlane);

		const FName SplineName(*FString::Printf(TEXT("GeneratedRootSpline_Extrude_%d"), k));
		USplineComponent* ExtrudeSpline = CreateSplineComponent(SplineName, AttachParent);
		if (!ExtrudeSpline)
		{
			continue;
		}

		const float ExtrudePhase = Rand.FRandRange(0.0f, 2.0f * PI);

		TArray<FVector> ExtrudePoints;
		BuildWavyPoints2D(
			SpawnPos,
			ExtrudeEnd,
			ZPlane,
			ExtrudeNumPoints,
			ExtrudeWidth,
			ExtrudeOndulations,
			ExtrudePhase,
			ExtrudePoints
		);

		FillSplineFromPoints(ExtrudeSpline, ExtrudePoints);

		ExtrusionSplines.Add(ExtrudeSpline);
		OutExtrusionSplines.Add(ExtrudeSpline);
	}
}
