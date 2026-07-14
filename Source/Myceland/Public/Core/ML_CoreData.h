// Copyright Myceland Team, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ML_CoreData.generated.h"

// ==================== STATIC ====================

// static const FIntPoint Directions[6];

static const FIntPoint Directions[6] = {
	FIntPoint(1, 0),
	FIntPoint(1, -1),
	FIntPoint(0, -1),
	FIntPoint(-1, 0),
	FIntPoint(-1, 1),
	FIntPoint(0, 1)
};


// ==================== ENUM ====================

class UInputMappingContext;
class AML_Collectible;
class UML_PropagationWaves;
class AML_Tile;
class AActor;

UENUM(BlueprintType)
enum class EInputMappingType : uint8
{
	Cinematic,
	Teleport
};

UENUM(BlueprintType)
enum class EML_HexGridLayout : uint8
{
	HexagonRadius UMETA(DisplayName="Hexagon (Radius)"),
	RectangleWH   UMETA(DisplayName="Rectangle (Width/Height)")
};

UENUM(BlueprintType)
enum class EML_HexOffsetLayout : uint8
{
	OddR,   // PointyTop le plus fréquent
	EvenR,
	OddQ,   // FlatTop le plus fréquent
	EvenQ
};

UENUM(BlueprintType)
enum class EML_HexOrientation : uint8
{
	FlatTop,
	PointyTop
};

UENUM(BlueprintType)
enum class EML_TileType : uint8
{
	Dirt,
	Grass,
	Parasite,
	Water,
	WaterPath,
	Obstacle,
	Tree
};

UENUM(BlueprintType)
enum class EML_WinLose : uint8
{
	None,
	Win,
	Lose
};

UENUM(BlueprintType)
enum class EML_WidgetInputMode : uint8
{
	GameOnly,
	UIOnly,
	GameAndUI
};

UENUM(BlueprintType)
enum class EML_PlayerMovementMode : uint8
{
	InsideBoard,   // Tile-by-tile movement (current behavior)
	EnteringBoard, // PLayer enter the board from exterior
	ExitingBoard,  // Hold in progress to confirm exit
	FreeMovement   // Free movement off the board
};

UENUM(BlueprintType)
enum class EML_PlayerBoardActionState : uint8
{
	Idle,           // Standing still — all board input accepted
	Moving,         // Walking to a tile (left-click)
	MovingToPlant,  // Walking to plant position (right-click move-and-plant)
	TurningToPlant, // Facing the target tile before planting — no input accepted
};

UENUM(BlueprintType)
enum class EML_InputDevice : uint8
{
	MouseKeyboard UMETA(DisplayName = "Mouse & Keyboard"),
	Gamepad       UMETA(DisplayName = "Gamepad")
};

// Which input device(s) a gameplay IMC entry covers. Drives which devices the input-device switch is
// allowed to target: an array whose entries only cover one device locks input to it. "Both" is the
// neutral default — also what non-gameplay IMCs (Cinematic, Teleport) keep, since they ignore this field.
UENUM(BlueprintType)
enum class EML_InputMappingDevice : uint8
{
	Both,
	MouseKeyboard UMETA(DisplayName = "Mouse & Keyboard"),
	Gamepad       UMETA(DisplayName = "Gamepad")
};

UENUM(BlueprintType)
enum class ESettingValueType : uint8
{
	Float,
	Bool,
	Int32,
	Unknown
};


// ==================== STRUCT ====================

USTRUCT(BlueprintType)
struct FML_GameResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	EML_WinLose Result = EML_WinLose::None;

	UPROPERTY(BlueprintReadOnly)
	bool bIsGameOver = false;
};

USTRUCT(BlueprintType)
struct FML_WaveChange
{
	GENERATED_BODY()
	
	UPROPERTY()
	AML_Tile* Tile = nullptr; // Tile
	
	UPROPERTY()
	EML_TileType TargetType = EML_TileType::Dirt; // Tile
	
	UPROPERTY()
	AML_Tile* Neighbor = nullptr; // Collectible
	
	UPROPERTY()
	AML_Tile* SourceParasite = nullptr; // Collectible — the specific parasite that triggered this spawn

	UPROPERTY()
	FVector SpawnLocation = FVector::ZeroVector; // Collectible
	
	UPROPERTY()
	TSubclassOf<AML_Collectible> CollectibleClass = nullptr; // Collectible
	
	UPROPERTY()
	int32 DistanceFromOrigin = 0;
	
	FML_WaveChange() = default;
	FML_WaveChange(AML_Tile* InTile, const EML_TileType InType, const int32 InDistance) : Tile(InTile), TargetType(InType), DistanceFromOrigin(InDistance) {}
	FML_WaveChange(const FVector& InLocation, const TSubclassOf<AML_Collectible> Class, const int32 InDistance) : SpawnLocation(InLocation), CollectibleClass(Class), DistanceFromOrigin(InDistance)  {}
};

USTRUCT(BlueprintType)
struct FML_InputMappingEntry
{
	GENERATED_BODY()

	// Only meaningful for the gameplay IMC array: which device(s) this IMC covers. The input-device
	// switch only targets devices present in the array, so a single-device array locks input to it.
	// Left at "Both" (the default) for Cinematic / Teleport IMCs, which ignore this field.
	UPROPERTY(EditAnywhere, config)
	EML_InputMappingDevice Device = EML_InputMappingDevice::Both;

	UPROPERTY(EditAnywhere, config)
	TSoftObjectPtr<UInputMappingContext> Mapping;

	UPROPERTY(EditAnywhere, config)
	int32 Priority = 0;
};

USTRUCT(BlueprintType)
struct FML_TileGroup
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TArray<AML_Tile*> Tiles;
	
	UPROPERTY(BlueprintReadOnly)
	TArray<AML_Tile*> Goals;
};

USTRUCT(BlueprintType)
struct FML_TabSlotSettings
{
	GENERATED_BODY()
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment = HAlign_Fill;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	TEnumAsByte<EVerticalAlignment> VerticalAlignment = VAlign_Fill;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	FMargin Padding = FMargin(10.0f, 0.0f, 10.0f, 0.0f); // Left, Top, Right, Bottom
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot", meta=(Tooltip="TRUE = Automatic | FALSE = Fill"))
	bool bAutoSize = false; // false = Fill, true = Auto
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot", meta=(EditCondition="!bAutoSize"))
	float Size = 1.0f; // Used when bAutoSize is false (Fill mode)
};

USTRUCT(BlueprintType)
struct FML_TabData
{
	GENERATED_BODY()
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tab")
	FText TabName;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tab")
	TSubclassOf<UUserWidget> ContentWidgetClass;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tab")
	FML_TabSlotSettings SlotSettings;
};

USTRUCT(BlueprintType)
struct FML_WavePriorityEntry
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	TSubclassOf<UML_PropagationWaves> WaveClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta=(Tooltip="If true and this wave has no changes, the propagation will stop entirely. If false, continues to next wave."))
	bool bCanStopHereIfNoChanges = true;
};

USTRUCT(BlueprintType)
struct FML_WaterPath
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water Path")
	TObjectPtr<AML_Tile> EntryTile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water Path")
	TObjectPtr<AML_Tile> ExitTile;
};

// Describes one gamepad board exit: a plane/actor to highlight and the border tiles that activate it.
// The ExitPlane is also the world destination the player walks toward when confirming the exit.
USTRUCT(BlueprintType)
struct FML_BoardExit
{
	GENERATED_BODY()

	// The plane/actor that should highlight when the player stands on one of the ExitTiles.
	// It also serves as the world destination the player walks toward when confirming the exit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Board Exit")
	TObjectPtr<AActor> ExitPlane = nullptr;

	// Border tiles that activate this exit while the player stands on one of them.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Board Exit")
	TArray<TObjectPtr<AML_Tile>> ExitTiles;
};
