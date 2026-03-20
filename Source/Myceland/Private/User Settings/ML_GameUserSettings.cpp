// Copyright Myceland Team, All Rights Reserved.


#include "User Settings/ML_GameUserSettings.h"

#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "Kismet/GameplayStatics.h"

UML_GameUserSettings::UML_GameUserSettings()
{
	// Set default values
	InitValues();
}

void UML_GameUserSettings::InitValues()
{
	MasterVolume = 1.0f;
	MusicVolume = 1.0f;
	SFXVolume = 1.0f;
	UIVolume = 1.0f;
	VoiceVolume = 1.0f;
    
	MouseSensitivity = 1.0f;
	GamepadSensitivity = 1.0f;
	GamepadDeadZone = 0.25f;
}


// ==================== Audio Settings ====================

void UML_GameUserSettings::SetMasterVolume(float Volume)
{
	MasterVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
}

void UML_GameUserSettings::SetMusicVolume(float Volume)
{
	MusicVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
}

void UML_GameUserSettings::SetSFXVolume(float Volume)
{
	SFXVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
}

void UML_GameUserSettings::SetUIVolume(float Volume)
{
	UIVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
}

void UML_GameUserSettings::SetVoiceVolume(float Volume)
{
	VoiceVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
}

void UML_GameUserSettings::ApplyAudioSettings()
{
	const UML_MycelandDeveloperSettings* DevSettings = UML_MycelandDeveloperSettings::GetMycelandDeveloperSettings();
	ensureMsgf(DevSettings, TEXT("Failed to get MycelandDeveloperSettings for audio"));
    if (!DevSettings) return;

    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
    if (!World)
    {
	    UE_LOG(LogTemp, Error, TEXT("Failed to get world from context object"));
    	return;
    }

    // Load Sound Classes from Developer Settings
    USoundClass* MasterClass = DevSettings->MasterSoundClass.LoadSynchronous();
    USoundClass* MusicClass = DevSettings->MusicSoundClass.LoadSynchronous();
    USoundClass* SFXClass = DevSettings->SFXSoundClass.LoadSynchronous();
    USoundClass* UIClass = DevSettings->UISoundClass.LoadSynchronous();
    USoundClass* VoiceClass = DevSettings->VoiceSoundClass.LoadSynchronous();
    
    USoundMix* GameMix = DevSettings->GameSoundMix.LoadSynchronous();
	ensureMsgf(GameMix, TEXT("Failed to get GameSoundMix from Developer Settings"));
    if (!GameMix) return;

    // Apply volumes to Sound Mix
    if (MasterClass)
        UGameplayStatics::SetSoundMixClassOverride(World, GameMix, MasterClass, MasterVolume);
    
    if (MusicClass)
        UGameplayStatics::SetSoundMixClassOverride(World, GameMix, MusicClass, MusicVolume);
    
    if (SFXClass)
        UGameplayStatics::SetSoundMixClassOverride(World, GameMix, SFXClass, SFXVolume);
    
    if (UIClass)
        UGameplayStatics::SetSoundMixClassOverride(World, GameMix, UIClass, UIVolume);
    
    if (VoiceClass)
        UGameplayStatics::SetSoundMixClassOverride(World, GameMix, VoiceClass, VoiceVolume);

    // Push the Sound Mix
    UGameplayStatics::PushSoundMixModifier(World, GameMix);
    UE_LOG(LogTemp, Log, TEXT("Audio settings applied - Master: %.2f, Music: %.2f, SFX: %.2f, UI: %.2f, Voice: %.2f"), MasterVolume, MusicVolume, SFXVolume, UIVolume, VoiceVolume);
}


// ==================== Input Settings ====================

void UML_GameUserSettings::SetMouseSensitivity(float Sensitivity)
{
	MouseSensitivity = FMath::Max(Sensitivity, 0.1f);
}

void UML_GameUserSettings::SetGamepadSensitivity(float Sensitivity)
{
	GamepadSensitivity = FMath::Max(Sensitivity, 0.1f);
}

void UML_GameUserSettings::SetGamepadDeadZone(float DeadZone)
{
	GamepadDeadZone = FMath::Clamp(DeadZone, 0.0f, 0.9f);
}


// ==================== Graphics Helpers ====================

void UML_GameUserSettings::SetGraphicsPreset(int32 Preset)
{
	// 0 = Low, 1 = Medium, 2 = High, 3 = Ultra
	SetOverallScalabilityLevel(FMath::Clamp(Preset, 0, 3));
}

void UML_GameUserSettings::SetResolutionScale(float Scale)
{
	SetResolutionScaleValueEx(FMath::Clamp(Scale, 10.0f, 100.0f));
}


// ==================== Overrides ====================

void UML_GameUserSettings::ApplySettings(bool bCheckForCommandLineOverrides)
{
	Super::ApplySettings(bCheckForCommandLineOverrides);
	
	ApplyAudioSettings();
}

void UML_GameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	
	// Reset custom settings
	InitValues();
}

void UML_GameUserSettings::LoadSettings(bool bForceReload)
{
	Super::LoadSettings(bForceReload);
	
	ApplyAudioSettings();
}
