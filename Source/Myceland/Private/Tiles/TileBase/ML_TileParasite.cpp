#include "Tiles/TileBase/ML_TileParasite.h"

#include "EngineUtils.h"
#include "TechArt/ML_ParasiteRootNetwork.h"

AML_TileParasite::AML_TileParasite()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AML_TileParasite::BeginPlay()
{
	Super::BeginPlay();
	NotifyRootNetworksRegistered();
}

void AML_TileParasite::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	NotifyRootNetworksUnregistered();
	Super::EndPlay(EndPlayReason);
}

void AML_TileParasite::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AML_TileParasite::NotifyRootNetworksRegistered()
{
	UWorld* World = GetWorld();
	if (!World) return;

	for (TActorIterator<AML_ParasiteRootNetwork> It(World); It; ++It)
	{
		AML_ParasiteRootNetwork* Network = *It;
		if (IsValid(Network))
		{
			Network->RegisterParasiteActor(this);
		}
	}
}

void AML_TileParasite::NotifyRootNetworksUnregistered()
{
	UWorld* World = GetWorld();
	if (!World) return;

	for (TActorIterator<AML_ParasiteRootNetwork> It(World); It; ++It)
	{
		AML_ParasiteRootNetwork* Network = *It;
		if (IsValid(Network))
		{
			Network->UnregisterParasiteActor(this);
		}
	}
}