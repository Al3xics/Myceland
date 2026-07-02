#include "TechArt/SplineFilledMeshActor.h"
#include "Components/SplineComponent.h"
#include "ProceduralMeshComponent.h"

ASplineFilledMeshActor::ASplineFilledMeshActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	RootComponent = Spline;
	Spline->SetClosedLoop(true);

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Generated Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->bUseAsyncCooking = true;
}

void ASplineFilledMeshActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildMesh();
}

static void AddTriangleSafe(TArray<int32>& Triangles, int32 A, int32 B, int32 C)
{
	Triangles.Add(A);
	Triangles.Add(B);
	Triangles.Add(C);
}

void ASplineFilledMeshActor::BuildMesh()
{
	if (!Spline || !Mesh)
	{
		return;
	}

	const int32 Num = FMath::Max(8, SplineSamples);
	const int32 NumFillRings = FMath::Max(1, FillRings);
	const int32 NumBevelSegments = FMath::Max(1, BevelSegments);

	const float HalfThickness = Thickness * 0.5f;
	const float SafeBevelRadius = FMath::Clamp(BevelRadius, 0.f, HalfThickness);

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;
	TArray<FProcMeshTangent> Tangents;

	TArray<FVector> Outer;
	Outer.Reserve(Num);

	for (int32 i = 0; i < Num; i++)
	{
		const float Alpha = static_cast<float>(i) / static_cast<float>(Num);
		const float Distance = Alpha * Spline->GetSplineLength();

		FVector P = Spline->GetLocationAtDistanceAlongSpline(
			Distance,
			ESplineCoordinateSpace::Local
		);

		P.Z += ZOffset;
		Outer.Add(P);
	}

	FVector Center = FVector::ZeroVector;
	for (const FVector& P : Outer)
	{
		Center += P;
	}
	Center /= static_cast<float>(Outer.Num());

	auto AddVertex = [&](
		const FVector& Position,
		const FVector& Normal,
		const FVector2D& UV
	)
	{
		const int32 Index = Vertices.Num();

		Vertices.Add(Position);
		Normals.Add(Normal.GetSafeNormal());
		UVs.Add(UV);
		Colors.Add(FColor::White);
		Tangents.Add(FProcMeshTangent(1, 0, 0));

		return Index;
	};

	// -------------------------
	// TOP FILLED SUBDIVIDED FACE
	// -------------------------

	const int32 TopStart = Vertices.Num();

	for (int32 Ring = 0; Ring <= NumFillRings; Ring++)
	{
		const float T = static_cast<float>(Ring) / static_cast<float>(NumFillRings);

		for (int32 i = 0; i < Num; i++)
		{
			FVector P = FMath::Lerp(Outer[i], Center, T);
			P.Z += HalfThickness;

			AddVertex(
				P,
				FVector::UpVector,
				FVector2D(P.X * UVScale, P.Y * UVScale)
			);
		}
	}

	for (int32 Ring = 0; Ring < NumFillRings; Ring++)
	{
		const int32 CurrentRing = TopStart + Ring * Num;
		const int32 NextRing = TopStart + (Ring + 1) * Num;

		for (int32 i = 0; i < Num; i++)
		{
			const int32 Next = (i + 1) % Num;

			const int32 A = CurrentRing + i;
			const int32 B = CurrentRing + Next;
			const int32 C = NextRing + i;
			const int32 D = NextRing + Next;

			AddTriangleSafe(Triangles, A, C, B);
			AddTriangleSafe(Triangles, B, C, D);
		}
	}

	// -------------------------
	// BOTTOM FILLED SUBDIVIDED FACE
	// -------------------------

	const int32 BottomStart = Vertices.Num();

	for (int32 Ring = 0; Ring <= NumFillRings; Ring++)
	{
		const float T = static_cast<float>(Ring) / static_cast<float>(NumFillRings);

		for (int32 i = 0; i < Num; i++)
		{
			FVector P = FMath::Lerp(Outer[i], Center, T);
			P.Z -= HalfThickness;

			AddVertex(
				P,
				FVector::DownVector,
				FVector2D(P.X * UVScale, P.Y * UVScale)
			);
		}
	}

	for (int32 Ring = 0; Ring < NumFillRings; Ring++)
	{
		const int32 CurrentRing = BottomStart + Ring * Num;
		const int32 NextRing = BottomStart + (Ring + 1) * Num;

		for (int32 i = 0; i < Num; i++)
		{
			const int32 Next = (i + 1) % Num;

			const int32 A = CurrentRing + i;
			const int32 B = CurrentRing + Next;
			const int32 C = NextRing + i;
			const int32 D = NextRing + Next;

			AddTriangleSafe(Triangles, A, B, C);
			AddTriangleSafe(Triangles, B, D, C);
		}
	}

	// -------------------------
	// ROUNDED OUTER TORUS-LIKE EDGE
	// -------------------------

	const int32 BevelStart = Vertices.Num();

	for (int32 Segment = 0; Segment <= NumBevelSegments; Segment++)
	{
		const float T = static_cast<float>(Segment) / static_cast<float>(NumBevelSegments);
		const float Angle = FMath::Lerp(HALF_PI, -HALF_PI, T);

		const float HorizontalOffset = FMath::Cos(Angle) * SafeBevelRadius;
		const float Z = FMath::Sin(Angle) * SafeBevelRadius;

		for (int32 i = 0; i < Num; i++)
		{
			const int32 Prev = (i - 1 + Num) % Num;
			const int32 Next = (i + 1) % Num;

			const FVector Tangent = (Outer[Next] - Outer[Prev]).GetSafeNormal();
			FVector Outward = FVector::CrossProduct(Tangent, FVector::UpVector).GetSafeNormal();

			if (FVector::DotProduct(Outer[i] + Outward * 10.f - Center, Outer[i] - Center) < 0.f)
			{
				Outward *= -1.f;
			}

			FVector P = Outer[i];
			P += Outward * HorizontalOffset;
			P.Z += ZOffset + Z;

			FVector Normal = (Outward * HorizontalOffset + FVector::UpVector * Z).GetSafeNormal();

			AddVertex(
				P,
				Normal,
				FVector2D(
					static_cast<float>(i) / static_cast<float>(Num),
					T
				)
			);
		}
	}

	for (int32 Segment = 0; Segment < NumBevelSegments; Segment++)
	{
		const int32 CurrentRing = BevelStart + Segment * Num;
		const int32 NextRing = BevelStart + (Segment + 1) * Num;

		for (int32 i = 0; i < Num; i++)
		{
			const int32 Next = (i + 1) % Num;

			const int32 A = CurrentRing + i;
			const int32 B = CurrentRing + Next;
			const int32 C = NextRing + i;
			const int32 D = NextRing + Next;

			AddTriangleSafe(Triangles, A, C, B);
			AddTriangleSafe(Triangles, B, C, D);
		}
	}

	Mesh->ClearAllMeshSections();

	Mesh->CreateMeshSection(
		0,
		Vertices,
		Triangles,
		Normals,
		UVs,
		Colors,
		Tangents,
		bCreateCollision
	);

	if (Material)
	{
		Mesh->SetMaterial(0, Material);
	}
}