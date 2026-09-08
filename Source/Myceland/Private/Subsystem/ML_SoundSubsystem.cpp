// Copyright Myceland Team, All Rights Reserved.


#include "Subsystem/ML_SoundSubsystem.h"

#include "FMODAudioComponent.h"
#include "FMODEvent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogMycelandAudio, Log, All);

void UML_SoundPlaybackHandle::Initialize(UML_SoundSubsystem* InSoundSubsystem, UFMODAudioComponent* InAudioComponent, const FML_OnSoundFinished& InOnFinishedCallback)
{
	SoundSubsystem = InSoundSubsystem;
	AudioComponent = InAudioComponent;
	OnFinishedCallback = InOnFinishedCallback;

	if (AudioComponent)
		AudioComponent->OnEventStopped.AddDynamic(this, &UML_SoundPlaybackHandle::HandleEventStopped);
}

void UML_SoundPlaybackHandle::Stop()
{
	if (AudioComponent && AudioComponent->IsPlaying())
		AudioComponent->Stop();
}

void UML_SoundPlaybackHandle::ReleaseComponent()
{
	if (!AudioComponent)
		return;

	AudioComponent->OnEventStopped.RemoveAll(this);
	AudioComponent = nullptr;
}

bool UML_SoundPlaybackHandle::IsPlaying() const
{
	return AudioComponent && AudioComponent->IsPlaying();
}

void UML_SoundPlaybackHandle::HandleEventStopped()
{
	if (AudioComponent)
		AudioComponent->OnEventStopped.RemoveAll(this);

	if (OnFinishedCallback.IsBound())
		OnFinishedCallback.Execute(this);

	OnFinished.Broadcast(this);

	if (SoundSubsystem)
		SoundSubsystem->NotifyPlaybackFinished(this);
}

void UML_SoundSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Tracked sounds run on audio components owned by this subsystem - which outlives every world - but
	// registered with whichever world was current when they started. They have no owning actor and belong to
	// no level, so UWorld::CleanupWorld never unregisters them: after a level change they stay flagged as
	// registered while pointing at a destroyed world. USceneComponent always creates a render state, so the
	// next global render state recreate (any scalability change, the settings Apply button included) walks
	// into that dead world's scene and crashes. Tear them down with their world instead.
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &UML_SoundSubsystem::HandleWorldCleanup);
}

void UML_SoundSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	WorldCleanupHandle.Reset();

	DestroyTrackedSounds(nullptr);

	Super::Deinitialize();
}

void UML_SoundSubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	DestroyTrackedSounds(World);
}

void UML_SoundSubsystem::DestroyTrackedSounds(const UWorld* World)
{
	// Iterate copies: releasing a handle can fire OnEventStopped, which removes it from
	// ActivePlaybackHandles through NotifyPlaybackFinished.
	const TArray<TObjectPtr<UML_SoundPlaybackHandle>> HandlesToCheck = ActivePlaybackHandles;

	for (UML_SoundPlaybackHandle* Handle : HandlesToCheck)
	{
		if (!IsValid(Handle))
		{
			ActivePlaybackHandles.Remove(Handle);
			continue;
		}

		// A null World means every world (subsystem shutdown). A handle whose component is already gone is
		// dead weight either way, so it is dropped too.
		const UFMODAudioComponent* AudioComponent = Handle->GetAudioComponent();
		if (World && AudioComponent && AudioComponent->GetWorld() != World)
			continue;

		Handle->ReleaseComponent();
		ActivePlaybackHandles.Remove(Handle);
	}

	// The components are destroyed from their own list, not through the handles: a tracked sound that ended
	// or was stopped without bAutoDestroy has already dropped its handle, and this list is then the only
	// thing left pointing at its still-registered component.
	const TArray<TObjectPtr<UFMODAudioComponent>> ComponentsToCheck = TrackedAudioComponents;

	for (UFMODAudioComponent* AudioComponent : ComponentsToCheck)
	{
		if (!IsValid(AudioComponent))
		{
			TrackedAudioComponents.Remove(AudioComponent);
			continue;
		}

		if (World && AudioComponent->GetWorld() != World)
			continue;

		if (AudioComponent->IsPlaying())
			AudioComponent->Stop();

		// DestroyComponent unregisters first, which is the whole point: a component left registered on a
		// world that is going away is what crashes the next global render state recreate.
		AudioComponent->DestroyComponent();
		TrackedAudioComponents.Remove(AudioComponent);
	}
}

UML_SoundSubsystem* UML_SoundSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
		return nullptr;

	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
	return GameInstance ? GameInstance->GetSubsystem<UML_SoundSubsystem>() : nullptr;
}

UFMODAudioComponent* UML_SoundSubsystem::StartSound(UFMODEvent* Sound, UFMODAudioComponent* AudioComponent, const bool bRestartIfPlaying)
{
	if (!Sound)
	{
		UE_LOG(LogTemp, Warning, TEXT("StartSound called with no FMOD event"));
		return nullptr;
	}

	if (!AudioComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("StartSound called with no FMOD audio component for event %s"), *Sound->GetName());
		return nullptr;
	}

	if (bRestartIfPlaying && AudioComponent->IsPlaying())
		AudioComponent->Stop();

	AudioComponent->SetEvent(Sound);
	AudioComponent->Play();
	return AudioComponent;
}

void UML_SoundSubsystem::StopSound(UFMODAudioComponent* AudioComponent)
{
	if (AudioComponent && AudioComponent->IsPlaying())
		AudioComponent->Stop();
}

bool UML_SoundSubsystem::IsSoundPlaying(UFMODAudioComponent* AudioComponent) const
{
	return AudioComponent && AudioComponent->IsPlaying();
}

FFMODEventInstance UML_SoundSubsystem::StartSound2D(UFMODEvent* Sound, const bool bAutoPlay)
{
	if (!Sound)
	{
		UE_LOG(LogTemp, Warning, TEXT("StartSound2D called with no FMOD event"));
		return FFMODEventInstance();
	}

	return UFMODBlueprintStatics::PlayEvent2D(this, Sound, bAutoPlay);
}

FFMODEventInstance UML_SoundSubsystem::StartSoundAtLocation(UFMODEvent* Sound, const FTransform& Location, const bool bAutoPlay)
{
	if (!Sound)
	{
		UE_LOG(LogTemp, Warning, TEXT("StartSoundAtLocation called with no FMOD event"));
		return FFMODEventInstance();
	}

	return UFMODBlueprintStatics::PlayEventAtLocation(this, Sound, Location, bAutoPlay);
}

FFMODEventInstance UML_SoundSubsystem::StartSound2DByPath(const FString& EventPath, const bool bAutoPlay)
{
	UFMODEvent* Sound = UFMODBlueprintStatics::FindEventByName(EventPath);
	if (!Sound)
	{
		UE_LOG(LogTemp, Warning, TEXT("FMOD event not found: %s. Build the banks and refresh FMOD assets in Unreal."), *EventPath);
		return FFMODEventInstance();
	}

	const FFMODEventInstance Instance = StartSound2D(Sound, bAutoPlay);
	UE_LOG(LogMycelandAudio, Log, TEXT("Played 2D event %s (valid instance: %s)"),
		*EventPath,
		UFMODBlueprintStatics::EventInstanceIsValid(Instance) ? TEXT("true") : TEXT("false"));
	return Instance;
}

FFMODEventInstance UML_SoundSubsystem::StartSoundAtLocationByPath(const FString& EventPath, const FTransform& Location, const bool bAutoPlay)
{
	UFMODEvent* Sound = UFMODBlueprintStatics::FindEventByName(EventPath);
	if (!Sound)
	{
		UE_LOG(LogTemp, Warning, TEXT("FMOD event not found: %s. Build the banks and refresh FMOD assets in Unreal."), *EventPath);
		return FFMODEventInstance();
	}

	const FFMODEventInstance Instance = StartSoundAtLocation(Sound, Location, bAutoPlay);
	UE_LOG(LogMycelandAudio, Log, TEXT("Played 3D event %s at %s (valid instance: %s)"),
		*EventPath,
		*Location.GetLocation().ToCompactString(),
		UFMODBlueprintStatics::EventInstanceIsValid(Instance) ? TEXT("true") : TEXT("false"));
	return Instance;
}

UML_SoundPlaybackHandle* UML_SoundSubsystem::StartTrackedSound2D(UFMODEvent* Sound, FML_OnSoundFinished OnFinished, const bool bAutoDestroy)
{
	return CreateTrackedSound(Sound, FTransform::Identity, OnFinished, bAutoDestroy, TEXT("StartTrackedSound2D"));
}

UML_SoundPlaybackHandle* UML_SoundSubsystem::StartTrackedSoundAtLocation(UFMODEvent* Sound, const FTransform& Location, FML_OnSoundFinished OnFinished, const bool bAutoDestroy)
{
	return CreateTrackedSound(Sound, Location, OnFinished, bAutoDestroy, TEXT("StartTrackedSoundAtLocation"));
}

UML_SoundPlaybackHandle* UML_SoundSubsystem::StartTrackedSound2DByPath(const FString& EventPath, FML_OnSoundFinished OnFinished, const bool bAutoDestroy)
{
	UFMODEvent* Sound = UFMODBlueprintStatics::FindEventByName(EventPath);
	if (!Sound)
	{
		UE_LOG(LogTemp, Warning, TEXT("FMOD event not found: %s. Build the banks and refresh FMOD assets in Unreal."), *EventPath);
		return nullptr;
	}

	return CreateTrackedSound(Sound, FTransform::Identity, OnFinished, bAutoDestroy, TEXT("StartTrackedSound2DByPath"));
}

UML_SoundPlaybackHandle* UML_SoundSubsystem::CreateTrackedSound(UFMODEvent* Sound, const FTransform& Location, const FML_OnSoundFinished& OnFinished, const bool bAutoDestroy, const TCHAR* FunctionName)
{
	if (!Sound)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s called with no FMOD event"), FunctionName);
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s called without a valid world for event %s"), FunctionName, *Sound->GetName());
		return nullptr;
	}

	UFMODAudioComponent* AudioComponent = NewObject<UFMODAudioComponent>(this);
	if (!AudioComponent)
		return nullptr;

	AudioComponent->Event = Sound;
	AudioComponent->bAutoActivate = false;
	AudioComponent->bAutoDestroy = bAutoDestroy;
#if WITH_EDITORONLY_DATA
	AudioComponent->bVisualizeComponent = false;
#endif
	AudioComponent->RegisterComponentWithWorld(World);
	AudioComponent->SetWorldTransform(Location);

	// Registered on World but owned by this subsystem, which outlives it: DestroyTrackedSounds is what
	// unregisters it when that world is torn down.
	TrackedAudioComponents.Add(AudioComponent);

	UML_SoundPlaybackHandle* PlaybackHandle = NewObject<UML_SoundPlaybackHandle>(this);
	PlaybackHandle->Initialize(this, AudioComponent, OnFinished);
	ActivePlaybackHandles.Add(PlaybackHandle);

	AudioComponent->Play();
	return PlaybackHandle;
}

void UML_SoundSubsystem::NotifyPlaybackFinished(UML_SoundPlaybackHandle* PlaybackHandle)
{
	ActivePlaybackHandles.Remove(PlaybackHandle);
}

void UML_SoundSubsystem::StopSoundInstance(const FFMODEventInstance SoundInstance, const bool bRelease)
{
	if (UFMODBlueprintStatics::EventInstanceIsValid(SoundInstance))
		UFMODBlueprintStatics::EventInstanceStop(SoundInstance, bRelease);
}

void UML_SoundSubsystem::SetSoundParameter(const FFMODEventInstance SoundInstance, const FName ParameterName, const float Value)
{
	if (ParameterName.IsNone() || !UFMODBlueprintStatics::EventInstanceIsValid(SoundInstance))
		return;

	UFMODBlueprintStatics::EventInstanceSetParameter(SoundInstance, ParameterName, Value);
}

void UML_SoundSubsystem::SetGlobalSoundParameter(const FName ParameterName, const float Value)
{
	if (ParameterName.IsNone())
		return;

	UFMODBlueprintStatics::SetGlobalParameterByName(ParameterName, Value);
}
