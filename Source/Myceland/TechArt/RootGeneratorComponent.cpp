#include "RootGeneratorComponent.h"

#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "Components/SplineComponent.h"

static const FName RootTag(TEXT("RootGen"));

URootGeneratorComponent::URootGeneratorComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // no animation in C++ now
}

void URootGeneratorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyRoot();
	Super::EndPlay(EndPlayReason);
}

void URootGeneratorComponent::SetRootEnabled(bool bEnabled)
{
	if (bEnabled) GenerateRoot();
	else DestroyRoot();
}

USplineComponent* URootGeneratorComponent::GenerateRoot()
{
	ResolveTargetMesh();

	UE_LOG(LogTemp, Warning, TEXT("[%s] GenerateRoot. Owner=%s Target=%s TraceChannel=%d"),
		*RootTag.ToString(),
		GetOwner() ? *GetOwner()->GetName() : TEXT("NULL"),
		TargetMeshPtr ? *TargetMeshPtr->GetName() : TEXT("NULL"),
		(int32)TraceChannel);

	if (!TargetMeshPtr || !GetWorld() || !GetOwner())
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] Abort: missing TargetMeshPtr/World/Owner"), *RootTag.ToString());
		return nullptr;
	}

	if (!BuildRootPathSimpleStartEnd())
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] Abort: BuildRootPathSimpleStartEnd failed"), *RootTag.ToString());
		return nullptr;
	}

	if (!BuildOrUpdateSplineFromPoints())
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] Abort: BuildOrUpdateSplineFromPoints failed"), *RootTag.ToString());
		return nullptr;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] OK. Points=%d Spline=%s"),
		*RootTag.ToString(),
		RootPoints.Num(),
		RootSpline ? TEXT("OK") : TEXT("NULL"));

	return RootSpline;
}

void URootGeneratorComponent::DestroyRoot()
{
	CleanupSpline();
	RootPoints.Reset();
}

void URootGeneratorComponent::CleanupSpline()
{
	if (RootSpline)
	{
		RootSpline->DestroyComponent();
		RootSpline = nullptr;
	}
}

void URootGeneratorComponent::ResolveTargetMesh()
{
	if (TargetMeshPtr) return;

	if (UActorComponent* C = TargetMesh.GetComponent(GetOwner()))
	{
		TargetMeshPtr = Cast<UStaticMeshComponent>(C);
	}
	if (!TargetMeshPtr && GetOwner())
	{
		TargetMeshPtr = GetOwner()->FindComponentByClass<UStaticMeshComponent>();
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] ResolveTargetMesh: %s"),
		*RootTag.ToString(),
		TargetMeshPtr ? *TargetMeshPtr->GetName() : TEXT("NULL"));
}

float URootGeneratorComponent::PolylineLength(const TArray<FVector>& Pts)
{
	float L = 0.f;
	for (int32 i = 0; i < Pts.Num() - 1; i++)
	{
		L += FVector::Distance(Pts[i], Pts[i + 1]);
	}
	return L;
}

bool URootGeneratorComponent::BuildRootPathSimpleStartEnd()
{
	RootPoints.Reset();

	UWorld* World = GetWorld();
	if (!World || !TargetMeshPtr || !GetOwner()) return false;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RootTrace), false);

	auto TraceAccept = [&](const FVector& A, const FVector& B, FHitResult& OutHit) -> bool
		{
			FHitResult Hit;
			if (!World->LineTraceSingleByChannel(Hit, A, B, TraceChannel, Params)) return false;
			if (Hit.GetComponent() != TargetMeshPtr) return false;
			OutHit = Hit;
			return true;
		};

	FVector Origin, Extents;
	GetOwner()->GetActorBounds(true, Origin, Extents);

	// Start
	FHitResult StartHit;
	{
		bool bFound = false;
		for (int32 i = 0; i < 40 && !bFound; i++)
		{
			const FVector Dir = UKismetMathLibrary::RandomUnitVector();
			const FVector A = Origin + Dir * (Extents.Size() + TraceHeightCm);
			const FVector B = Origin;
			bFound = TraceAccept(A, B, StartHit);
		}
		if (!bFound)
		{
			UE_LOG(LogTemp, Error, TEXT("[%s] No StartHit. Check collision & TraceChannel."), *RootTag.ToString());
			return false;
		}
	}

	const FVector StartP = StartHit.ImpactPoint + StartHit.ImpactNormal.GetSafeNormal() * SurfaceOffsetCm;

	// End (farthest from start)
	FHitResult EndHit;
	{
		bool bFound = false;
		float Best = -1.f;

		for (int32 i = 0; i < 80; i++)
		{
			const FVector Dir = UKismetMathLibrary::RandomUnitVector();
			const FVector A = Origin + Dir * (Extents.Size() + TraceHeightCm);
			const FVector B = Origin;

			FHitResult H;
			if (!TraceAccept(A, B, H)) continue;

			const float D2 = FVector::DistSquared(H.ImpactPoint, StartP);
			if (D2 > Best)
			{
				Best = D2;
				EndHit = H;
				bFound = true;
			}
		}
		if (!bFound)
		{
			UE_LOG(LogTemp, Error, TEXT("[%s] No EndHit."), *RootTag.ToString());
			return false;
		}
	}

	const FVector EndP = EndHit.ImpactPoint + EndHit.ImpactNormal.GetSafeNormal() * SurfaceOffsetCm;

	RootPoints.Add(StartP);

	// Step toward end with swirl + projection
	FVector P = StartP;
	FVector N = StartHit.ImpactNormal.GetSafeNormal();
	FVector D = FVector::CrossProduct(N, FVector::UpVector).GetSafeNormal();
	if (D.IsNearlyZero()) D = FVector::CrossProduct(N, FVector::RightVector).GetSafeNormal();

	const int32 MaxSteps = FMath::Clamp(MaxPoints, 8, 1024);

	for (int32 i = 0; i < MaxSteps; i++)
	{
		const FVector ToEnd = (EndP - P);
		const float DistToEnd = ToEnd.Size();

		if (DistToEnd <= StepLengthCm * 1.25f)
		{
			RootPoints.Add(EndP);
			break;
		}

		const FVector Toward = ToEnd.GetSafeNormal();
		FVector Blend = (D * 0.55f + Toward * 0.45f).GetSafeNormal();
		Blend = (Blend - FVector::DotProduct(Blend, N) * N).GetSafeNormal();
		if (Blend.IsNearlyZero()) Blend = Toward;

		const float Turn = FMath::FRandRange(-MaxTurnDeg, MaxTurnDeg);
		Blend = Blend.RotateAngleAxis(Turn, N).GetSafeNormal();

		const FVector Candidate = P + Blend * StepLengthCm;

		FHitResult ProjHit;
		bool bProjected = false;

		// normal both ways
		{
			const FVector A = Candidate + N * TraceHeightCm;
			const FVector B = Candidate - N * TraceHeightCm;
			if (TraceAccept(A, B, ProjHit)) bProjected = true;
			else if (TraceAccept(B, A, ProjHit)) bProjected = true;
		}
		// toward actor center
		if (!bProjected)
		{
			const FVector CenterDir = (Origin - Candidate).GetSafeNormal();
			const FVector A = Candidate + CenterDir * TraceHeightCm;
			const FVector B = Candidate - CenterDir * TraceHeightCm;
			if (TraceAccept(A, B, ProjHit)) bProjected = true;
			else if (TraceAccept(B, A, ProjHit)) bProjected = true;
		}
		// world up/down fallback
		if (!bProjected)
		{
			const FVector A = Candidate + FVector(0, 0, TraceHeightCm);
			const FVector B = Candidate - FVector(0, 0, TraceHeightCm);
			if (TraceAccept(A, B, ProjHit)) bProjected = true;
			else if (TraceAccept(B, A, ProjHit)) bProjected = true;
		}

		if (!bProjected)
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] Step failed at i=%d. Stop."), *RootTag.ToString(), i);
			break;
		}

		const FVector NewN = ProjHit.ImpactNormal.GetSafeNormal();
		const FVector NewP = ProjHit.ImpactPoint + NewN * SurfaceOffsetCm;

		if (FVector::DistSquared(P, NewP) < 1.f) break;

		if (bDebugDraw)
		{
			DrawDebugLine(World, P, NewP, FColor::Green, false, 8.f, 0, 2.f);
		}

		RootPoints.Add(NewP);

		P = NewP;
		N = NewN;
		D = Blend;

		if (RootPoints.Num() >= MaxSegments + 1) break;
	}

	if (RootPoints.Num() < 2)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] Path ended with <2 points"), *RootTag.ToString());
		return false;
	}

	const float Len = PolylineLength(RootPoints);
	UE_LOG(LogTemp, Warning, TEXT("[%s] Path OK. Points=%d TotalLen=%.1f"), *RootTag.ToString(), RootPoints.Num(), Len);

	return Len > 1.f;
}

bool URootGeneratorComponent::BuildOrUpdateSplineFromPoints()
{
	if (!GetOwner() || !TargetMeshPtr) return false;
	if (RootPoints.Num() < 2) return false;

	AActor* Owner = GetOwner();

	// Create if needed
	if (!RootSpline)
	{
		RootSpline = NewObject<USplineComponent>(Owner);
		if (!RootSpline) return false;

		RootSpline->RegisterComponent();
		// keep world so spline points are world coords
		RootSpline->AttachToComponent(TargetMeshPtr, FAttachmentTransformRules::KeepWorldTransform);
	}

	RootSpline->ClearSplinePoints(false);
	for (const FVector& Pt : RootPoints)
	{
		RootSpline->AddSplinePoint(Pt, ESplineCoordinateSpace::World, false);
	}
	RootSpline->UpdateSpline();
	return true;
}
