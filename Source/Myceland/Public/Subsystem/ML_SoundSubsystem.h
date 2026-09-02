// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FMODBlueprintStatics.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ML_SoundSubsystem.generated.h"

class UFMODAudioComponent;
class UFMODEvent;
class UML_SoundPlaybackHandle;
class UML_SoundSubsystem;

DECLARE_DYNAMIC_DELEGATE_OneParam(FML_OnSoundFinished, UML_SoundPlaybackHandle*, PlaybackHandle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FML_OnSoundPlaybackFinished, UML_SoundPlaybackHandle*, PlaybackHandle);

UCLASS(BlueprintType)
class MYCELAND_API UML_SoundPlaybackHandle : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category="Sound|FMOD")
	FML_OnSoundPlaybackFinished OnFinished;

	UFUNCTION(BlueprintCallable, Category="Sound|FMOD")
	void Stop();

	UFUNCTION(BlueprintPure, Category="Sound|FMOD")
	bool IsPlaying() const;

	UFUNCTION(BlueprintPure, Category="Sound|FMOD")
	UFMODAudioComponent* GetAudioComponent() const { return AudioComponent; }

private:
	friend class UML_SoundSubsystem;

	UPROPERTY()
	TObjectPtr<UFMODAudioComponent> AudioComponent = nullptr;

	UPROPERTY()
	TObjectPtr<UML_SoundSubsystem> SoundSubsystem = nullptr;

	FML_OnSoundFinished OnFinishedCallback;

	void Initialize(UML_SoundSubsystem* InSoundSubsystem, UFMODAudioComponent* InAudioComponent, const FML_OnSoundFinished& InOnFinishedCallback);

	UFUNCTION()
	void HandleEventStopped();
};

UCLASS()
class MYCELAND_API UML_SoundSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

private:
	UPROPERTY()
	TArray<TObjectPtr<UML_SoundPlaybackHandle>> ActivePlaybackHandles;

public:
	static UML_SoundSubsystem* Get(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category="Sound|FMOD")
	UFMODAudioComponent* StartSound(UFMODEvent* Sound, UFMODAudioComponent* AudioComponent, bool bRestartIfPlaying = true);

	UFUNCTION(BlueprintCallable, Category="Sound|FMOD")
	void StopSound(UFMODAudioComponent* AudioComponent);

	UFUNCTION(BlueprintPure, Category="Sound|FMOD")
	bool IsSoundPlaying(UFMODAudioComponent* AudioComponent) const;

	UFUNCTION(BlueprintCallable, Category="Sound|FMOD")
	FFMODEventInstance StartSound2D(UFMODEvent* Sound, bool bAutoPlay = true);

	UFUNCTION(BlueprintCallable, Category="Sound|FMOD")
	FFMODEventInstance StartSoundAtLocation(UFMODEvent* Sound, const FTransform& Location, bool bAutoPlay = true);

	/** Resolve an FMOD Studio event path (event:/...) and play it as a 2D sound. */
	UFUNCTION(BlueprintCallable, Category="Sound|FMOD")
	FFMODEventInstance StartSound2DByPath(const FString& EventPath, bool bAutoPlay = true);

	/** Resolve an FMOD Studio event path (event:/...) and play it at a world location. */
	UFUNCTION(BlueprintCallable, Category="Sound|FMOD")
	FFMODEventInstance StartSoundAtLocationByPath(const FString& EventPath, const FTransform& Location, bool bAutoPlay = true);

	UFUNCTION(BlueprintCallable, Category="Sound|FMOD")
	UML_SoundPlaybackHandle* StartTrackedSound2D(UFMODEvent* Sound, FML_OnSoundFinished OnFinished, bool bAutoDestroy = true);

	UFUNCTION(BlueprintCallable, Category="Sound|FMOD")
	UML_SoundPlaybackHandle* StartTrackedSoundAtLocation(UFMODEvent* Sound, const FTransform& Location, FML_OnSoundFinished OnFinished, bool bAutoDestroy = true);

	UFUNCTION(BlueprintCallable, Category="Sound|FMOD")
	void StopSoundInstance(FFMODEventInstance SoundInstance, bool bRelease = true);

	UFUNCTION(BlueprintCallable, Category="Sound|FMOD")
	void SetSoundParameter(FFMODEventInstance SoundInstance, FName ParameterName, float Value);

	UFUNCTION(BlueprintCallable, Category="Sound|FMOD")
	void SetGlobalSoundParameter(FName ParameterName, float Value);
	
	UML_SoundPlaybackHandle* CreateTrackedSound(UFMODEvent* Sound, const FTransform& Location, const FML_OnSoundFinished& OnFinished, bool bAutoDestroy, const TCHAR* FunctionName);
	void NotifyPlaybackFinished(UML_SoundPlaybackHandle* PlaybackHandle);
};
