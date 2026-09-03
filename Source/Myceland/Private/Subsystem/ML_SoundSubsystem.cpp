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
