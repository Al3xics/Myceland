// Copyright Myceland Team, All Rights Reserved.

#include "Input/ML_InputDeviceManager.h"

#include "GameFramework/InputDeviceSubsystem.h"
#include "Input/ML_InputHandlerBase.h"
#include "Input/Handlers/ML_MouseKeyboardInputHandler.h"
#include "Input/Handlers/ML_GamepadInputHandler.h"

UML_InputDeviceManager::UML_InputDeviceManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UML_InputDeviceManager::Initialize(UML_MouseKeyboardInputHandler* InMouseKeyboard, UML_GamepadInputHandler* InGamepad)
{
	MouseKeyboardHandler = InMouseKeyboard;
	GamepadHandler = InGamepad;

	// Start on mouse/keyboard; first gamepad input will switch automatically.
	SwitchToDevice(EML_InputDevice::MouseKeyboard);

	if (MouseKeyboardHandler) MouseKeyboardHandler->OnActivated();
	if (GamepadHandler) GamepadHandler->OnDeactivated();

	// Subscribe to hardware device change (player switches between keyboard and gamepad).
	if (UInputDeviceSubsystem* DevSub = GEngine ? GEngine->GetEngineSubsystem<UInputDeviceSubsystem>() : nullptr)
		DevSub->OnInputHardwareDeviceChanged.AddDynamic(this, &UML_InputDeviceManager::HandleHardwareDeviceChanged);

	// Subscribe to connection change (gamepad disconnect/reconnect).
	IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().AddUObject(this, &UML_InputDeviceManager::HandleDeviceConnectionChanged);
}

void UML_InputDeviceManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if (UInputDeviceSubsystem* DevSub = GEngine ? GEngine->GetEngineSubsystem<UInputDeviceSubsystem>() : nullptr)
		DevSub->OnInputHardwareDeviceChanged.RemoveDynamic(this, &UML_InputDeviceManager::HandleHardwareDeviceChanged);

	IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().RemoveAll(this);
}

void UML_InputDeviceManager::SwitchToDevice(EML_InputDevice NewDevice)
{
	if (ActiveHandler) ActiveHandler->OnDeactivated();

	CurrentDevice = NewDevice;
	ActiveHandler = (NewDevice == EML_InputDevice::Gamepad)
	                ? static_cast<UML_InputHandlerBase*>(GamepadHandler)
	                : static_cast<UML_InputHandlerBase*>(MouseKeyboardHandler);

	if (ActiveHandler) ActiveHandler->OnActivated();
	OnInputDeviceChanged.Broadcast(NewDevice);
	
	UE_LOG(LogTemp, Log, TEXT("Input device switched to %s"), *ActiveHandler->GetName());
}

void UML_InputDeviceManager::NotifyGamepadInput()
{
	bGamepadConnectEventPending = false;
	if (CurrentDevice != EML_InputDevice::Gamepad)
		SwitchToDevice(EML_InputDevice::Gamepad);
}

void UML_InputDeviceManager::NotifyMixedAction()
{
	// IA_Move is shared between mouse click and gamepad confirm button.
	// HandleHardwareDeviceChanged (UInputDeviceSubsystem) fires before Enhanced Input action
	// callbacks, so CurrentDevice is already up-to-date here. Use it directly to avoid
	// platform user ID mismatches when querying GetMostRecentlyUsedHardwareDevice.
	if (CurrentDevice == EML_InputDevice::Gamepad)
		NotifyGamepadInput();
	else
		NotifyMouseKeyboardInput();
}

void UML_InputDeviceManager::NotifyMouseKeyboardInput()
{
	if (CurrentDevice != EML_InputDevice::MouseKeyboard)
		SwitchToDevice(EML_InputDevice::MouseKeyboard);
}

void UML_InputDeviceManager::HandleHardwareDeviceChanged(const FPlatformUserId UserId, const FInputDeviceId DeviceId)
{
	// Guard: ignore events fired outside of actual gameplay (e.g. editor, stale subscriptions).
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	const ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	if (!LP) return;

	if (LP->GetPlatformUserId() != UserId) return;

	UInputDeviceSubsystem* DevSub = GEngine ? GEngine->GetEngineSubsystem<UInputDeviceSubsystem>() : nullptr;
	if (!DevSub) return;

	const FHardwareDeviceIdentifier HWDevice = DevSub->GetMostRecentlyUsedHardwareDevice(UserId);
	const bool bIsGamepad = (HWDevice.PrimaryDeviceType == EHardwareDevicePrimaryType::Gamepad);

	if (bIsGamepad)
	{
		// Gamepad button press. Guard against auto-switch on mere connection when
		// bSwitchOnGamepadConnect is false.
		const bool bFromConnectionEvent = bGamepadConnectEventPending;
		bGamepadConnectEventPending = false;

		if (bFromConnectionEvent && !bSwitchOnGamepadConnect)
			return;

		if (CurrentDevice != EML_InputDevice::Gamepad)
			SwitchToDevice(EML_InputDevice::Gamepad);
	}
}

void UML_InputDeviceManager::HandleDeviceConnectionChanged(EInputDeviceConnectionState NewState, FPlatformUserId UserId, FInputDeviceId InputDeviceId)
{
	if (NewState == EInputDeviceConnectionState::Disconnected)
	{
		bGamepadConnectEventPending = false;
		if (CurrentDevice == EML_InputDevice::Gamepad)
			SwitchToDevice(EML_InputDevice::MouseKeyboard);
	}
	else if (NewState == EInputDeviceConnectionState::Connected)
	{
		if (bSwitchOnGamepadConnect)
		{
			bGamepadConnectEventPending = false;
			if (CurrentDevice != EML_InputDevice::Gamepad)
				SwitchToDevice(EML_InputDevice::Gamepad);
		}
		else
		{
			bGamepadConnectEventPending = true;
			// HandleHardwareDeviceChanged may have already fired before this callback
			// and auto-switched — undo that if bSwitchOnGamepadConnect is false.
			if (CurrentDevice == EML_InputDevice::Gamepad)
				SwitchToDevice(EML_InputDevice::MouseKeyboard);
		}
	}
}
