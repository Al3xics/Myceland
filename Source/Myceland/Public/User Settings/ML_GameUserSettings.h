// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "ML_GameUserSettings.generated.h"

UCLASS()
class MYCELAND_API UML_GameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()
	
public:
	UML_GameUserSettings();
	void InitValues();

    // ==================== Singleton Access ====================
    
    UFUNCTION(BlueprintPure, Category = "Settings")
    static UML_GameUserSettings* GetMycelandGameUserSettings()
    {
    	return Cast<UML_GameUserSettings>(UGameUserSettings::GetGameUserSettings());
    }

	
	
    // ==================== Audio Settings ====================
    
    UPROPERTY(Config)
    float MasterVolume = 1.0f;
    
    UPROPERTY(Config)
    float MusicVolume = 1.0f;
    
    UPROPERTY(Config)
    float SFXVolume = 1.0f;
    
    UPROPERTY(Config)
    float UIVolume = 1.0f;
    
    UPROPERTY(Config)
    float VoiceVolume = 1.0f;

	
    // ===== Audio Setters =====
	
    UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
    void SetMasterVolume(float Volume);
    
    UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
    void SetMusicVolume(float Volume);
    
    UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
    void SetSFXVolume(float Volume);
    
    UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
    void SetUIVolume(float Volume);
    
    UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
    void SetVoiceVolume(float Volume);

	
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

	
    // ===== Apply all audio settings at once =====
	
    UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
    void ApplyAudioSettings();

	
	
    // ==================== Input Settings ====================
    
    UPROPERTY(Config)
    float MouseSensitivity = 1.0f;
    
    UPROPERTY(Config)
    float GamepadSensitivity = 1.0f;
    
    UPROPERTY(Config)
    float GamepadDeadZone = 0.25f;

	
    // ===== Input Setters =====
	
    UFUNCTION(BlueprintCallable, Category = "Settings|Input")
    void SetMouseSensitivity(float Sensitivity);
    
    UFUNCTION(BlueprintCallable, Category = "Settings|Input")
    void SetGamepadSensitivity(float Sensitivity);
    
    UFUNCTION(BlueprintCallable, Category = "Settings|Input")
    void SetGamepadDeadZone(float DeadZone);

	
    // ===== Input Getters =====
	
    UFUNCTION(BlueprintPure, Category = "Settings|Input")
    float GetMouseSensitivity() const { return MouseSensitivity; }
    
    UFUNCTION(BlueprintPure, Category = "Settings|Input")
    float GetGamepadSensitivity() const { return GamepadSensitivity; }
    
    UFUNCTION(BlueprintPure, Category = "Settings|Input")
    float GetGamepadDeadZone() const { return GamepadDeadZone; }

	
	
    // ==================== Graphics Helpers ====================
    
    // ===== Wrapper methods for common graphics settings with Blueprint-friendly names =====
    
    UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
    void SetGraphicsPreset(int32 Preset);
    
    UFUNCTION(BlueprintPure, Category = "Settings|Graphics")
    int32 GetGraphicsPreset() const { return GetOverallScalabilityLevel(); }
    
    UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
    void SetResolutionScale(float Scale);
    
    UFUNCTION(BlueprintPure, Category = "Settings|Graphics")
    float GetResolutionScale() const { return GetResolutionScaleNormalized() * 100.0f; }

	
	
    // ==================== Overrides ====================
    
    virtual void ApplySettings(bool bCheckForCommandLineOverrides) override;
    virtual void SetToDefaults() override;
    virtual void LoadSettings(bool bForceReload = false) override;
};
