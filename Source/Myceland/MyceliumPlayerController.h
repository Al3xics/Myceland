#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "MyceliumPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class AML_BoardSpawner;
class AMyceliumCharacter;
class AML_Tile;

UCLASS()
class MYCELAND_API AMyceliumPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMyceliumPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;

	// ---------- Enhanced Input ----------
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<const UInputAction> IA_SetDestination_Click = nullptr;

	// Assign from editor (simpler)
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputMappingContext> DefaultIMC = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Input")
	int32 DefaultIMCPriority = 0;

	UFUNCTION()
	void OnSetDestinationTriggered(const FInputActionValue& Value);

	// ---------- Movement tuning ----------
	UPROPERTY(EditAnywhere, Category="Myceland|Movement")
	float AcceptanceRadius = 12.f;

	UPROPERTY(EditAnywhere, Category="Myceland|Movement")
	float MoveSpeedScale = 1.f;

	// 0 = strict center-to-center, 1 = maximum smoothing (skip corners easily)
	UPROPERTY(EditAnywhere, Category="Myceland|Movement|Smoothing", meta=(ClampMin="0.0", ClampMax="1.0"))
	float CornerCutStrength = 0.5f;

	// Distance (in cm) used to decide if we can skip a waypoint (scaled by strength)
	UPROPERTY(EditAnywhere, Category="Myceland|Movement|Smoothing", meta=(ClampMin="0.0"))
	float BaseCornerCutDistance = 80.f;

private:
	// Path in world space (tile centers)
	TArray<FVector> CurrentPathWorld;
	int32 CurrentPathIndex = 0;

	// ---- Helpers ----
	AML_Tile* GetTileUnderCursor() const;
	AML_BoardSpawner* GetBoardFromCurrentTile(const AMyceliumCharacter* MyChar) const;

	bool IsTileWalkable(const AML_Tile* Tile) const;

	bool BuildPath_AxialBFS(
		const FIntPoint& StartAxial,
		const FIntPoint& GoalAxial,
		const TMap<FIntPoint, AML_Tile*>& GridMap,
		TArray<FIntPoint>& OutAxialPath) const;

	void StartMoveAlongPath(const TArray<FIntPoint>& AxialPath, const TMap<FIntPoint, AML_Tile*>& GridMap);
	void TickMoveAlongPath(float DeltaTime);

	static const FIntPoint AxialDirections[6];
};