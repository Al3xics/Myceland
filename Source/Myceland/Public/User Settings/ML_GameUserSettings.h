// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "ML_GameUserSettings.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingsLoaded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnControlsSettingsApplied);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAccessibilitySettingsApplied);

/** Scalability preset, mirrors Unreal's overall scalability levels (0-4). */
UENUM(BlueprintType)
enum class EMLQualityLevel : uint8
{
	Low,
	Medium,
	High,
	Epic,
	Cinematic
};

UENUM(BlueprintType)
enum class EMLColorblindMode : uint8
{
	None,
	Protanopia,
	Deuteranopia,
	Tritanopia
};

UENUM(BlueprintType)
enum class EMLSettingCategory : uint8
{
	Graphics,
	Audio,
	Controls,
	Accessibility
};

UCLASS()
class MYCELAND_API UML_GameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

private:
	// ==================== Default values ====================

	static inline const FIntPoint DefaultResolutionPx = FIntPoint(1920, 1080);
	static constexpr int32 DefaultResolutionValue = 2;
	static constexpr float DefaultResolutionScale = 70.0f;
	static constexpr EWindowMode::Type DefaultWindowMode = EWindowMode::Windowed;
	static constexpr bool DefaultVSync = false;
	static constexpr int32 DefaultFrameLimit = 1;
	static constexpr float DefaultFrameRateLimit = 60.0f;
	static constexpr EMLQualityLevel DefaultQualityLevel = EMLQualityLevel::Medium;
	static constexpr float DefaultMasterVolume = 100.0f;
	static constexpr float DefaultMusicVolume = 100.0f;
	static constexpr float DefaultSFXVolume = 100.0f;
	static constexpr float DefaultVoiceVolume = 100.0f;
	static constexpr float DefaultGamepadDeadZone = 0.2f;
	static constexpr float DefaultHoldRepeatDelay = 0.3f;
	static constexpr float DefaultHoldRepeatInterval = 0.1f;
	static constexpr bool DefaultSubtitles = true;
	static constexpr float DefaultSubtitlesSize = 18.0f;
	static constexpr EMLColorblindMode DefaultColorblindMode = EMLColorblindMode::None;



	// ==================== Graphics Variables (Wrappers) ====================

	/*
	 * Note: These properties are wrappers for Unreal's native settings.
	 * They are NOT saved in the .ini file (no Config).
	 * They are synchronized from the actual values in LoadSettings().
	 * BlueprintReadOnly + AllowPrivateAccess: WB_Graphics reads them to sync its widgets.
	 */

	FIntPoint ResolutionPx = DefaultResolutionPx; // Actual resolution in pixel
	float FrameRLimit = 0.0f; // Actual frame rate limit

	// Whitelists, filled from the DeveloperSettings (EnsureValidListsInitialized)
	static TArray<FIntPoint> ValidResolutions;
	static TArray<float> ValidFrameLimits;

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics", meta = (AllowPrivateAccess = "true"))
	int32 ResolutionValue = DefaultResolutionValue; // Index in the component

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics", meta = (AllowPrivateAccess = "true"))
	float ResolutionScale = DefaultResolutionScale; // 0-100

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<EWindowMode::Type> WindowMode = DefaultWindowMode;

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics", meta = (AllowPrivateAccess = "true"))
	bool bVSync = DefaultVSync;

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics", meta = (AllowPrivateAccess = "true"))
	int32 FrameLimit = DefaultFrameLimit; // Index in the component (0=30, 1=60, 2=120, 3=144, 4=Unlimited...)

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics", meta = (AllowPrivateAccess = "true"))
	EMLQualityLevel OverallQuality = DefaultQualityLevel;



	// ==================== Audio Variables ====================

	UPROPERTY(Config)
	float MasterVolume = DefaultMasterVolume;

	UPROPERTY(Config)
	float MusicVolume = DefaultMusicVolume;

	UPROPERTY(Config)
	float SFXVolume = DefaultSFXVolume;

	UPROPERTY(Config)
	float VoiceVolume = DefaultVoiceVolume;



	// ==================== Controls Variables ====================

	UPROPERTY(Config)
	float GamepadDeadZone = DefaultGamepadDeadZone;

	UPROPERTY(Config)
	float GamepadHoldRepeatDelay = DefaultHoldRepeatDelay;

	UPROPERTY(Config)
	float GamepadHoldRepeatInterval = DefaultHoldRepeatInterval;



	// ==================== Accessibility Variables ====================

	UPROPERTY(Config)
	bool bSubtitles = DefaultSubtitles;

	UPROPERTY(Config)
	float SubtitlesSize = DefaultSubtitlesSize;

	UPROPERTY(Config)
	EMLColorblindMode ColorblindMode = DefaultColorblindMode;



	// ==================== Internal Helpers ====================

	UWorld* GetWorld() const;
	FIntPoint GetClosestValidResolution(FIntPoint DesiredResolution) const;

	void LoadResolution();
	void LoadFrameLimit();
	void InitValues();

	static bool AreValidResolutionsInitialized();
	static bool AreValidFrameLimitsInitialized();

	// Fills the whitelists from the DeveloperSettings the first time they are needed
	static void EnsureValidListsInitialized();
	static float GetClosestValidFrameLimit(float DesiredLimit);

	// Called automatically by the setters and ApplyMycelandSetting; Blueprints never call these directly
	void ApplyGraphicsSettings();
	void ApplyAudioSettings();
	void ApplyControlsSettings();
	void ApplyAccessibilitySettings();

public:
	UML_GameUserSettings();

	// ==================== Singleton Access ====================

	UFUNCTION(BlueprintPure, Category = "Settings")
	static UML_GameUserSettings* GetMycelandGameUserSettings()
	{
		return Cast<UML_GameUserSettings>(UGameUserSettings::GetGameUserSettings());
	}



	// ==================== Delegates ====================

	UPROPERTY(BlueprintAssignable, Category = "Settings|Events")
	FOnSettingsLoaded OnSettingsLoaded;

	/** Fired by ApplyControlsSettings. Bind input code that caches dead zone or hold repeat values. */
	UPROPERTY(BlueprintAssignable, Category = "Settings|Events")
	FOnControlsSettingsApplied OnControlsSettingsApplied;

	/** Fired by ApplyAccessibilitySettings. Bind subtitle widgets, colorblind post-process, etc. */
	UPROPERTY(BlueprintAssignable, Category = "Settings|Events")
	FOnAccessibilitySettingsApplied OnAccessibilitySettingsApplied;



	// ==================== Helpers ====================

	UFUNCTION(BlueprintPure, Category = "Settings|Helper")
	static float Normalize(float Value, float Min, float Max);

	UFUNCTION(BlueprintPure, Category = "Settings|Helper")
	static float Denormalize(float Normalized, float Min, float Max);



	// ==================== Graphics Setters (from widgets) ====================

	/**
	 * Options for the resolution dropdown, from the DeveloperSettings whitelist (e.g. "1920 x 1080").
	 * Display only: these are localized, so never parse them back. Index into them with SetResolutionFromIndex.
	 */
	UFUNCTION(BlueprintPure, Category = "Settings|Graphics")
	static TArray<FText> GetValidResolutionTexts();

	/**
	 * Options for the frame limit dropdown, from the DeveloperSettings whitelist (e.g. "60", "Unlimited").
	 * Display only: these are localized, so never parse them back. Index into them with SetFrameRateLimitFromIndex.
	 */
	UFUNCTION(BlueprintPure, Category = "Settings|Graphics")
	static TArray<FText> GetValidFrameLimitTexts();

	/**
	 * Set resolution from the dropdown selection, indexing the same whitelist that filled
	 * GetValidResolutionTexts. Also updates the ResolutionValue index. Out-of-range values are ignored.
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void SetResolutionFromIndex(int32 Index);

	/**
	 * Set frame rate limit from the dropdown selection, indexing the same whitelist that filled
	 * GetValidFrameLimitTexts. Also updates the FrameLimit index. Out-of-range values are ignored.
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void SetFrameRateLimitFromIndex(int32 Index);

	/**
	 * Set resolution scale from slider value (0-100)
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void SetResolutionScaleFromSlider(const float SliderValue) { ResolutionScale = SliderValue; }

	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void SetWindowMode(const EWindowMode::Type Mode) { WindowMode = Mode; }

	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void SetVSyncFromToggle(const bool bEnabled) { bVSync = bEnabled; }

	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void SetOverallQuality(const EMLQualityLevel Quality) { OverallQuality = Quality; }



	// ==================== Audio Setters / Getters ====================

	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void SetMasterVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void SetMusicVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void SetSFXVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void SetVoiceVolume(float Volume);

	UFUNCTION(BlueprintPure, Category = "Settings|Audio")
	float GetMasterVolume() const { return MasterVolume; }

	UFUNCTION(BlueprintPure, Category = "Settings|Audio")
	float GetMusicVolume() const { return MusicVolume; }

	UFUNCTION(BlueprintPure, Category = "Settings|Audio")
	float GetSFXVolume() const { return SFXVolume; }

	UFUNCTION(BlueprintPure, Category = "Settings|Audio")
	float GetVoiceVolume() const { return VoiceVolume; }



	// ==================== Controls Setters / Getters ====================

	UFUNCTION(BlueprintCallable, Category = "Settings|Controls")
	void SetGamepadDeadZone(float DeadZone);

	UFUNCTION(BlueprintCallable, Category = "Settings|Controls")
	void SetGamepadHoldRepeatDelay(float Delay);

	UFUNCTION(BlueprintCallable, Category = "Settings|Controls")
	void SetGamepadHoldRepeatInterval(float Interval);

	UFUNCTION(BlueprintPure, Category = "Settings|Controls")
	float GetGamepadDeadZone() const { return GamepadDeadZone; }

	UFUNCTION(BlueprintPure, Category = "Settings|Controls")
	float GetGamepadHoldRepeatDelay() const { return GamepadHoldRepeatDelay; }

	UFUNCTION(BlueprintPure, Category = "Settings|Controls")
	float GetGamepadHoldRepeatInterval() const { return GamepadHoldRepeatInterval; }



	// ==================== Accessibility Setters / Getters ====================

	UFUNCTION(BlueprintCallable, Category = "Settings|Accessibility")
	void SetSubtitles(bool bEnable);

	UFUNCTION(BlueprintCallable, Category = "Settings|Accessibility")
	void SetSubtitlesSize(float Size);

	UFUNCTION(BlueprintCallable, Category = "Settings|Accessibility")
	void SetColorblindMode(EMLColorblindMode Mode);

	UFUNCTION(BlueprintPure, Category = "Settings|Accessibility")
	bool GetSubtitles() const { return bSubtitles; }

	UFUNCTION(BlueprintPure, Category = "Settings|Accessibility")
	float GetSubtitlesSize() const { return SubtitlesSize; }

	UFUNCTION(BlueprintPure, Category = "Settings|Accessibility")
	EMLColorblindMode GetColorblindMode() const { return ColorblindMode; }



	// ==================== Per-Category Apply / Reset ====================

	/**
	 * Apply every setting of the given category, then save to disk.
	 * Call this when a value is committed (slider released, option selected, toggle clicked).
	 * Graphics are normally batched behind the Apply button instead.
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void ApplyMycelandSetting(EMLSettingCategory Setting);

	/**
	 * Reset every setting of the given category to its default value.
	 * Non-graphics categories are applied and saved immediately.
	 * Graphics only update the pending values: press Apply to take effect.
	 * Broadcasts OnSettingsLoaded so the UI can refresh its widgets.
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void ResetMycelandSettingToDefault(EMLSettingCategory Setting);



	// ==================== Overrides ====================

	/**
	 * Snaps out-of-whitelist saved values (first run, monitor change, hand-edited ini).
	 * The engine calls this at boot before creating the game window and before every apply,
	 * so the game always starts on a whitelisted resolution / frame limit.
	 */
	virtual void ValidateSettings() override;
	virtual void ApplySettings(bool bCheckForCommandLineOverrides) override;
	virtual void SetToDefaults() override;
	virtual void LoadSettings(bool bForceReload = false) override;
};
