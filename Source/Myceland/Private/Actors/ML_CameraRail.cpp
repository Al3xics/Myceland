// Copyright Myceland Team, All Rights Reserved.


#include "Actors/ML_CameraRail.h"

#include "Camera/CameraComponent.h"
#include "Components/SplineComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Player/ML_PlayerCharacter.h"
#include "Player/ML_PlayerController.h"


AML_CameraRail::AML_CameraRail()
{
	PrimaryActorTick.bCanEverTick = true;
	
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	
	Rail = CreateDefaultSubobject<USplineComponent>(TEXT("Rail"));
	Rail->SetupAttachment(Root);
	Rail->SetRelativeLocation(FVector(0.f, 1400.f, 1400.f));
	Rail->EditorUnselectedSplineSegmentColor = FColor::Red;
	
	LookAt = CreateDefaultSubobject<USplineComponent>(TEXT("LookAt"));
	LookAt->SetupAttachment(Root);
	LookAt->EditorUnselectedSplineSegmentColor = FColor::Emerald;
	
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->SetWorldLocation(FVector(0.f, 1400.f, 1400.f));
	Camera->SetWorldRotation(FRotator(-45.f, -90.f, 0.f));
}

void AML_CameraRail::BeginPlay()
{
	Super::BeginPlay();

	Player = Cast<AML_PlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));

	if (IsValid(Player))
	{
		// Initialize the CurrentCompletion of the CameraRail
		const float LookAtDist = LookAt->GetDistanceAlongSplineAtSplineInputKey(LookAt->FindInputKeyClosestToWorldLocation(Player->GetActorLocation()));
		CurrentCompletion = LookAtDist / LookAt->GetSplineLength();
	}

	// Take control of the player's view immediately
	AML_PlayerController* PC = Cast<AML_PlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	PC->SetViewTarget(this);
}

void AML_CameraRail::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!IsValid(Player)) return;

	const float LookAtDist = LookAt->GetDistanceAlongSplineAtSplineInputKey(LookAt->FindInputKeyClosestToWorldLocation(Player->GetActorLocation()));
	const float TargetCompletion = LookAtDist / LookAt->GetSplineLength();

	// Add lag to the camera by interpolating at a certain speed between the current and target completion
	if (bEnableCameraLag)
		CurrentCompletion = FMath::FInterpTo(CurrentCompletion, TargetCompletion, DeltaTime, CameraLagSpeed);
	else
		CurrentCompletion = TargetCompletion;

	PlaceCamera(
		Rail->GetLocationAtDistanceAlongSpline(CurrentCompletion * Rail->GetSplineLength(),   ESplineCoordinateSpace::World),
		LookAt->GetLocationAtDistanceAlongSpline(CurrentCompletion * LookAt->GetSplineLength(), ESplineCoordinateSpace::World)
	);
}

// Copies every spline point (position, type, tangents) from Source to Destination in local space.
// Called by the CallInEditor sync buttons — destructive, overwrites Destination permanently.
void AML_CameraRail::SyncSplines(USplineComponent* Source, USplineComponent* Destination) const
{
	Destination->ClearSplinePoints(false);

	const int32 PointCount = Source->GetNumberOfSplinePoints();
	for (int32 i = 0; i < PointCount; i++)
	{
		const FVector Pos = Source->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
		Destination->AddSplinePoint(Pos, ESplineCoordinateSpace::Local, false);
		Destination->SetSplinePointType(i, Source->GetSplinePointType(i), false);

		const FVector ArriveTangent = Source->GetArriveTangentAtSplinePoint(i, ESplineCoordinateSpace::Local);
		const FVector LeaveTangent  = Source->GetLeaveTangentAtSplinePoint(i, ESplineCoordinateSpace::Local);
		Destination->SetTangentsAtSplinePoint(i, ArriveTangent, LeaveTangent, ESplineCoordinateSpace::Local, false);
	}

	Destination->UpdateSpline();
}

void AML_CameraRail::PlaceCamera(const FVector& CameraPosition, const FVector& LookAtPosition) const
{
	Camera->SetWorldLocation(CameraPosition);
	Camera->SetWorldRotation(UKismetMathLibrary::FindLookAtRotation(CameraPosition, LookAtPosition));
}

// Editor preview: PreviewCompletion (0-1) scrubs both splines with the same completion value,
// matching exactly the logic used at runtime.
void AML_CameraRail::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	PlaceCamera(
		Rail->GetLocationAtDistanceAlongSpline(PreviewCompletion * Rail->GetSplineLength(),     ESplineCoordinateSpace::World),
		LookAt->GetLocationAtDistanceAlongSpline(PreviewCompletion * LookAt->GetSplineLength(), ESplineCoordinateSpace::World)
	);
}

