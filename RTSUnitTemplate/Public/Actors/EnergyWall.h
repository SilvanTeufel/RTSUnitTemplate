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

	/** Gibt die Batch-Plaetze zurueck. Ohne das bliebe eine unsichtbare Instanz fuer immer belegt. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "EnergyWall")
	int32 TeamId;

	/**
	 * Laesst die Wand dem Hoehenunterschied ihrer beiden Tuerme folgen, statt waagerecht zu stehen.
	 *
	 * Vorher wurde die Hoehendifferenz mit `Direction.Z = 0` ausdruecklich weggeworfen - die Wand
	 * stand auch am Hang immer flach. Auf ebenem Grund aendert sich durch diesen Schalter NICHTS:
	 * dort ist die Differenz null und damit auch die Neigung.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bFollowTerrainSlope = true;

	/**
	 * Steigung der Wand je Laengeneinheit (dZ pro uu entlang der lokalen Y-Achse).
	 *
	 * Wird als Instanzdatum 0 an das Schildmaterial gereicht, das daraus die Scherung baut:
	 * WorldPositionOffset.Z = WallSlopePerUnit * lokales Y. Damit bleiben die Seitenkanten
	 * senkrecht und nur Ober- und Unterkante laufen schraeg - das gewuenschte Trapez.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	float WallSlopePerUnit = 0.f;

	/** Waagerechte Richtung der Wand (normiert), fuer die Scherung im Material. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	FVector2D WallDirectionXY = FVector2D(1.f, 0.f);

	/** Reicht Steigung und Richtung an alle drei Batch-Instanzen weiter (Custom Data 2..4). */
	void SendeSteigungAnBatch();

	UPROPERTY(EditAnywhere, Category = "EnergyWall|Effects")
	TSubclassOf<class UGameplayEffect> FriendlyEffectClass;

	UPROPERTY(EditAnywhere, Category = "EnergyWall|Effects")
	TSubclassOf<class UGameplayEffect> EnemyEffectClass;

	/**
	 * Bepflanzung entlang der NavObstacleBox entfernen.
	 *
	 * WARUM DER TAG "Obstacle" HIER NICHT REICHT: die PCG-Graphen unter
	 * /Game/RTSUnits/Material/Landscape/PCG holen sich ueber GetActorData alle Aktoren mit diesem
	 * Tag und ziehen sie per Difference von der Streuflaeche ab - abgezogen werden dabei die
	 * BOUNDS des Aktors. Bei einem Gebaeude passt das. Bei einer Wand nicht: was den Boden
	 * versperrt, ist die NavObstacleBox, die sich zwischen die beiden Tuerme spannt, und die
	 * entsteht erst, wenn die Wand ihre Laenge kennt.
	 *
	 * Zweitens fragt PCG die Aktoren nur beim Erzeugen ab (bAlwaysRequeryActors = false). Eine im
	 * Spiel gebaute Wand erreicht den Graphen also ohnehin nicht mehr - deshalb wird hier direkt
	 * geraeumt, wie es die Gebaeude ueber ClearPCGInstancesInRadius auch tun.
	 */
	UFUNCTION(BlueprintCallable, Category = "EnergyWall|PCG")
	int32 RaeumePCGEntlangDerWand();

	/** Zusaetzliche Breite beim Freiraeumen, damit nichts direkt an der Wand klebt (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyWall|PCG")
	float PCGClearPadding = 120.f;

	/** Freiraeumen ueberhaupt durchfuehren. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyWall|PCG")
	bool bClearPCGAlongWall = true;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

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
	void InitializeAdditionalISM(UInstancedStaticMeshComponent* InISMComponent);

	/**
	 * Starts the despawn process, notifying the navigation system and applying visual effects via materials.
	 * @param DestroyedActor The actor whose destruction triggered this call.
	 */
	UFUNCTION()
	void StartDespawn(AActor* DestroyedActor);

	/**
	 * Deactivates the wall visually and for navigation, without destroying the actor.
	 */
	UFUNCTION(NetMulticast, Reliable, Category = "EnergyWall")
	void Multicast_DeactivateWall();

	/**
	 * Activates the wall visually and for navigation.
	 */
	UFUNCTION(NetMulticast, Reliable, Category = "EnergyWall")
	void Multicast_ActivateWall();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyWall")
	float DespawnDelay = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyWall")
	FName DespawnStartTimeParameterName = "DespawnStartTime";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyWall|Navigation")
	float MinThickness = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyWall|Navigation")
	float MaxThickness = 10.0f;

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

	UFUNCTION(BlueprintPure, Category = "EnergyWall")
	bool IsDeactivated() const { return bIsDeactivated; }

private:
	UPROPERTY(Replicated)
	ABuildingBase* CachedBuildingA;

	UPROPERTY(Replicated)
	ABuildingBase* CachedBuildingB;

	bool bIsInitialized = false;
	void UpdateWallTransformAndDimensions();
	void RegisterObstacle(float Length, float Height);

	void DeactivateNavigation();

	void ApplyDespawnEffects();

	void ActivateNavigation();

	UFUNCTION()
	void OnInitializationTimerComplete();

	UFUNCTION()
	void OnDeactivationTimerComplete();

	FTimerHandle InitializationTimerHandle;
	/**
	 * Plaetze im gemeinsamen Batch-ISM (UEnergyWallBatchSubsystem).
	 *
	 * Die drei eigenen ISM-Komponenten bleiben als Mesh- und Materialquelle bestehen - das Blueprint
	 * setzt sie -, werden aber nicht mehr gezeichnet. Gezeichnet wird ueber diese Indizes.
	 * INDEX_NONE heisst "nicht belegt".
	 */
	int32 BatchIndexTop = INDEX_NONE;
	int32 BatchIndexBottom = INDEX_NONE;
	int32 BatchIndexShield = INDEX_NONE;

	/** Meldet die drei Teile beim Batch an und blendet die eigenen Komponenten aus. */
	void MeldeBeimBatchAn();

	/** Gibt die drei Plaetze zurueck. Mehrfach aufrufbar. */
	void MeldeVomBatchAb();

	/** Schreibt Lage und Groesse der drei Teile in den Batch. */
	void SchreibeBatchTransformationen();

	/** Schaltet das Schild ueber Custom Data 1 sichtbar oder unsichtbar. */
	void SetzeSchildSichtbar(bool bSichtbar);

	/**
	 * Zuletzt gesetzte Schildsichtbarkeit.
	 *
	 * Frueher las der Code ShieldISM->bHiddenInGame zurueck. Im gemeinsamen Batch gibt es diesen
	 * Zustand je Instanz nicht mehr, also wird er hier gefuehrt - er ist die Umschaltquelle fuer das
	 * Flackern.
	 */
	bool bSchildZuletztSichtbar = false;

	float TargetScaleY = 1.0f;
	float CurrentScaleY = 0.0f;
	float TargetDistance2D = 0.0f;
	float TargetWallHeight = 0.0f;
	bool bIsInitializing = false;
	bool bIsDespawning = false;
	bool bIsVisibleByFoW = false;

	UPROPERTY(Replicated)
	bool bIsDeactivated = false;
};
