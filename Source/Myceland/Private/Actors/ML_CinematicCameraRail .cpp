#include "Actors/ML_CinematicCameraRail.h"

#include "CineCameraComponent.h"
#include "Components/SplineComponent.h"
#include "Kismet/KismetMathLibrary.h"

AML_CinematicCameraRail::AML_CinematicCameraRail()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Rail = CreateDefaultSubobject<USplineComponent>(TEXT("Rail"));
	Rail->SetupAttachment(Root);
	Rail->SetRelativeLocation(FVector(0.f, 1400.f, 1400.f));
	Rail->EditorUnselectedSplineSegmentColor = FColor::Red;

	LookAt = CreateDefaultSubobject<USplineComponent>(TEXT("LookAt"));
	LookAt->SetupAttachment(Root);
	LookAt->EditorUnselectedSplineSegmentColor = FColor::Emerald;

	Camera = CreateDefaultSubobject<UCineCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->SetWorldLocation(FVector(0.f, 1400.f, 1400.f));
	Camera->SetWorldRotation(FRotator(-45.f, -90.f, 0.f));
}

void AML_CinematicCameraRail::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const float ClampedCompletion = FMath::Clamp(PreviewCompletion, 0.f, 1.f);

	PlaceCamera(
		Rail->GetLocationAtDistanceAlongSpline(
			ClampedCompletion * Rail->GetSplineLength(),
			ESplineCoordinateSpace::World
		),
		LookAt->GetLocationAtDistanceAlongSpline(
			ClampedCompletion * LookAt->GetSplineLength(),
			ESplineCoordinateSpace::World
		)
	);
}

void AML_CinematicCameraRail::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	const float ClampedCompletion = FMath::Clamp(PreviewCompletion, 0.f, 1.f);

	PlaceCamera(
		Rail->GetLocationAtDistanceAlongSpline(
			ClampedCompletion * Rail->GetSplineLength(),
			ESplineCoordinateSpace::World
		),
		LookAt->GetLocationAtDistanceAlongSpline(
			ClampedCompletion * LookAt->GetSplineLength(),
			ESplineCoordinateSpace::World
		)
	);
}

void AML_CinematicCameraRail::SetPreviewCompletion(float NewCompletion)
{
	PreviewCompletion = FMath::Clamp(NewCompletion, 0.f, 1.f);

	const float ClampedCompletion = FMath::Clamp(PreviewCompletion, 0.f, 1.f);

	PlaceCamera(
		Rail->GetLocationAtDistanceAlongSpline(
			ClampedCompletion * Rail->GetSplineLength(),
			ESplineCoordinateSpace::World
		),
		LookAt->GetLocationAtDistanceAlongSpline(
			ClampedCompletion * LookAt->GetSplineLength(),
			ESplineCoordinateSpace::World
		)
	);
}

void AML_CinematicCameraRail::PlaceCamera(const FVector& CameraPosition, const FVector& LookAtPosition) const
{
	if (!IsValid(Camera))
	{
		return;
	}

	Camera->SetWorldLocation(CameraPosition);
	Camera->SetWorldRotation(UKismetMathLibrary::FindLookAtRotation(CameraPosition, LookAtPosition));
}

void AML_CinematicCameraRail::SyncSplines(USplineComponent* Source, USplineComponent* Destination) const
{
	if (!IsValid(Source) || !IsValid(Destination))
	{
		return;
	}

	Destination->ClearSplinePoints(false);

	const int32 PointCount = Source->GetNumberOfSplinePoints();

	for (int32 i = 0; i < PointCount; i++)
	{
		const FVector Pos = Source->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);

		Destination->AddSplinePoint(Pos, ESplineCoordinateSpace::Local, false);
		Destination->SetSplinePointType(i, Source->GetSplinePointType(i), false);

		const FVector ArriveTangent = Source->GetArriveTangentAtSplinePoint(i, ESplineCoordinateSpace::Local);
		const FVector LeaveTangent = Source->GetLeaveTangentAtSplinePoint(i, ESplineCoordinateSpace::Local);

		Destination->SetTangentsAtSplinePoint(
			i,
			ArriveTangent,
			LeaveTangent,
			ESplineCoordinateSpace::Local,
			false
		);
	}

	Destination->UpdateSpline();
}