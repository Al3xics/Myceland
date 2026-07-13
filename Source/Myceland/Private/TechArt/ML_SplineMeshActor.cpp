#include "TechArt/ML_SplineMeshActor.h"

#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

AML_SplineMeshActor::AML_SplineMeshActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	Spline->SetupAttachment(Root);
	Spline->SetMobility(EComponentMobility::Movable);
	Spline->SetClosedLoop(false);
	Spline->bDrawDebug = true;
}

void AML_SplineMeshActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildMeshes();
}

void AML_SplineMeshActor::RebuildMeshes()
{
	ClearMeshes();

	if (!IsValid(Spline) || !IsValid(StaticMesh))
	{
		return;
	}

	const int32 PointCount = Spline->GetNumberOfSplinePoints();

	if (PointCount < 2)
	{
		return;
	}

	int32 SegmentCount = PointCount - 1;

	if (Spline->IsClosedLoop() && bGenerateClosedLoopSegment)
	{
		SegmentCount = PointCount;
	}

	const float SplineLength = Spline->GetSplineLength();

	for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; SegmentIndex++)
	{
		const int32 StartPointIndex = SegmentIndex;
		const int32 EndPointIndex = (SegmentIndex + 1) % PointCount;

		float StartDistance = Spline->GetDistanceAlongSplineAtSplinePoint(StartPointIndex);
		float EndDistance;

		if (EndPointIndex == 0 && Spline->IsClosedLoop())
		{
			EndDistance = SplineLength;
		}
		else
		{
			EndDistance = Spline->GetDistanceAlongSplineAtSplinePoint(EndPointIndex);
		}

		GenerateSegment(SegmentIndex, StartDistance, EndDistance);
	}
}

void AML_SplineMeshActor::ClearMeshes()
{
	for (UStaticMeshComponent* MeshComponent : GeneratedMeshes)
	{
		if (IsValid(MeshComponent))
		{
			MeshComponent->DestroyComponent();
		}
	}

	GeneratedMeshes.Reset();
}

void AML_SplineMeshActor::GenerateSegment(int32 SegmentIndex, float StartDistance, float EndDistance)
{
	const int32 MeshCount = GetMeshCountForSegment(SegmentIndex);

	if (MeshCount <= 0)
	{
		return;
	}

	const float SegmentLength = EndDistance - StartDistance;
	const float UsableLength = SegmentLength - StartPadding - EndPadding;

	if (UsableLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float PaddedStartDistance = StartDistance + StartPadding;
	const float PaddedEndDistance = EndDistance - EndPadding;

	for (int32 MeshIndex = 0; MeshIndex < MeshCount; MeshIndex++)
	{
		float Alpha;

		if (bCenterMeshesInsideSegment)
		{
			Alpha = (static_cast<float>(MeshIndex) + 0.5f) / static_cast<float>(MeshCount);
		}
		else if (MeshCount == 1)
		{
			Alpha = 0.5f;
		}
		else
		{
			Alpha = static_cast<float>(MeshIndex) / static_cast<float>(MeshCount - 1);
		}

		const float Distance = FMath::Lerp(PaddedStartDistance, PaddedEndDistance, Alpha);

		const FTransform SplineTransform = Spline->GetTransformAtDistanceAlongSpline(
			Distance,
			ESplineCoordinateSpace::Local,
			true
		);

		UStaticMeshComponent* MeshComponent = CreateMeshComponent(SegmentIndex, MeshIndex);

		if (!IsValid(MeshComponent))
		{
			continue;
		}

		FQuat FinalRotation = GetForwardAxisCorrection() * RotationOffset.Quaternion();

		if (bAlignMeshToSpline)
		{
			FinalRotation = SplineTransform.GetRotation() * FinalRotation;
		}

		FVector FinalScale = MeshScale;

		if (bUseSplinePointScale)
		{
			FinalScale *= SplineTransform.GetScale3D();
		}

		const FVector RotatedLocationOffset = FinalRotation.RotateVector(LocationOffset);
		const FVector FinalLocation = SplineTransform.GetLocation() + RotatedLocationOffset;

		MeshComponent->SetRelativeLocation(FinalLocation);
		MeshComponent->SetRelativeRotation(FinalRotation);
		MeshComponent->SetRelativeScale3D(FinalScale);
	}
}

UStaticMeshComponent* AML_SplineMeshActor::CreateMeshComponent(int32 SegmentIndex, int32 MeshIndex)
{
	if (!IsValid(Spline) || !IsValid(StaticMesh))
	{
		return nullptr;
	}

	const FName ComponentName = MakeUniqueObjectName(
		this,
		UStaticMeshComponent::StaticClass(),
		*FString::Printf(TEXT("GeneratedMesh_%03d_%03d"), SegmentIndex, MeshIndex)
	);

	UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>(
		this,
		UStaticMeshComponent::StaticClass(),
		ComponentName,
		RF_Transactional
	);

	if (!IsValid(MeshComponent))
	{
		return nullptr;
	}

	MeshComponent->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	MeshComponent->SetMobility(MeshMobility);
	MeshComponent->SetupAttachment(Spline);
	MeshComponent->SetStaticMesh(StaticMesh);
	MeshComponent->SetCastShadow(bCastShadow);
	MeshComponent->SetVisibility(bVisible);

	if (IsValid(MaterialOverride))
	{
		MeshComponent->SetMaterial(MaterialIndex, MaterialOverride);
	}

	if (bEnableCollision)
	{
		MeshComponent->SetCollisionProfileName(CollisionProfile);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	else
	{
		MeshComponent->SetCollisionProfileName(TEXT("NoCollision"));
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	MeshComponent->RegisterComponent();

	GeneratedMeshes.Add(MeshComponent);

	return MeshComponent;
}

int32 AML_SplineMeshActor::GetMeshCountForSegment(int32 SegmentIndex) const
{
	if (MeshCountPerSegment.IsValidIndex(SegmentIndex))
	{
		return FMath::Max(0, MeshCountPerSegment[SegmentIndex]);
	}

	return FMath::Max(0, DefaultMeshCountPerSegment);
}

FQuat AML_SplineMeshActor::GetForwardAxisCorrection() const
{
	switch (MeshForwardAxis)
	{
		case EMLMeshForwardAxis::Y:
			return FRotator(0.0f, -90.0f, 0.0f).Quaternion();

		case EMLMeshForwardAxis::Z:
			return FRotator(90.0f, 0.0f, 0.0f).Quaternion();

		case EMLMeshForwardAxis::X:
		default:
			return FQuat::Identity;
	}
}

int32 AML_SplineMeshActor::GetGeneratedMeshCount() const
{
	int32 Count = 0;

	for (const UStaticMeshComponent* MeshComponent : GeneratedMeshes)
	{
		if (IsValid(MeshComponent))
		{
			Count++;
		}
	}

	return Count;
}