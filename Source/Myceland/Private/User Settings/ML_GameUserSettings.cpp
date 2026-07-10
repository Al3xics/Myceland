// Copyright Myceland Team, All Rights Reserved.


#include "User Settings/ML_GameUserSettings.h"

#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "FMODStudioModule.h"
#include "FMOD/fmod_studio.hpp"

TArray<FIntPoint> UML_GameUserSettings::ValidResolutions;
TArray<float> UML_GameUserSettings::ValidFrameLimits;

namespace
{
	void SetFMODVCAVolume(const FString& VCAPath, const float Volume01)
	{
		if (VCAPath.IsEmpty() || !IFMODStudioModule::IsAvailable())
			return;

		FMOD::Studio::System* StudioSystem = IFMODStudioModule::Get().GetStudioSystem(EFMODSystemContext::Runtime);
		if (!StudioSystem)
			return;

		FMOD::Studio::VCA* VCA = nullptr;
		const FMOD_RESULT FindResult = StudioSystem->getVCA(TCHAR_TO_UTF8(*VCAPath), &VCA);
		if (FindResult != FMOD_OK || !VCA)
		{
			UE_LOG(LogTemp, Warning, TEXT("FMOD VCA not found: %s. Check FMOD VCA names, build banks, then refresh FMOD assets."), *VCAPath);
			return;
		}

		const float ClampedVolume = FMath::Clamp(Volume01, 0.0f, 1.0f);
		const FMOD_RESULT VolumeResult = VCA->setVolume(ClampedVolume);
		if (VolumeResult != FMOD_OK)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to set FMOD VCA volume: %s"), *VCAPath);
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("FMOD VCA volume applied - %s: %.2f"), *VCAPath, ClampedVolume);
	}
}

UWorld* UML_GameUserSettings::GetWorld() const
{
	if (!GEngine)
		return nullptr;

	// Try to get from GameInstance
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
			return Context.World();
	}

	return nullptr;
}

FIntPoint UML_GameUserSettings::GetClosestValidResolution(FIntPoint DesiredResolution) const
{
	if (ValidResolutions.Num() == 0)
		return FIntPoint(1920, 1080);
	
	FIntPoint ClosestResolution = ValidResolutions[0];
	float ClosestDistance = FLT_MAX;
	
	for (const FIntPoint& ValidRes : ValidResolutions)
	{
		// Calculate distance (using area difference as metric)
		const float DesiredArea = DesiredResolution.X * DesiredResolution.Y;
		const float ValidArea = ValidRes.X * ValidRes.Y;
		const float Distance = FMath::Abs(DesiredArea - ValidArea);
        
		if (Distance < ClosestDistance)
		{
			ClosestDistance = Distance;
			ClosestResolution = ValidRes;
		}
	}
    
	return ClosestResolution;
}

bool UML_GameUserSettings::ParseResolutionText(const FString& ResolutionText, FIntPoint& OutResolution)
{
	// Normalize the input: trim whitespace and uppercase
	const FString NormalizedText = ResolutionText.TrimStartAndEnd().ToUpper();
	
	// Split on "X" (now guaranteed uppercase, no spaces)
	FString LeftStr, RightStr;
	if (NormalizedText.Split(TEXT("X"), &LeftStr, &RightStr))
	{
		// Trim any remaining whitespace on each side
		LeftStr = LeftStr.TrimStartAndEnd();
		RightStr = RightStr.TrimStartAndEnd();
		
		const int32 Width = FCString::Atoi(*LeftStr);
		const int32 Height = FCString::Atoi(*RightStr);
		
		// Validate
		if (Width > 0 && Height > 0)
		{
			OutResolution = FIntPoint(Width, Height);
			return true;
		}
	}
	
	return false;
}

bool UML_GameUserSettings::ParseFrameLimitText(const FString& LimitText, float& OutFrameLimit)
{
	const FString LimitString = LimitText.TrimStartAndEnd();

	// Parse "Unlimited" or similar → 0.0f
	if (LimitString.Equals(TEXT("Unlimited"), ESearchCase::IgnoreCase))
		OutFrameLimit = 0.0f;
	else
	{
		// Parse numeric value (e.g., "30", "60", "120", "144")
		const float ParsedLimit = static_cast<float>(FCString::Atoi(*LimitString));
		if (ParsedLimit > 0.0f)
			OutFrameLimit = ParsedLimit;
		else
			return false;
	}
	
	return true;
}

void UML_GameUserSettings::LoadResolution()
{
	// Snap to the closest valid resolution
	const FIntPoint NativeResolution = Super::GetScreenResolution();
	if (AreValidResolutionsInitialized())
	{
		if (bool bIsValid = ValidResolutions.Contains(NativeResolution))
			ResolutionPx = NativeResolution;
		else
		{
			// Snap to closest valid
			ResolutionPx = GetClosestValidResolution(NativeResolution);
			Super::SetScreenResolution(ResolutionPx);
		}
	}
	else
		ResolutionPx = NativeResolution;
}

void UML_GameUserSettings::LoadFrameLimit()
{
	if (!AreValidFrameLimitsInitialized()) return;
	
	FrameLimit = ValidFrameLimits.IndexOfByPredicate(
		[this](float Value)
		{
			if (Value == 0.f && FrameRLimit == 0.f)
				return true;
			return FMath::IsNearlyEqual(Value, FrameRLimit);
		}
	);

	if (FrameLimit == INDEX_NONE)
		FrameLimit = 0;
}

UML_GameUserSettings::UML_GameUserSettings()
{
}

void UML_GameUserSettings::InitValues()
{
	// Graphics (wrappers - defaults)
	ResolutionValue = 2;
	ResolutionPx = FIntPoint(1920, 1080);
	ResolutionScale = 100.0f;
	WindowMode = 2;
	bVSync = false;
	FrameLimit = 1;
	FrameRLimit = 60.0f;
	OverallQuality = 1;
	TextureQuality = 1;
	ShadowQuality = 1;
	EffectsQuality = 1;
	PostProcessingQuality = 1;
	GlobalIlluminationQuality = 1;
	
	// Audio
	MasterVolume = 100.0f;
	MusicVolume = 100.0f;
	SFXVolume = 100.0f;
	VoiceVolume = 100.0f;
	
	// Controls
	MouseSensitivity = 0.5f;
	GamepadSensitivity = 0.5f;
	GamepadDeadZone = 0.2f;
	
	// Gameplay
	bShowGrid = true;
	bTileHighlight = true;
	HighlightIntensity = 1.0f;
	bPropagationPreview = true;
	bUndoConfirmation = false;
	bResetConfirmation = true;
	bTutorialHints = true;
	
	// Accessibility
	bSubtitles = false;
	SubtitlesSize = 50.0f;
	ColorblindMode = 0;
}

float UML_GameUserSettings::Normalize(float Value, float Min, float Max)
{
	if (Max <= Min) return 0.f;
	return (Value - Min) / (Max - Min);
}

float UML_GameUserSettings::Denormalize(float Normalized, float Min, float Max)
{
	if (Max <= Min) return Min;
	return Min + Normalized * (Max - Min);
}

void UML_GameUserSettings::SetMasterVolume(const float Volume)
{
	MasterVolume = FMath::Clamp(Volume, 0.0f, 100.0f);
	ApplyAudioSettings();
}

void UML_GameUserSettings::SetMusicVolume(const float Volume)
{
	MusicVolume = FMath::Clamp(Volume, 0.0f, 100.0f);
	ApplyAudioSettings();
}

void UML_GameUserSettings::SetSFXVolume(const float Volume)
{
	SFXVolume = FMath::Clamp(Volume, 0.0f, 100.0f);
	ApplyAudioSettings();
}

void UML_GameUserSettings::SetVoiceVolume(const float Volume)
{
	VoiceVolume = FMath::Clamp(Volume, 0.0f, 100.0f);
	ApplyAudioSettings();
}


// ==================== Graphics Settings (Wrappers) ====================

void UML_GameUserSettings::SetValidResolutions(const TArray<FText>& Resolutions)
{
	for (const FText& ResText : Resolutions)
	{
		FIntPoint ParsedResolution;
		if (ParseResolutionText(ResText.ToString(), ParsedResolution))
			ValidResolutions.AddUnique(ParsedResolution);
	}
}

bool UML_GameUserSettings::AreValidResolutionsInitialized()
{
	return ValidResolutions.Num() > 0;
}

void UML_GameUserSettings::SetValidFrameLimits(const TArray<FText>& Limits)
{
	for (const FText& LimitText : Limits)
	{
		float ParsedLimit;
		if (ParseFrameLimitText(LimitText.ToString(), ParsedLimit))
			ValidFrameLimits.AddUnique(ParsedLimit);
	}
}

bool UML_GameUserSettings::AreValidFrameLimitsInitialized()
{
	return ValidFrameLimits.Num() > 0;
}

void UML_GameUserSettings::SetResolutionFromText(const FString& ResolutionText)
{
	FIntPoint RequestedResolution;
	if (ParseResolutionText(ResolutionText, RequestedResolution))
	{
		ResolutionPx = GetClosestValidResolution(RequestedResolution);
		ResolutionValue = ValidResolutions.IndexOfByKey(ResolutionPx);
	}
}

void UML_GameUserSettings::SetFrameRateLimitFromText(const FString& FrameRateText)
{
	// Parse "60" → 60
	// Parse "Unlimited" → 0
	if (FrameRateText.Equals(TEXT("Unlimited"), ESearchCase::IgnoreCase))
		FrameRLimit = 0;
	else
		FrameRLimit = static_cast<float>(FCString::Atoi(*FrameRateText));
	
	LoadFrameLimit();
}

FString UML_GameUserSettings::GetCurrentResolutionText() const
{
	return FString::Printf(TEXT("%d x %d"), ResolutionPx.X, ResolutionPx.Y);
}

FString UML_GameUserSettings::GetCurrentFrameRateLimitText() const
{
	if (FrameRLimit <= 0.0f)
		return TEXT("Unlimited");
	
	return FString::Printf(TEXT("%.0f"), FrameRLimit);
}


// ==================== Apply Methods ====================

void UML_GameUserSettings::ApplyGraphicsSettings()
{
	Super::SetScreenResolution(ResolutionPx);
	Super::SetResolutionScaleValueEx(ResolutionValue);
	Super::SetFullscreenMode(static_cast<EWindowMode::Type>(WindowMode));
	Super::SetVSyncEnabled(bVSync);
	Super::SetFrameRateLimit(FrameRLimit);
	
	Super::SetOverallScalabilityLevel(OverallQuality);
	Super::SetTextureQuality(TextureQuality);
	Super::SetShadowQuality(ShadowQuality);
	Super::SetVisualEffectQuality(EffectsQuality);
	Super::SetPostProcessingQuality(PostProcessingQuality);
	Super::SetGlobalIlluminationQuality(GlobalIlluminationQuality);
}

void UML_GameUserSettings::ApplyAudioSettings()
{
	const UML_MycelandDeveloperSettings* DevSettings = UML_MycelandDeveloperSettings::GetMycelandDeveloperSettings();
	if (!ensureMsgf(DevSettings, TEXT("Failed to get MycelandDeveloperSettings for audio")))
		return;

	SetFMODVCAVolume(DevSettings->MasterFMODVCAPath, Normalize(MasterVolume, 0.0f, 100.0f));
	SetFMODVCAVolume(DevSettings->MusicFMODVCAPath, Normalize(MusicVolume, 0.0f, 100.0f));
	SetFMODVCAVolume(DevSettings->SFXFMODVCAPath, Normalize(SFXVolume, 0.0f, 100.0f));
	SetFMODVCAVolume(DevSettings->VoiceFMODVCAPath, Normalize(VoiceVolume, 0.0f, 100.0f));
	
	UE_LOG(LogTemp, Log, TEXT("Audio settings applied - Master: %.2f, Music: %.2f, SFX: %.2f, Voice: %.2f"), MasterVolume, MusicVolume, SFXVolume, VoiceVolume);
}

void UML_GameUserSettings::ApplyGameplaySettings()
{
	// Broadcast events to update gameplay in real-time
	// You can use a delegate/event system here to notify the game
	// Example: OnGameplaySettingsChanged.Broadcast();
	
	UE_LOG(LogTemp, Log, TEXT("Gameplay settings applied - Grid: %s, Highlight: %s, Intensity: %.2f"),
		bShowGrid ? TEXT("On") : TEXT("Off"),
		bTileHighlight ? TEXT("On") : TEXT("Off"),
		HighlightIntensity);
}

void UML_GameUserSettings::ApplyAccessibilitySettings()
{
	// Apply colorblind mode via post-process or material parameter collection
	// Example: SetScalarParameter on a MPC to change game colors
	
	UE_LOG(LogTemp, Log, TEXT("Accessibility settings applied - Subtitles: %s, Size: %.2f, Colorblind: %d"),
		bSubtitles ? TEXT("On") : TEXT("Off"),
		SubtitlesSize,
		ColorblindMode);
}


// ==================== Overrides ====================

void UML_GameUserSettings::ApplySettings(bool bCheckForCommandLineOverrides)
{
	// Apply all custom settings
	ApplyGraphicsSettings();
	ApplyAudioSettings();
	ApplyGameplaySettings();
	ApplyAccessibilitySettings();
	
	// This will apply resolution and non-resolution settings
	// THEN it will SaveSettings()
	Super::ApplySettings(bCheckForCommandLineOverrides);
}

void UML_GameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	
	// Reset custom settings to default values
	InitValues();
	
	ApplyGraphicsSettings();
	ApplyAudioSettings();
	ApplyGameplaySettings();
	ApplyAccessibilitySettings();
	
	SaveSettings();
	
	OnSettingsLoaded.Broadcast();
}

void UML_GameUserSettings::LoadSettings(bool bForceReload)
{
	// This will LoadConfig() from .ini file
	Super::LoadSettings(bForceReload);
	
	LoadResolution();
	// ResolutionScale = FMath::Clamp(Super::GetResolutionScaleNormalized() * 100.0f, 1.0f, 100.0f);
	ResolutionScale = ScalabilityQuality.ResolutionQuality;
	WindowMode = static_cast<int32>(Super::GetFullscreenMode());
	bVSync = Super::IsVSyncEnabled();
	FrameRLimit = Super::GetFrameRateLimit();
	LoadFrameLimit();
	
	OverallQuality = FMath::Clamp(Super::GetOverallScalabilityLevel(), 0, 4);
	TextureQuality = Super::GetTextureQuality();
	ShadowQuality = Super::GetShadowQuality();
	EffectsQuality = Super::GetVisualEffectQuality();
	PostProcessingQuality = Super::GetPostProcessingQuality();
	GlobalIlluminationQuality = Super::GetGlobalIlluminationQuality();
	
	ApplyAudioSettings();
	ApplyGameplaySettings();
	ApplyAccessibilitySettings();
	
	OnSettingsLoaded.Broadcast();
}
