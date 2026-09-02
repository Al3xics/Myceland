// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/ML_CoreData.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "ML_InputDeviceManager.generated.h"

class UML_InputHandlerBase;
class UML_MouseKeyboardInputHandler;
class UML_GamepadInputHandler;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInputDeviceChanged, EML_InputDevice, NewDevice);

/**
 * Detects the active input device and routes input to the correct handler.
 *
 * Detection strategy: subscribes to UInputDeviceSubsystem::OnInputHardwareDeviceChanged
 * (fires when the player switches device) and IPlatformInputDeviceMapper's connection-change
 * delegate (fires on disconnect). No per-frame polling required.
 */
UCLASS(ClassGroup=(Myceland), meta=(BlueprintSpawnableComponent))
class MYCELAND_API UML_InputDeviceManager : public UActorComponent
{
	GENERATED_BODY()

private:
	UPROPERTY()
	UML_MouseKeyboardInputHandler* MouseKeyboardHandler = nullptr;

	UPROPERTY()
	UML_GamepadInputHandler* GamepadHandler = nullptr;

	UPROPERTY()
	UML_InputHandlerBase* ActiveHandler = nullptr;

	EML_InputDevice CurrentDevice = EML_InputDevice::MouseKeyboard;

	void SwitchToDevice(EML_InputDevice NewDevice);

	// Must be UFUNCTION because OnInputHardwareDeviceChanged is a dynamic multicast delegate.
	UFUNCTION()
	void HandleHardwareDeviceChanged(const FPlatformUserId UserId, const FInputDeviceId DeviceId);

	void HandleDeviceConnectionChanged(EInputDeviceConnectionState NewState, FPlatformUserId UserId, FInputDeviceId InputDeviceId);

	/**
	 * True between the moment a gamepad connects and the moment HandleHardwareDeviceChanged
	 * consumes that connection event. Used to distinguish "connection fired the hardware event"
	 * from "player actually pressed a button", so bSwitchOnGamepadConnect is respected.
	 */
	bool bGamepadConnectEventPending = false;

	// Which devices have a gameplay IMC mapped (see SetAvailableDevices). The manager never switches to
	// a device that is not available, so a single-IMC setup locks input to that device.
	bool bMouseKeyboardAvailable = true;
	bool bGamepadAvailable = true;


public:
	UML_InputDeviceManager();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void Initialize(UML_MouseKeyboardInputHandler* InMouseKeyboard, UML_GamepadInputHandler* InGamepad);

	UML_InputHandlerBase* GetActiveHandler() const { return ActiveHandler; }
	EML_InputDevice GetCurrentDevice() const { return CurrentDevice; }

	/**
	 * Declares which devices have a gameplay IMC mapped. The manager will refuse to switch to a device
	 * that is not available (so an array with only one device's IMC keeps input locked to it), and
	 * immediately switches away from the current device if it just became unavailable.
	 */
	void SetAvailableDevices(bool bMouseKeyboard, bool bGamepad);

	/**
	 * Call this from any gamepad-only input callback (analog axis, gamepad-exclusive button).
	 * Switches to Gamepad if not already active — bypasses OnInputHardwareDeviceChanged,
	 * which does not fire for analog axis inputs.
	 */
	void NotifyGamepadInput();

	/**
	 * Call this from any mouse/keyboard-only action callback.
	 * Switches to MouseKeyboard if not already active.
	 * HandleHardwareDeviceChanged no longer handles this direction because UE fires that event
	 * every frame for the virtual cursor driven by the analog stick (unavoidable false positive).
	 */
	void NotifyMouseKeyboardInput();

	/**
	 * Call this from actions shared between MK and gamepad (e.g. IA_Move bound to both
	 * mouse click and gamepad confirm button). Queries the most recently used hardware device
	 * to pick the correct handler instead of blindly switching to MouseKeyboard.
	 */
	void NotifyMixedAction();

	/**
	 * If true, switches to Gamepad as soon as one is connected.
	 * If false, waits for the player to press a button on the gamepad before switching.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Myceland")
	bool bSwitchOnGamepadConnect = false;

	/** Broadcast whenever the active device changes. Useful for HUD icon updates. */
	UPROPERTY(BlueprintAssignable, Category = "Myceland")
	FOnInputDeviceChanged OnInputDeviceChanged;
};
