// Copyright Myceland Team, All Rights Reserved.

#include "Component/ML_NavigationBridgeComponent.h"

#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Player/ML_HexPathfinder.h"
#include "Player/ML_PlayerCharacter.h"
#include "Player/ML_PlayerController.h"
#include "Tiles/ML_BoardSpawner.h"
#include "Tiles/ML_Tile.h"

void UML_NavigationBridgeComponent::Initialize(AML_PlayerController* Controller, AML_PlayerCharacter* Character, float InNavMeshAcceptanceRadius)
{
	OwningController = Controller;
	PlayerCharacter = Character;
	NavMeshAcceptanceRadius = InNavMeshAcceptanceRadius;
}

void UML_NavigationBridgeComponent::StartNavMeshMovement(const FVector& WorldLocation)
{
	if (!IsValid(OwningController) || !IsValid(PlayerCharacter))
		return;

	UAIBlueprintHelperLibrary::SimpleMoveToLocation(OwningController, WorldLocation);

	PendingFreeMovementTarget = WorldLocation;
	bHasFreeMovementTarget = true;
	bIsUsingNavMeshMovement = true;
}

void UML_NavigationBridgeComponent::StopNavMeshMovement()
{
	if (!bIsUsingNavMeshMovement)
		return;

	if (IsValid(PlayerCharacter))
	{
		if (UCharacterMovementComponent* MovementComponent = PlayerCharacter->GetCharacterMovement())
		{
			MovementComponent->StopMovementImmediately();
		}
	}

	bIsUsingNavMeshMovement = false;
	bHasFreeMovementTarget = false;
}

bool UML_NavigationBridgeComponent::TickNavMeshMovement(float DeltaTime)
{
	if (!bIsUsingNavMeshMovement || !bHasFreeMovementTarget)
		return false;

	if (!IsValid(PlayerCharacter))
	{
		StopNavMeshMovement();
		return false;
	}

	const FVector CurrentLocation = PlayerCharacter->GetActorLocation();
	const float DistSq = FVector::DistSquared2D(CurrentLocation, PendingFreeMovementTarget);

	if (DistSq > FMath::Square(NavMeshAcceptanceRadius))
		return false;

	StopNavMeshMovement();
	return true;
}

bool UML_NavigationBridgeComponent::IsCompleteNavMeshPath(const UNavigationPath* Path, const FVector& Destination) const
{
	if (!Path || !Path->IsValid() || Path->PathPoints.Num() == 0 || Path->IsPartial())
		return false;

	const FVector LastPoint = Path->PathPoints.Last();
	return FVector::DistSquared2D(LastPoint, Destination) <= FMath::Square(NavMeshAcceptanceRadius);
}

AML_Tile* UML_NavigationBridgeComponent::PredictNavMeshEntryTile(const AML_BoardSpawner* Board, const FVector& Destination) const
{
	if (!IsValid(Board) || !IsValid(PlayerCharacter))
		return nullptr;

	UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
		GetWorld(),
		PlayerCharacter->GetActorLocation(),
		Destination);

	if (!IsCompleteNavMeshPath(Path, Destination))
		return nullptr;

	const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();
	constexpr float SampleSpacing = 50.f;

	for (int32 PointIndex = 1; PointIndex < Path->PathPoints.Num(); ++PointIndex)
	{
		const FVector SegmentStart = Path->PathPoints[PointIndex - 1];
		const FVector SegmentEnd = Path->PathPoints[PointIndex];
		const float SegmentLength = FVector::Dist2D(SegmentStart, SegmentEnd);
		const int32 StepCount = FMath::Max(1, FMath::CeilToInt(SegmentLength / SampleSpacing));

		for (int32 Step = 1; Step <= StepCount; ++Step)
		{
			const float Alpha = static_cast<float>(Step) / static_cast<float>(StepCount);
			const FVector SamplePoint = FMath::Lerp(SegmentStart, SegmentEnd, Alpha);
			const FIntPoint Axial = Board->WorldToAxial(SamplePoint);

			if (AML_Tile* const* TilePtr = GridMap.Find(Axial))
			{
				AML_Tile* Tile = *TilePtr;
				if (IsValid(Tile) && Tile->IsBorderTile() && UML_HexPathfinder::IsTileWalkable(Tile))
					return Tile;
			}
		}
	}

	return nullptr;
}

AML_Tile* UML_NavigationBridgeComponent::FindReachableExitBorderTile(const AML_BoardSpawner* Board, const FVector& OutsideDestination) const
{
	if (!IsValid(Board) || !IsValid(PlayerCharacter) || !IsValid(PlayerCharacter->CurrentTileOn))
		return nullptr;

	const TMap<FIntPoint, AML_Tile*> GridMap = Board->GetGridMap();
	const FIntPoint StartAxial = PlayerCharacter->CurrentTileOn->GetAxialCoord();
	if (!GridMap.Contains(StartAxial))
		return nullptr;

	float MinDistSq = FLT_MAX;
	AML_Tile* ClosestReachableBorderTile = nullptr;

	for (const TPair<FIntPoint, AML_Tile*>& Pair : GridMap)
	{
		AML_Tile* Tile = Pair.Value;
		if (!IsValid(Tile) || !Tile->IsBorderTile() || !UML_HexPathfinder::IsTileWalkable(Tile))
			continue;

		TArray<FIntPoint> BoardPath;
		if (!UML_HexPathfinder::BuildPath_AxialBFS(StartAxial, Pair.Key, GridMap, BoardPath))
			continue;

		UNavigationPath* NavPath = UNavigationSystemV1::FindPathToLocationSynchronously(
			GetWorld(),
			Tile->GetActorLocation(),
			OutsideDestination);

		if (!IsCompleteNavMeshPath(NavPath, OutsideDestination))
			continue;

		const float DistSq = FVector::DistSquared(Tile->GetActorLocation(), OutsideDestination);
		if (DistSq < MinDistSq)
		{
			MinDistSq = DistSq;
			ClosestReachableBorderTile = Tile;
		}
	}

	return ClosestReachableBorderTile;
}
