// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "ML_GameUserSettings.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingsLoaded);

UCLASS()
class MYCELAND_API UML_GameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()
	
private:
	FIntPoint ResolutionPx = FIntPoint(1920, 1080); // Actual resolution in pixel
	float FrameRLimit = 0.0f; // Actual frame rate limit
	static TArray<FIntPoint> ValidResolutions;
	static TArray<float> ValidFrameLimits;
	
	UWorld* GetWorld() const;
	FIntPoint GetClosestValidResolution(FIntPoint DesiredResolution) const;
	static bool ParseResolutionText(const FString& ResolutionText, FIntPoint& OutResolution);
	static bool ParseFrameLimitText(const FString& LimitText, float& OutFrameLimit);
	
	void LoadResolution();
	void LoadFrameLimit();
	
public:
	UML_GameUserSettings();
	void InitValues();

	// ==================== Singleton Access ====================
	
	UFUNCTION(BlueprintPure, Category = "Settings")
	static UML_GameUserSettings* GetMycelandGameUserSettings()
	{
		return Cast<UML_GameUserSettings>(UGameUserSettings::GetGameUserSettings());
	}
	
	
	
	// ==================== Delegates ====================
	
	UPROPERTY(BlueprintAssignable, Category = "Settings|Events")
	FOnSettingsLoaded OnSettingsLoaded;
	
	
	
	// ==================== Delegates ====================
	
	UFUNCTION(BlueprintPure, Category = "Settings|Helper")
	static float Normalize(float Value, float Min, float Max);
	
	UFUNCTION(BlueprintPure, Category = "Settings|Helper")
	static float Denormalize(float Normalized, float Min, float Max);
	
	
	
	// ==================== Graphics Settings (Wrappers) ====================

	/*
	 * Note: These properties are wrappers for Unreal's native settings.
	 * They are NOT saved in the .ini file (no Config).
	 * They are synchronized from the actual values in LoadSettings()
	 */

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics")
	int32 ResolutionValue = 2; // Index in the component

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics")
	float ResolutionScale = 100.0f; // 0-100

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics")
	int32 WindowMode = 2; // 0=Fullscreen, 1=Windowed Fullscreen, 2=Windowed

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics")
	bool bVSync = false;

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics")
	int32 FrameLimit = 1; // Index in the component (0=30, 1=60, 2=120, 3=144, 4=Unlimited...)

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics")
	int32 OverallQuality = 1; // 0=Low, 1=Medium, 2=High, 3=Epic, 4=Cinematic

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics")
	int32 TextureQuality = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics")
	int32 ShadowQuality = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics")
	int32 EffectsQuality = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics")
	int32 PostProcessingQuality = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics")
	int32 GlobalIlluminationQuality = 1;


	// ===== Graphics Setters (from widgets) =====
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	static void SetValidResolutions(const TArray<FText>& Resolutions);
	
	UFUNCTION(BlueprintPure, Category = "Settings|Graphics")
	static bool AreValidResolutionsInitialized();
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	static void SetValidFrameLimits(const TArray<FText>& FrameLimits);
	
	UFUNCTION(BlueprintPure, Category = "Settings|Graphics")
	static bool AreValidFrameLimitsInitialized();

	/**
	 * Set resolution from dropdown text (e.g., "1920x1080")
	 * Also updates ResolutionValue index.
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void SetResolutionFromText(const FString& ResolutionText);

	/**
	 * Set frame rate limit from dropdown text (e.g., "60" or "Unlimited")
	 * Also updates FrameLimit index.
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void SetFrameRateLimitFromText(const FString& FrameRateText);

	/**
	 * Set resolution scale from slider value (0-100)
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void SetResolutionScaleFromSlider(const float SliderValue) { ResolutionScale = SliderValue; }

	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void SetWindowModeFromIndex(const int32 Index) { WindowMode = FMath::Clamp(Index, 0, 2); }

	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void SetVSyncFromToggle(const bool bEnabled) { bVSync = bEnabled; }

	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void SetOverallQualityFromIndex(const int32 Index) { OverallQuality = FMath::Clamp(Index, 0, 4); }

	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void SetTextureQualityFromIndex(const int32 Index) { TextureQuality = FMath::Clamp(Index, 0, 4); }

	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void SetShadowQualityFromIndex(const int32 Index) { ShadowQuality = FMath::Clamp(Index, 0, 4); }

	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void SetEffectsQualityFromIndex(const int32 Index) { EffectsQuality = FMath::Clamp(Index, 0, 4); }

	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void SetPostProcessingQualityFromIndex(const int32 Index) { PostProcessingQuality = FMath::Clamp(Index, 0, 4); }

	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void SetGlobalIlluminationQualityFromIndex(const int32 Index) { GlobalIlluminationQuality = FMath::Clamp(Index, 0, 4); }


	// ===== Graphics Getters (for debugging) =====

	UFUNCTION(BlueprintPure, Category = "Settings|Graphics")
	FString GetCurrentResolutionText() const;

	UFUNCTION(BlueprintPure, Category = "Settings|Graphics")
	FString GetCurrentFrameRateLimitText() const;

	
	
	// ==================== Audio Settings ====================
	
	UPROPERTY(Config)
	float MasterVolume = 100.0f;
	
	UPROPERTY(Config)
	float MusicVolume = 100.0f;
	
	UPROPERTY(Config)
	float SFXVolume = 100.0f;
	
	UPROPERTY(Config)
	float UIVolume = 100.0f;
	
	UPROPERTY(Config)
	float VoiceVolume = 100.0f;

	
	// ===== Audio Setters =====
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void SetMasterVolume(const float Volume) { MasterVolume = Volume; }
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void SetMusicVolume(const float Volume) { MusicVolume = Volume; }
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void SetSFXVolume(const float Volume) { SFXVolume = Volume; }
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void SetUIVolume(const float Volume) { UIVolume = Volume; }
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void SetVoiceVolume(const float Volume) { VoiceVolume = Volume; }

	
	// ===== Audio Getters =====
	
	UFUNCTION(BlueprintPure, Category = "Settings|Audio")
	float GetMasterVolume() const { return MasterVolume; }
	
	UFUNCTION(BlueprintPure, Category = "Settings|Audio")
	float GetMusicVolume() const { return MusicVolume; }
	
	UFUNCTION(BlueprintPure, Category = "Settings|Audio")
	float GetSFXVolume() const { return SFXVolume; }
	
	UFUNCTION(BlueprintPure, Category = "Settings|Audio")
	float GetUIVolume() const { return UIVolume; }
	
	UFUNCTION(BlueprintPure, Category = "Settings|Audio")
	float GetVoiceVolume() const { return VoiceVolume; }

	
	
	// ==================== Controls ====================
	
	UPROPERTY(Config)
	float MouseSensitivity = 0.5f;
	
	UPROPERTY(Config)
	float GamepadSensitivity = 0.5f;
	
	UPROPERTY(Config)
	float GamepadDeadZone = 0.2f;

	
	// ===== Controls Setters =====
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Controls")
	void SetMouseSensitivity(const float Sensitivity) { MouseSensitivity = Sensitivity; }
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Controls")
	void SetGamepadSensitivity(const float Sensitivity) { GamepadSensitivity = Sensitivity; }
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Controls")
	void SetGamepadDeadZone(const float DeadZone) { GamepadDeadZone = DeadZone; }

	
	// ===== Controls Getters =====
	
	UFUNCTION(BlueprintPure, Category = "Settings|Controls")
	float GetMouseSensitivity() const { return MouseSensitivity; }
	
	UFUNCTION(BlueprintPure, Category = "Settings|Controls")
	float GetGamepadSensitivity() const { return GamepadSensitivity; }
	
	UFUNCTION(BlueprintPure, Category = "Settings|Controls")
	float GetGamepadDeadZone() const { return GamepadDeadZone; }

	
	
	// ==================== Gameplay Settings ====================
	
	UPROPERTY(Config)
	bool bShowGrid = true;
	
	UPROPERTY(Config)
	bool bTileHighlight = true;
	
	UPROPERTY(Config)
	float HighlightIntensity = 1.0f;
	
	UPROPERTY(Config)
	bool bPropagationPreview = true;
	
	UPROPERTY(Config)
	bool bUndoConfirmation = false;
	
	UPROPERTY(Config)
	bool bResetConfirmation = true;
	
	UPROPERTY(Config)
	bool bTutorialHints = true;

	
	// ===== Gameplay Setters =====
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Gameplay")
	void SetShowGrid(const bool bShow) { bShowGrid = bShow; }
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Gameplay")
	void SetTileHighlight(const bool bEnable) { bTileHighlight = bEnable; }
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Gameplay")
	void SetHighlightIntensity(const float Intensity) { HighlightIntensity = Intensity; }
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Gameplay")
	void SetPropagationPreview(const bool bEnable) { bPropagationPreview = bEnable; }
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Gameplay")
	void SetUndoConfirmation(const bool bEnable) { bUndoConfirmation = bEnable; }
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Gameplay")
	void SetResetConfirmation(const bool bEnable) { bResetConfirmation = bEnable; }
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Gameplay")
	void SetTutorialHints(const bool bEnable) { bTutorialHints = bEnable; }

	
	// ===== Gameplay Getters =====
	
	UFUNCTION(BlueprintPure, Category = "Settings|Gameplay")
	bool GetShowGrid() const { return bShowGrid; }
	
	UFUNCTION(BlueprintPure, Category = "Settings|Gameplay")
	bool GetTileHighlight() const { return bTileHighlight; }
	
	UFUNCTION(BlueprintPure, Category = "Settings|Gameplay")
	float GetHighlightIntensity() const { return HighlightIntensity; }
	
	UFUNCTION(BlueprintPure, Category = "Settings|Gameplay")
	bool GetPropagationPreview() const { return bPropagationPreview; }
	
	UFUNCTION(BlueprintPure, Category = "Settings|Gameplay")
	bool GetUndoConfirmation() const { return bUndoConfirmation; }
	
	UFUNCTION(BlueprintPure, Category = "Settings|Gameplay")
	bool GetResetConfirmation() const { return bResetConfirmation; }
	
	UFUNCTION(BlueprintPure, Category = "Settings|Gameplay")
	bool GetTutorialHints() const { return bTutorialHints; }

	
	
	// ==================== Accessibility Settings ====================
	
	UPROPERTY(Config)
	bool bSubtitles = false;
	
	UPROPERTY(Config)
	float SubtitlesSize = 50.0f;
	
	UPROPERTY(Config)
	int32 ColorblindMode = 0; // 0=None, 1=Protanopia, 2=Deuteranopia, 3=Tritanopia

	
	// ===== Accessibility Setters =====
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Accessibility")
	void SetSubtitles(const bool bEnable) { bSubtitles = bEnable; }
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Accessibility")
	void SetSubtitlesSize(const float Size) { SubtitlesSize = Size; }
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Accessibility")
	void SetColorblindMode(const int32 Mode) { ColorblindMode = Mode; }

	
	// ===== Accessibility Getters =====
	
	UFUNCTION(BlueprintPure, Category = "Settings|Accessibility")
	bool GetSubtitles() const { return bSubtitles; }
	
	UFUNCTION(BlueprintPure, Category = "Settings|Accessibility")
	float GetSubtitlesSize() const { return SubtitlesSize; }
	
	UFUNCTION(BlueprintPure, Category = "Settings|Accessibility")
	int32 GetColorblindMode() const { return ColorblindMode; }

	
	
	// ==================== Apply Methods ====================
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void ApplyGraphicsSettings();
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void ApplyAudioSettings();
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Gameplay")
	void ApplyGameplaySettings();
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Accessibility")
	void ApplyAccessibilitySettings();

	
	
	// ==================== Overrides ====================
	
	virtual void ApplySettings(bool bCheckForCommandLineOverrides) override;
	virtual void SetToDefaults() override;
	virtual void LoadSettings(bool bForceReload = false) override;
};