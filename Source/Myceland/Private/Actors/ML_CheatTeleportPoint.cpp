// Copyright Myceland Team, All Rights Reserved.

#include "Actors/ML_CheatTeleportPoint.h"

#include "Components/ArrowComponent.h"

AML_CheatTeleportPoint::AML_CheatTeleportPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	ArrowComponent = CreateDefaultSubobject<UArrowComponent>(TEXT("ArrowComponent"));
	SetRootComponent(ArrowComponent);
	ArrowComponent->ArrowSize = 2.f;
	ArrowComponent->SetHiddenInGame(true);
}

FText AML_CheatTeleportPoint::GetDisplayLabel() const
{
	if (!DisplayName.IsEmpty())
		return DisplayName;

	return FText::FromString(GetActorNameOrLabel());
}
