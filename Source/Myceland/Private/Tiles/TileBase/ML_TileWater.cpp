// Copyright Myceland Team, All Rights Reserved.

#include "Tiles/TileBase/ML_TileWater.h"

// Sets default values
AML_TileWater::AML_TileWater()
{
	// Tick is enabled but starts disabled: only woken up during an active transition
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

// Called when the game starts or when spawned
void AML_TileWater::BeginPlay()
{
	Super::BeginPlay();
}

void AML_TileWater::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsTransitioning)
	{
		return;
	}

	// Move CurrentAlpha towards TargetAlpha at a constant speed, from wherever it currently is
	const float Step = DeltaTime / FMath::Max(TransitionDuration, KINDA_SMALL_NUMBER);
	CurrentAlpha = FMath::FInterpConstantTo(CurrentAlpha, TargetAlpha, DeltaTime, 1.f / TransitionDuration);

	UpdateMaterialParameters(CurrentAlpha);

	if (FMath::IsNearlyEqual(CurrentAlpha, TargetAlpha, KINDA_SMALL_NUMBER))
	{
		CurrentAlpha = TargetAlpha;
		bIsTransitioning = false;
		SetActorTickEnabled(false);
	}
}

void AML_TileWater::SetWaterState(EML_WaterState NewState)
{
	if (WaterState == NewState)
	{
		return;
	}

	WaterState = NewState;

	if (WaterMaterialRef)
	{
		const bool bToAlive = (NewState == EML_WaterState::Alive);
		WaterMaterialRef->SetVectorParameterValue(
			TEXT("CoastLineColor"),
			bToAlive ? AliveCoastLineColor : DeadCoastLineColor);
	}

	StartTransition(NewState == EML_WaterState::Alive);

	OnWaterStateChanged(NewState);
}

void AML_TileWater::StartTransition(bool bToAlive)
{
	TargetAlpha = bToAlive ? 1.f : 0.f;
	bIsTransitioning = true;
	SetActorTickEnabled(true);
}

void AML_TileWater::UpdateMaterialParameters(float Alpha)
{
	if (!WaterMaterialRef)
	{
		return;
	}

	const float CoastLineValue = FMath::Lerp(CoastLineLerpRange.X, CoastLineLerpRange.Y, Alpha);
	const float DeadToAliveValue = FMath::Lerp(DeadToAliveLerpRange.X, DeadToAliveLerpRange.Y, Alpha);

	WaterMaterialRef->SetScalarParameterValue(TEXT("CoastLine"), CoastLineValue);
	WaterMaterialRef->SetScalarParameterValue(TEXT("DeadToAlive"), DeadToAliveValue);
}