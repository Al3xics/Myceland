// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ML_InputHandlerBase.generated.h"

class AML_PlayerController;

/**
 * Abstract base for device-specific input handlers.
 * Each concrete handler implements the relevant virtual methods for its device
 * (mouse/keyboard or gamepad). The controller delegates to the active handler
 * without knowing which device is in use.
 */
UCLASS(Abstract, ClassGroup=(Myceland), meta=(BlueprintSpawnableComponent))
class MYCELAND_API UML_InputHandlerBase : public UActorComponent
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	AML_PlayerController* Controller = nullptr;

public:
	void Initialize(AML_PlayerController* InController);

	/** Called by UML_InputDeviceManager when this handler becomes the active one. */
	virtual void OnActivated() {}

	/** Called by UML_InputDeviceManager when another handler takes over. */
	virtual void OnDeactivated() {}

	/** IA_Move → Started. One shot per press. */
	virtual void OnMoveActionStarted() {}

	/** IA_Move → Triggered. Called every frame while held. */
	virtual void OnMoveActionTriggered(float DeltaTime) {}

	/** IA_Move → Completed / Canceled. */
	virtual void OnMoveActionReleased() {}

	/** IA_MoveAndPlant → Started. */
	virtual void OnMoveAndPlantAction() {}

	/**
	 * IA_GamepadMove → Triggered. Called every frame while the stick is away from neutral.
	 * StickValue is the raw Axis2D value (X = right, Y = up in screen space).
	 */
	virtual void OnStickAxis(FVector2D StickValue, float DeltaTime) {}

	/** IA_GamepadMove → Completed / Canceled. Called when the stick returns to the dead zone. */
	virtual void OnStickReleased() {}
};
