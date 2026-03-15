// Copyright 2024 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "EnergyWall.generated.h"

class UInstancedStaticMeshComponent;
class UBoxComponent;
class UNavModifierComponent;
class ABuildingBase;

/**
 * AEnergyWall - An adaptive energy wall actor that connects two buildings.
 * Holds three ISM meshes (Top Rod, Bottom Rod, Shield Plane) and acts as a navigation obstacle.
 */
UCLASS()
class RTSUNITTEMPLATE_API AEnergyWall : public AActor
{
	GENERATED_BODY()
	
public:	
	AEnergyWall();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* WallRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInstancedStaticMeshComponent* TopRodISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInstancedStaticMeshComponent* BottomRodISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInstancedStaticMeshComponent* ShieldISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* NavObstacleBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UNavModifierComponent* NavModifier;
	
	/**
	 * Initializes the wall between two buildings, setting up its position, rotation, scaling, and navigation obstacle.
	 * @param BuildingA First building to connect.
	 * @param BuildingB Second building to connect.
	 */
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = "EnergyWall")
	void Multicast_InitializeWall(ABuildingBase* BuildingA, ABuildingBase* BuildingB);

	/**
	 * Updates the visibility of the wall components based on the visibility of the connected buildings.
	 */
	UFUNCTION(BlueprintCallable, Category = "EnergyWall")
	void UpdateVisibility();

	/**
	 * Internal initialization of the wall, called on server and client.
	 */
	void InitializeWallInternal();

	/**
	 * Initializes the ISMs for the wall.
	 */
	UFUNCTION(BlueprintCallable, Category = "EnergyWall")
	void InitializeISMs();

	/**
	 * Initializes an additional ISM for the wall.
	 * @param InISMComponent The ISM component to initialize.
	 * @return The index of the added or updated instance.
	 */
	UFUNCTION(BlueprintCallable, Category = "EnergyWall")
	int32 InitializeAdditionalISM(UInstancedStaticMeshComponent* InISMComponent);

	/**
	 * Starts the despawn process, notifying the navigation system and applying visual effects via materials.
	 * @param DestroyedActor The actor whose destruction triggered this call.
	 */
	UFUNCTION()
	void StartDespawn(AActor* DestroyedActor);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyWall")
	float DespawnDelay = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyWall")
	FName DespawnStartTimeParameterName = "DespawnStartTime";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyWall|Navigation")
	float MinThickness = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyWall|Navigation")
	float MaxThickness = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyWall|Navigation")
	float MinPadding = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyWall|Navigation")
	float MaxPadding = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyWall|Navigation")
	float NavigationZPadding = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyWall|Navigation")
	float AgentRadiusPadding = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyWall|Navigation")
	float DirtyAreaExpansion = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyWall|Visual")
	float InitializationDuration = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyWall|Visual")
	bool bFlickerOnInitialize = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyWall|Visual")
	bool bFlickerOnDespawn = true;

	UFUNCTION(BlueprintPure, Category = "EnergyWall")
	ABuildingBase* GetBuildingA() const { return CachedBuildingA; }

	UFUNCTION(BlueprintPure, Category = "EnergyWall")
	ABuildingBase* GetBuildingB() const { return CachedBuildingB; }

private:
	UPROPERTY(Replicated)
	ABuildingBase* CachedBuildingA;

	UPROPERTY(Replicated)
	ABuildingBase* CachedBuildingB;

	bool bIsInitialized = false;

	void RegisterObstacle(float Length, float Height);

	UFUNCTION()
	void OnInitializationTimerComplete();

	FTimerHandle InitializationTimerHandle;
	float TargetScaleY = 1.0f;
	float CurrentScaleY = 0.0f;
	float TargetDistance2D = 0.0f;
	float TargetWallHeight = 0.0f;
	bool bIsInitializing = false;
	bool bIsDespawning = false;
	bool bIsVisibleByFoW = false;
};
