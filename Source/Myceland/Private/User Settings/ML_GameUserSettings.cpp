// Copyright Myceland Team, All Rights Reserved.


#include "User Settings/ML_GameUserSettings.h"

#include "Developer Settings/ML_MycelandDeveloperSettings.h"
#include "FMODStudioModule.h"
#include "FMOD/fmod_studio.hpp"

#define LOCTEXT_NAMESPACE "MycelandSettings"

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

void UML_GameUserSettings::LoadResolution()
{
	EnsureValidListsInitialized();

	// Snap the wrapper to the closest valid resolution for the UI.
	// Load only pulls state: the snapped value is NOT pushed to the engine here
	// (ValidateSettings handles out-of-whitelist values at boot / on apply).
	const FIntPoint NativeResolution = Super::GetScreenResolution();
	if (AreValidResolutionsInitialized() && !ValidResolutions.Contains(NativeResolution))
		ResolutionPx = GetClosestValidResolution(NativeResolution);
	else
		ResolutionPx = NativeResolution;

	// Keep the dropdown index in step with the resolution the UI reads back,
	// mirroring LoadFrameLimit.
	if (AreValidResolutionsInitialized())
	{
		ResolutionValue = ValidResolutions.IndexOfByKey(ResolutionPx);
		if (ResolutionValue == INDEX_NONE)
			ResolutionValue = DefaultResolutionValue;
	}
}

void UML_GameUserSettings::LoadFrameLimit()
{
	EnsureValidListsInitialized();

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
	ResolutionValue = DefaultResolutionValue;
	ResolutionPx = DefaultResolutionPx;
	ResolutionScale = DefaultResolutionScale;
	WindowMode = DefaultWindowMode;
	bVSync = DefaultVSync;
	FrameLimit = DefaultFrameLimit;
	FrameRLimit = DefaultFrameRateLimit;
	OverallQuality = DefaultQualityLevel;

	// Audio
	MasterVolume = DefaultMasterVolume;
	MusicVolume = DefaultMusicVolume;
	SFXVolume = DefaultSFXVolume;
	VoiceVolume = DefaultVoiceVolume;

	// Controls
	// Hold repeat defaults live in the DeveloperSettings: the designer tunes them there,
	// the player overrides them here, and reset picks the designer values back up.
	const UML_MycelandDeveloperSettings* DevSettings = UML_MycelandDeveloperSettings::GetMycelandDeveloperSettings();
	GamepadDeadZone = DefaultGamepadDeadZone;
	GamepadHoldRepeatDelay = DevSettings ? DevSettings->GamepadHoldRepeatDelay : DefaultHoldRepeatDelay;
	GamepadHoldRepeatInterval = DevSettings ? DevSettings->GamepadHoldRepeatInterval : DefaultHoldRepeatInterval;

	// Accessibility
	bSubtitles = DefaultSubtitles;
	SubtitlesSize = DefaultSubtitlesSize;
	ColorblindMode = DefaultColorblindMode;
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


// ==================== Audio Settings ====================

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


// ==================== Controls Settings ====================

void UML_GameUserSettings::SetGamepadDeadZone(const float DeadZone)
{
	GamepadDeadZone = DeadZone;
	ApplyControlsSettings();
}

void UML_GameUserSettings::SetGamepadHoldRepeatDelay(const float Delay)
{
	GamepadHoldRepeatDelay = Delay;
	ApplyControlsSettings();
}

void UML_GameUserSettings::SetGamepadHoldRepeatInterval(const float Interval)
{
	GamepadHoldRepeatInterval = Interval;
	ApplyControlsSettings();
}


// ==================== Accessibility Settings ====================

void UML_GameUserSettings::SetSubtitles(const bool bEnable)
{
	bSubtitles = bEnable;
	ApplyAccessibilitySettings();
}

void UML_GameUserSettings::SetSubtitlesSize(const float Size)
{
	SubtitlesSize = Size;
	ApplyAccessibilitySettings();
}

void UML_GameUserSettings::SetColorblindMode(const EMLColorblindMode Mode)
{
	ColorblindMode = Mode;
	ApplyAccessibilitySettings();
}


// ==================== Graphics Settings (Wrappers) ====================

bool UML_GameUserSettings::AreValidResolutionsInitialized()
{
	return ValidResolutions.Num() > 0;
}

bool UML_GameUserSettings::AreValidFrameLimitsInitialized()
{
	return ValidFrameLimits.Num() > 0;
}

void UML_GameUserSettings::EnsureValidListsInitialized()
{
	if (AreValidResolutionsInitialized() && AreValidFrameLimitsInitialized())
		return;

	const UML_MycelandDeveloperSettings* DevSettings = UML_MycelandDeveloperSettings::GetMycelandDeveloperSettings();
	if (!ensureMsgf(DevSettings, TEXT("Failed to get MycelandDeveloperSettings for the graphics whitelists")))
		return;

	for (const FIntPoint& Resolution : DevSettings->ValidResolutions)
		ValidResolutions.AddUnique(Resolution);

	for (const float Limit : DevSettings->ValidFrameLimits)
		ValidFrameLimits.AddUnique(Limit);
}

TArray<FText> UML_GameUserSettings::GetValidResolutionTexts()
{
	EnsureValidListsInitialized();

	// Digits are formatted per culture, but never grouped: a resolution is an identifier,
	// so "1920" must not become "1,920".
	FNumberFormattingOptions NumberFormat;
	NumberFormat.SetUseGrouping(false);

	TArray<FText> Texts;
	for (const FIntPoint& Resolution : ValidResolutions)
	{
		Texts.Add(FText::Format(
			LOCTEXT("ResolutionOptionFormat", "{Width} x {Height}"),
			FFormatNamedArguments{
				{TEXT("Width"), FText::AsNumber(Resolution.X, &NumberFormat)},
				{TEXT("Height"), FText::AsNumber(Resolution.Y, &NumberFormat)}
			}));
	}
	return Texts;
}

TArray<FText> UML_GameUserSettings::GetValidFrameLimitTexts()
{
	EnsureValidListsInitialized();

	FNumberFormattingOptions NumberFormat;
	NumberFormat.SetUseGrouping(false);
	NumberFormat.SetMaximumFractionalDigits(0);

	TArray<FText> Texts;
	for (const float Limit : ValidFrameLimits)
	{
		Texts.Add(Limit <= 0.f
			? LOCTEXT("FrameLimitUnlimited", "Unlimited")
			: FText::AsNumber(Limit, &NumberFormat));
	}
	return Texts;
}

float UML_GameUserSettings::GetClosestValidFrameLimit(const float DesiredLimit)
{
	if (ValidFrameLimits.Num() == 0)
		return DesiredLimit;

	// 0 means Unlimited: exact match only. An unlimited request without an
	// Unlimited entry snaps to the highest limit, not the numerically closest.
	if (DesiredLimit <= 0.f)
	{
		if (ValidFrameLimits.Contains(0.f))
			return 0.f;

		float Highest = 0.f;
		for (const float Limit : ValidFrameLimits)
			Highest = FMath::Max(Highest, Limit);
		return Highest;
	}

	float Closest = DesiredLimit;
	float ClosestDistance = FLT_MAX;
	for (const float Limit : ValidFrameLimits)
	{
		if (Limit <= 0.f)
			continue;

		const float Distance = FMath::Abs(Limit - DesiredLimit);
		if (Distance < ClosestDistance)
		{
			ClosestDistance = Distance;
			Closest = Limit;
		}
	}
	return Closest;
}

void UML_GameUserSettings::SetResolutionFromIndex(const int32 Index)
{
	EnsureValidListsInitialized();

	// The index comes straight from the dropdown, which was filled by GetValidResolutionTexts:
	// same order, same source list, so it needs no snapping — only a range check.
	if (!ValidResolutions.IsValidIndex(Index))
	{
		UE_LOG(LogTemp, Warning, TEXT("SetResolutionFromIndex: index %d out of range (%d options)"), Index, ValidResolutions.Num());
		return;
	}

	ResolutionValue = Index;
	ResolutionPx = ValidResolutions[Index];
}

void UML_GameUserSettings::SetFrameRateLimitFromIndex(const int32 Index)
{
	EnsureValidListsInitialized();

	if (!ValidFrameLimits.IsValidIndex(Index))
	{
		UE_LOG(LogTemp, Warning, TEXT("SetFrameRateLimitFromIndex: index %d out of range (%d options)"), Index, ValidFrameLimits.Num());
		return;
	}

	FrameLimit = Index;
	FrameRLimit = ValidFrameLimits[Index];
}


// ==================== Apply Methods ====================

void UML_GameUserSettings::ApplyGraphicsSettings()
{
	Super::SetScreenResolution(ResolutionPx);
	Super::SetFullscreenMode(WindowMode);
	Super::SetVSyncEnabled(bVSync);
	Super::SetFrameRateLimit(FrameRLimit);

	// Overall quality FIRST: SetFromSingleQualityLevel overwrites ResolutionQuality with
	// the preset's value, so the explicit resolution scale must be applied after it to
	// keep both settings independent.
	Super::SetOverallScalabilityLevel(static_cast<int32>(OverallQuality));
	Super::SetResolutionScaleValueEx(ResolutionScale);
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

void UML_GameUserSettings::ApplyControlsSettings()
{
	// Controls are pull-based: the input code reads sensitivity, dead zone and hold repeat
	// through the getters when it needs them. The broadcast lets systems that cache these
	// values (e.g. an input component configured once) refresh themselves.
	OnControlsSettingsApplied.Broadcast();

	UE_LOG(LogTemp, Verbose, TEXT("Controls settings applied - DeadZone: %.2f, HoldDelay: %.2f, HoldInterval: %.2f"),
		GamepadDeadZone, GamepadHoldRepeatDelay, GamepadHoldRepeatInterval);
}

void UML_GameUserSettings::ApplyAccessibilitySettings()
{
	// Accessibility is pull-based too: subtitle widgets read bSubtitles/SubtitlesSize when they
	// display a line. The broadcast lets already-visible widgets and the colorblind post-process
	// (once it exists) react immediately instead of waiting for the next read.
	OnAccessibilitySettingsApplied.Broadcast();

	UE_LOG(LogTemp, Log, TEXT("Accessibility settings applied - Subtitles: %s, Size: %.2f, Colorblind: %s"),
		bSubtitles ? TEXT("On") : TEXT("Off"),
		SubtitlesSize,
		*UEnum::GetValueAsString(ColorblindMode));
}


// ==================== Per-Setting Apply / Reset ====================

void UML_GameUserSettings::ApplyMycelandSetting(const EMLSettingCategory Setting)
{
	switch (Setting)
	{
		case EMLSettingCategory::Graphics:
			// Graphics need the full engine apply: ApplyGraphicsSettings only writes the
			// pending values into the settings members. Super::ApplySettings (called by our
			// override) is what actually pushes them to the engine — resolution change and
			// Scalability::SetQualityLevels for the scalability CVars — and then saves.
			// Without it, SaveSettings' Scalability::SaveState would persist the UNCHANGED
			// active CVars and the pending scalability/resolution scale would be lost.
			ApplySettings(false);
			return; // ApplySettings already saved

		case EMLSettingCategory::Audio:
			ApplyAudioSettings();
			break;

		case EMLSettingCategory::Controls:
			ApplyControlsSettings();
			break;

		case EMLSettingCategory::Accessibility:
			ApplyAccessibilitySettings();
			break;
	}

	SaveSettings();
}

void UML_GameUserSettings::ResetMycelandSettingToDefault(const EMLSettingCategory Setting)
{
	switch (Setting)
	{
		case EMLSettingCategory::Graphics:
			ResolutionPx = AreValidResolutionsInitialized() ? GetClosestValidResolution(DefaultResolutionPx) : DefaultResolutionPx;
			ResolutionValue = ValidResolutions.IndexOfByKey(ResolutionPx);
			if (ResolutionValue == INDEX_NONE) ResolutionValue = DefaultResolutionValue;
			ResolutionScale = DefaultResolutionScale;
			WindowMode = DefaultWindowMode;
			bVSync = DefaultVSync;
			FrameRLimit = DefaultFrameRateLimit;
			FrameLimit = DefaultFrameLimit;
			LoadFrameLimit(); // Recompute the index from the valid limits when initialized
			OverallQuality = DefaultQualityLevel;
			break;

		case EMLSettingCategory::Audio:
			MasterVolume = DefaultMasterVolume;
			MusicVolume = DefaultMusicVolume;
			SFXVolume = DefaultSFXVolume;
			VoiceVolume = DefaultVoiceVolume;
			break;

		case EMLSettingCategory::Controls:
		{
			// Hold repeat defaults come from the DeveloperSettings (designer tuning)
			const UML_MycelandDeveloperSettings* DevSettings = UML_MycelandDeveloperSettings::GetMycelandDeveloperSettings();
			GamepadDeadZone = DefaultGamepadDeadZone;
			GamepadHoldRepeatDelay = DevSettings ? DevSettings->GamepadHoldRepeatDelay : DefaultHoldRepeatDelay;
			GamepadHoldRepeatInterval = DevSettings ? DevSettings->GamepadHoldRepeatInterval : DefaultHoldRepeatInterval;
			break;
		}

		case EMLSettingCategory::Accessibility:
			bSubtitles = DefaultSubtitles;
			SubtitlesSize = DefaultSubtitlesSize;
			ColorblindMode = DefaultColorblindMode;
			break;
	}

	// Graphics stay pending until the Apply button; everything else takes effect and is saved now
	if (Setting != EMLSettingCategory::Graphics)
		ApplyMycelandSetting(Setting);

	OnSettingsLoaded.Broadcast();
}


// ==================== Overrides ====================

void UML_GameUserSettings::ValidateSettings()
{
	Super::ValidateSettings();

	EnsureValidListsInitialized();

	// Snap an out-of-whitelist saved resolution (first run, monitor change, hand-edited ini).
	// At boot the engine runs this before creating the game window, so the window opens
	// directly at the snapped resolution instead of the invalid saved one.
	if (AreValidResolutionsInitialized())
	{
		const FIntPoint SavedResolution = Super::GetScreenResolution();
		if (!ValidResolutions.Contains(SavedResolution))
		{
			const FIntPoint SnappedResolution = GetClosestValidResolution(SavedResolution);
			Super::SetScreenResolution(SnappedResolution);

			// Push the change to the system resolution outside the editor (no-op in PIE).
			if (!GIsEditor)
				RequestResolutionChange(SnappedResolution.X, SnappedResolution.Y, Super::GetFullscreenMode(), false);
		}
	}

	if (AreValidFrameLimitsInitialized())
	{
		const float SavedLimit = Super::GetFrameRateLimit();
		const float SnappedLimit = GetClosestValidFrameLimit(SavedLimit);
		if (!FMath::IsNearlyEqual(SavedLimit, SnappedLimit))
			Super::SetFrameRateLimit(SnappedLimit);
	}
}

void UML_GameUserSettings::ApplySettings(bool bCheckForCommandLineOverrides)
{
	// Apply all custom settings
	ApplyGraphicsSettings();
	ApplyAudioSettings();
	ApplyControlsSettings();
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
	ApplyControlsSettings();
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
	WindowMode = Super::GetFullscreenMode();
	bVSync = Super::IsVSyncEnabled();
	FrameRLimit = Super::GetFrameRateLimit();
	LoadFrameLimit();
	
	// GetOverallScalabilityLevel returns -1 (custom) when ResolutionQuality doesn't match
	// the preset's expected value — which is normal for us since the resolution scale is
	// independent. In that case derive the level from the scalability groups instead:
	// our UI only ever sets them all together, so any group is representative.
	const int32 OverallLevel = Super::GetOverallScalabilityLevel();
	OverallQuality = static_cast<EMLQualityLevel>(FMath::Clamp(
		OverallLevel != INDEX_NONE ? OverallLevel : ScalabilityQuality.ViewDistanceQuality, 0, 4));
	
	ApplyAudioSettings();
	ApplyControlsSettings();
	ApplyAccessibilitySettings();

	OnSettingsLoaded.Broadcast();
}

#undef LOCTEXT_NAMESPACE
