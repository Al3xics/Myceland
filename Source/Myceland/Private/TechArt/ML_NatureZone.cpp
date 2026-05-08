#include "TechArt/ML_NatureZone.h"

#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "InstancedFoliageActor.h"

AML_NatureZone::AML_NatureZone()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	DetectionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectionBox"));
	DetectionBox->SetupAttachment(SceneRoot);
	DetectionBox->SetBoxExtent(FVector(200.f, 200.f, 200.f));
	DetectionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DetectionBox->SetCollisionResponseToAllChannels(ECR_Overlap);
}

void AML_NatureZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bIsGrowing)
	{
		return;
	}

	GrowthElapsed += DeltaSeconds;

	const float SafeDuration = FMath::Max(GrowthDuration, 0.01f);
	float Alpha = FMath::Clamp(GrowthElapsed / SafeDuration, 0.0f, 1.0f);

	if (bUseSmoothStepGrowth)
	{
		Alpha = FMath::SmoothStep(0.0f, 1.0f, Alpha);
	}

	ApplyGrowthAlpha(Alpha);

	if (GrowthElapsed >= SafeDuration)
	{
		bIsGrowing = false;
		ApplyGrowthAlpha(1.0f);
	}
}

TArray<FML_NatureInstanceReference> AML_NatureZone::GetFoliageInstancesInsideBox() const
{
	TArray<FML_NatureInstanceReference> Result;

	if (!GetWorld() || !DetectionBox)
	{
		return Result;
	}

	const FBox WorldBox = DetectionBox->Bounds.GetBox();

	for (TActorIterator<AInstancedFoliageActor> It(GetWorld()); It; ++It)
	{
		AInstancedFoliageActor* FoliageActor = *It;

		if (!IsValid(FoliageActor))
		{
			continue;
		}

		TArray<UActorComponent*> Components;
		FoliageActor->GetComponents(
			UInstancedStaticMeshComponent::StaticClass(),
			Components
		);

		for (UActorComponent* RawComponent : Components)
		{
			UInstancedStaticMeshComponent* ISM =
				Cast<UInstancedStaticMeshComponent>(RawComponent);

			if (!IsValid(ISM))
			{
				continue;
			}

			UStaticMesh* Mesh = ISM->GetStaticMesh();

			if (!ShouldAcceptMesh(Mesh))
			{
				continue;
			}

			const TArray<int32> Indices =
				ISM->GetInstancesOverlappingBox(WorldBox, true);

			for (int32 Index : Indices)
			{
				FTransform WorldTransform;

				if (!ISM->GetInstanceTransform(Index, WorldTransform, true))
				{
					continue;
				}

				if (bUsePreciseRotatedBoxCheck &&
					!IsWorldLocationInsideDetectionBox(WorldTransform.GetLocation()))
				{
					continue;
				}

				FML_NatureInstanceReference Ref;
				Ref.Component = ISM;
				Ref.InstanceIndex = Index;
				Ref.StaticMesh = Mesh;
				Ref.WorldTransform = WorldTransform;

				Result.Add(Ref);
			}
		}
	}

	return Result;
}

bool AML_NatureZone::SetFoliageInstanceScale(
	const FML_NatureInstanceReference& InstanceRef,
	FVector NewScale,
	bool bMarkRenderStateDirty
)
{
	if (!InstanceRef.IsValid())
	{
		return false;
	}

	UInstancedStaticMeshComponent* ISM = InstanceRef.Component.Get();

	if (!IsValid(ISM))
	{
		return false;
	}

	FTransform WorldTransform;

	if (!ISM->GetInstanceTransform(InstanceRef.InstanceIndex, WorldTransform, true))
	{
		return false;
	}

	WorldTransform.SetScale3D(NewScale);

	return ISM->UpdateInstanceTransform(
		InstanceRef.InstanceIndex,
		WorldTransform,
		true,
		bMarkRenderStateDirty,
		true
	);
}

int32 AML_NatureZone::SetFoliageInstancesScale(
	const TArray<FML_NatureInstanceReference>& InstanceRefs,
	FVector NewScale
)
{
	int32 ChangedCount = 0;
	TSet<TObjectPtr<UInstancedStaticMeshComponent>> DirtyComponents;

	for (const FML_NatureInstanceReference& Ref : InstanceRefs)
	{
		if (!Ref.IsValid())
		{
			continue;
		}

		UInstancedStaticMeshComponent* ISM = Ref.Component.Get();

		if (!IsValid(ISM))
		{
			continue;
		}

		FTransform WorldTransform;

		if (!ISM->GetInstanceTransform(Ref.InstanceIndex, WorldTransform, true))
		{
			continue;
		}

		WorldTransform.SetScale3D(NewScale);

		const bool bUpdated = ISM->UpdateInstanceTransform(
			Ref.InstanceIndex,
			WorldTransform,
			true,
			false,
			true
		);

		if (bUpdated)
		{
			ChangedCount++;
			DirtyComponents.Add(ISM);
		}
	}

	for (UInstancedStaticMeshComponent* ISM : DirtyComponents)
	{
		if (IsValid(ISM))
		{
			ISM->MarkRenderStateDirty();
		}
	}

	return ChangedCount;
}

int32 AML_NatureZone::CacheFoliageInstancesInsideBox()
{
	CachedInstances.Empty();

	const TArray<FML_NatureInstanceReference> FoundInstances =
		GetFoliageInstancesInsideBox();

	for (const FML_NatureInstanceReference& Ref : FoundInstances)
	{
		if (!Ref.IsValid())
		{
			continue;
		}

		FML_NatureCachedInstance Cached;
		Cached.InstanceRef = Ref;
		Cached.OriginalScale = Ref.WorldTransform.GetScale3D();

		CachedInstances.Add(Cached);
	}

	return CachedInstances.Num();
}

int32 AML_NatureZone::CacheAndHideFoliageInstancesInsideBox()
{
	const int32 CachedCount = CacheFoliageInstancesInsideBox();
	HideCachedFoliage();
	return CachedCount;
}

int32 AML_NatureZone::HideCachedFoliage()
{
	int32 ChangedCount = 0;
	TSet<TObjectPtr<UInstancedStaticMeshComponent>> DirtyComponents;

	const FVector TinyScale(HiddenScale, HiddenScale, HiddenScale);

	for (const FML_NatureCachedInstance& Cached : CachedInstances)
	{
		const FML_NatureInstanceReference& Ref = Cached.InstanceRef;

		if (!Ref.IsValid())
		{
			continue;
		}

		UInstancedStaticMeshComponent* ISM = Ref.Component.Get();

		if (!IsValid(ISM))
		{
			continue;
		}

		FTransform WorldTransform;

		if (!ISM->GetInstanceTransform(Ref.InstanceIndex, WorldTransform, true))
		{
			continue;
		}

		WorldTransform.SetScale3D(TinyScale);

		const bool bUpdated = ISM->UpdateInstanceTransform(
			Ref.InstanceIndex,
			WorldTransform,
			true,
			false,
			true
		);

		if (bUpdated)
		{
			ChangedCount++;
			DirtyComponents.Add(ISM);
		}
	}

	for (UInstancedStaticMeshComponent* ISM : DirtyComponents)
	{
		if (IsValid(ISM))
		{
			ISM->MarkRenderStateDirty();
		}
	}

	return ChangedCount;
}

void AML_NatureZone::StartGrowCachedFoliage()
{
	if (CachedInstances.IsEmpty())
	{
		CacheAndHideFoliageInstancesInsideBox();
	}

	GrowthElapsed = 0.0f;
	bIsGrowing = true;

	ApplyGrowthAlpha(0.0f);
}

void AML_NatureZone::ClearCachedFoliage()
{
	CachedInstances.Empty();
	bIsGrowing = false;
	GrowthElapsed = 0.0f;
}

void AML_NatureZone::ApplyGrowthAlpha(float Alpha)
{
	TSet<TObjectPtr<UInstancedStaticMeshComponent>> DirtyComponents;

	const FVector StartScale(HiddenScale, HiddenScale, HiddenScale);

	for (const FML_NatureCachedInstance& Cached : CachedInstances)
	{
		const FML_NatureInstanceReference& Ref = Cached.InstanceRef;

		if (!Ref.IsValid())
		{
			continue;
		}

		UInstancedStaticMeshComponent* ISM = Ref.Component.Get();

		if (!IsValid(ISM))
		{
			continue;
		}

		FTransform WorldTransform;

		if (!ISM->GetInstanceTransform(Ref.InstanceIndex, WorldTransform, true))
		{
			continue;
		}

		const FVector NewScale =
			FMath::Lerp(StartScale, Cached.OriginalScale, Alpha);

		WorldTransform.SetScale3D(NewScale);

		const bool bUpdated = ISM->UpdateInstanceTransform(
			Ref.InstanceIndex,
			WorldTransform,
			true,
			false,
			true
		);

		if (bUpdated)
		{
			DirtyComponents.Add(ISM);
		}
	}

	for (UInstancedStaticMeshComponent* ISM : DirtyComponents)
	{
		if (IsValid(ISM))
		{
			ISM->MarkRenderStateDirty();
		}
	}
}

bool AML_NatureZone::ShouldAcceptMesh(const UStaticMesh* Mesh) const
{
	if (!IsValid(Mesh))
	{
		return false;
	}

	if (TargetMeshes.IsEmpty())
	{
		return true;
	}

	for (const UStaticMesh* TargetMesh : TargetMeshes)
	{
		if (Mesh == TargetMesh)
		{
			return true;
		}
	}

	return false;
}

bool AML_NatureZone::IsWorldLocationInsideDetectionBox(const FVector& WorldLocation) const
{
	if (!DetectionBox)
	{
		return false;
	}

	const FTransform BoxTransform = DetectionBox->GetComponentTransform();
	const FVector LocalLocation = BoxTransform.InverseTransformPosition(WorldLocation);
	const FVector Extent = DetectionBox->GetUnscaledBoxExtent();

	return
		FMath::Abs(LocalLocation.X) <= Extent.X &&
		FMath::Abs(LocalLocation.Y) <= Extent.Y &&
		FMath::Abs(LocalLocation.Z) <= Extent.Z;
}