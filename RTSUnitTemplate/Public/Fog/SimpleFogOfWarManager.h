// SimpleFogManager.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimpleFogOfWarManager.generated.h"

// Forward declaration
class IFogRevealerInterface;

UENUM(BlueprintType)
enum class EFogState : uint8
{
	Hidden		UMETA(DisplayName = "Hidden"),
	Explored	UMETA(DisplayName = "Explored (Shroud)"),
	Visible		UMETA(DisplayName = "Visible")
};

UCLASS()
class RTSUNITTEMPLATE_API ASimpleFogOfWarManager : public AActor
{
	GENERATED_BODY()

public:
	ASimpleFogOfWarManager();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	//~ Begin Configuration Properties
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog Of War|Setup")
	FVector FogOrigin = FVector::ZeroVector; // Bottom-left corner of the fog area in world space

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog Of War|Setup")
	int32 FogGridWidth = 100; // Number of cells in X direction

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog Of War|Setup")
	int32 FogGridHeight = 100; // Number of cells in Y direction

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog Of War|Setup")
	float CellSize = 100.0f; // Size of each fog cell in world units

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog Of War|Debug")
	bool bUseDebugDrawing = true; // Toggle for drawing debug cells

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog Of War|Setup")
	float UpdateInterval = 0.1f; // How often to update the fog (in seconds)
	//~ End Configuration Properties


	//~ Begin Public Functions
	UFUNCTION(BlueprintCallable, Category = "Fog Of War")
	void AddRevealer(AActor* RevealerActor);

	UFUNCTION(BlueprintCallable, Category = "Fog Of War")
	void RemoveRevealer(AActor* RevealerActor);

	UFUNCTION(BlueprintPure, Category = "Fog Of War")
	EFogState GetFogStateAtWorldLocation(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Fog Of War")
	bool WorldToGrid(const FVector& WorldLocation, int32& OutX, int32& OutY) const;

	UFUNCTION(BlueprintPure, Category = "Fog Of War")
	FVector GridToWorld(int32 X, int32 Y) const;
	//~ End Public Functions

private:
	TArray<EFogState> FogGrid;
	TArray<TScriptInterface<IFogRevealerInterface>> Revealers;

	float TimeSinceLastUpdate = 0.0f;

	void InitializeFogGrid();
	void UpdateFog();
	void UpdateCellState(int32 X, int32 Y, EFogState NewState);
	EFogState GetCellState(int32 X, int32 Y) const;

	void DrawDebugFog() const; // For visualization
};