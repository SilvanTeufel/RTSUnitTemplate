// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
//#include "Characters/Unit/UnitBase.h"
#include "Components/SceneComponent.h"
#include "GameplayEffect.h"
#include "TimerManager.h"
#include "Core/WorkerData.h"
#include "Core/UnitData.h"
#include "Components/StaticMeshComponent.h"
#include "WorkArea.generated.h"

class AUnitBase;
class AWorkingUnitBase;
class UMaterialInstanceDynamic;
class UTexture;

UCLASS()
class RTSUNITTEMPLATE_API AWorkArea : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AWorkArea();

	FTimerHandle HideWorkAreaTimerHandle;
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	// Clear timers only on EndPlay
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	// Optional: if provided, a construction site will be spawned and tracked during build
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = Construction)
	TSubclassOf<class AUnitBase> ConstructionUnitClass;
	
	// Pointer to the active construction site actor (if any)
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = Construction)
	class AUnitBase* ConstructionUnit = nullptr;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = Construction)
	FVector ScaleConstructionUnit = FVector(1.f, 1.f, 1.f);

	// --- Per-build-site vertical tuning for DroneBehavior construction sites ---
	// Shifts the drone's whole vertical band up(+)/down(-) in world units, on top of the mesh-base
	// anchor. Use a negative value when the drone hovers too high for a particular building. (Too
	// large a negative value can push the band below ground, where SafeMin clamps it.)
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Construction|Drone")
	float DroneVerticalOffset = 0.f;

	// When > 0, overrides the drone's "building height" (otherwise BoxExtent.Z * 2 from the WorkArea
	// mesh bounds). Set this to the real finished-building height when the WorkArea mesh bounds are
	// taller than the building, so the drone scan range / SafeMin don't scale up and float too high.
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Construction|Drone")
	float DroneBuildingHeightOverride = 0.f;
	
	UFUNCTION(Server, Reliable,BlueprintCallable, Category = RTSUnitTemplate)
	void RemoveAreaFromGroup();
	
	UFUNCTION(Server, Reliable, Category = RTSUnitTemplate)
	void AddAreaToGroup();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void InitWorkerOverflowTimer();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FString Tag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UCapsuleComponent* TriggerCapsule;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USceneComponent* SceneRoot;

	float GetCollisionRadiusInDirection(const FVector& Direction) const;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UStaticMeshComponent* Mesh;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int TeamId = 0;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsNoBuildZone = false;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<UGameplayEffect> AreaEffect;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category = RTSUnitTemplate)
	//class UCapsuleComponent* TriggerCapsule;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleResourceExtractionArea(AUnitBase* UnitBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleBaseArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode, bool CanAffordConstruction);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SwitchResourceArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool SwitchBuildArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleBuildArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode, bool CanAffordConstruction);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	EResourceType ConvertWorkAreaTypeToResourceType(WorkAreaData::WorkAreaType WorkAreaType);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void DespawnWorkResource(AWorkResource* WorkResource);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TEnumAsByte<WorkAreaData::WorkAreaType> Type = WorkAreaData::Primary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<class AWorkResource> WorkResourceClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USoundBase* ExtractionSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float ExtractionSoundFadeOutDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float ExtractionSoundVolume = 1.0f;
	
	// Replicated so the owning client can read the finished building's HasWaypoint flag
	// (used to gate the construction-site rally-waypoint feature) before the server RPC.
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<class ABuildingBase> BuildingClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	ABuildingBase* Building;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float BuildTime = 5.f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = Worker)
	float CurrentBuildTime = 0.0f;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float AvailableResourceAmount = 200.f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float MaxAvailableResourceAmount = AvailableResourceAmount;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool ShrinkResource = true;

	UPROPERTY()
	FVector OriginalActorScale = FVector(1.f);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float BuildZOffset = 200.f;

	// Tracks how much build progress has already been converted into health for the ConstructionUnit
 // Not replicated/persisted intentionally -- used only to compute additive health gains during build syncing
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = Worker)
	float LastAppliedBuildProgress = 0.f;

 // Ensures the ConstructionUnit spawns only once per build session (5-10% window)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Construction)
	bool bConstructionUnitSpawned = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool PlannedBuilding = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool StartedBuilding = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    bool DestroyAfterBuild = true;

	/**
	 * Marks this as a DEFENSE build area (tower, spore, bunker...). Such areas are pushed toward the
	 * nearest enemy base on drop, because the AI otherwise places them behind its own base - it drops
	 * buildings under its camera, and the camera sits back with the workers. Measured: defense buildings
	 * ended up ~2300 units BEHIND the base centre while the rest of the base reached ~930 forward.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Construction)
	bool bIsDefenseArea = false;

	/**
	 * A build area that nobody ever starts is abandoned after this many seconds: it is destroyed and,
	 * if it had already been paid for, the ConstructionCost is refunded. Prevents stray areas from
	 * sitting on the map forever and blocking placement. Set <= 0 to disable.
	 *
	 * CURRENTLY 0 (off) while tracking down the Xeno collapse: with the defence push already ruled out,
	 * this timer is the next suspect - in the failing runs the Xeno base never grew past 4-6 buildings
	 * (14-22 in the good ones), which is what an over-eager cleanup looks like.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Construction)
	float AbandonTimeoutSeconds = 0.f;

	/** Server-side timer for AbandonTimeoutSeconds. */
	FTimerHandle AbandonTimerHandle;

	/** Destroys this area (with refund) if no worker/ConstructionUnit ever picked it up. */
	UFUNCTION()
	void AbandonIfUnclaimed();

	/**
	 * AI ONLY: how long a build area may stay orphaned before it is removed and refunded.
	 *
	 * Difference to AbandonTimeoutSeconds above: that one is a ONE-SHOT timer armed at BeginPlay, so it
	 * only ever catches areas that were never claimed at all. It cannot see an area that HAD a builder
	 * and lost it - worker killed on the way, worker pulled off by a group order - which is exactly the
	 * case the human player reported ("WorkAreas an denen keiner baut und keine ConstructionUnit ist").
	 * This check is recurring and ignores StartedBuilding, so a half-started area whose builder died is
	 * cleaned up too. <= 0 disables.
	 *
	 * The human player's areas are never touched by this - they are their own to manage.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Construction)
	float AiOrphanTimeoutSeconds = 45.f;

	/**
	 * Longer grace period for an area that was NEVER claimed by anyone. Such an area is not necessarily
	 * dead - the AI drops it and the worker is only assigned once one is free, which can take a while.
	 * Deleting those on the short timeout is exactly the over-eager cleanup that once cut the Xeno base
	 * from ~14-22 buildings down to 5-7.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Construction)
	float AiUnclaimedTimeoutSeconds = 150.f;

	/** Poll interval for AiOrphanTimeoutSeconds. Kept coarse: the claim scan walks all units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Construction)
	float AiOrphanCheckInterval = 10.f;

	/** Server-side timer + accumulator for AiOrphanTimeoutSeconds. */
	FTimerHandle AiOrphanTimerHandle;
	float AiOrphanElapsed = 0.f;

	/** Set once this area has been claimed by anyone; picks the short timeout from then on. */
	bool bAiOrphanWasClaimed = false;

	UFUNCTION()
	void TickAiOrphanCheck();

	/** True if the team owning this area is played by an AI controller (not the human). */
	bool IsOwnedByAiTeam() const;

    // Guard to ensure the final building is spawned only once per build session
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Construction)
    bool bFinalBuildingSpawned = false;
 	
 	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
 	TArray<AWorkingUnitBase*> Workers;

	// Max workers that may mine this resource at once (enforced at the mining slot). <= 0 = unlimited.
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int MaxWorkerCount = 3;

	// Workers currently occupying this resource (kept = Workers.Num()). Replicated so the HUD can draw
	// the "N/Max" count over resource nodes without a server round-trip. 0 => the HUD draws nothing.
	UPROPERTY(ReplicatedUsing = OnRep_WorkerCount, BlueprintReadOnly, Category = RTSUnitTemplate)
	int32 CurrentWorkers = 0;

	UFUNCTION()
	void OnRep_WorkerCount();

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void StartedBuild();

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void StartedResourceExtraction();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FBuildingCost ConstructionCost;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float ResetStartBuildTime = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float ControlTimer = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsPaid = false;
	//UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	//bool CanAffordConstruction(int32 TeamId, int32 NumberOfTeams, TArray<FResourceArray> TeamResources);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	class AWaypoint* NextWaypoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Worker)
	FRotator ServerMeshRotationBuilding = FRotator (0.f, -90.f, 0.f);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetScale(FVector NewScale);

	// -- NEW TIMER HANDLE & PROPERTIES --
	/** Timer handle for reverting the material change. */
	FTimerHandle ChangeMaterialTimerHandle;

	/** Material to apply temporarily for 3 seconds when TemporarilyChangeMaterial is called. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UMaterialInterface* TemporaryHighlightMaterial;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void TemporarilyChangeMaterial();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Material")
	FName MaterializeParameterName = FName(TEXT("Materialize Amount"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Material")
	FName OffsetParameterName = FName(TEXT("Materialize Amount"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Material")
	float OffsetParameterValue = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Material")
	float MaterializePower = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Material")
	FName BaseTexParameterName = FName(TEXT("Base Texture"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Material")
	UTexture* BaseTexParameterValue = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Material")
	FName MetallicTexParameterName = FName(TEXT("Metallic Texture"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Material")
	UTexture* MetallicTexParameterValue = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Material")
	FName NormalTexParameterName = FName(TEXT("Normal Texture"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Material")
	UTexture* NormalTexParameterValue = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Material")
	FName SpecTexParameterName = FName(TEXT("Specular Texture"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Material")
	UTexture* SpecTexParameterValue = nullptr;

	UPROPERTY(ReplicatedUsing = OnRep_MIDEnabled)
	bool bMIDEnabled = false;

	UFUNCTION()
	void OnRep_MIDEnabled();

	UPROPERTY()
	UMaterialInstanceDynamic* WorkAreaMID;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void EnableMID();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UMaterialInterface* BuildMaterial;
public:
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void AddWorkerToArray(class AWorkingUnitBase* Worker);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void RemoveWorkerFromArray(class AWorkingUnitBase* Worker);

	// True if Worker could take a mining slot on Area right now: Area is valid, not depleted, and either
	// uncapped (MaxWorkerCount <= 0), below MaxWorkerCount, or already holding a slot for this worker.
	// Every assignment path must consult this BEFORE routing a worker, otherwise the worker walks to a
	// full deposit and is bounced by ReserveMiningSlotOrReassign on arrival.
	// A null Worker only checks the free-capacity part.
	UFUNCTION(BlueprintPure, Category = RTSUnitTemplate)
	static bool HasFreeMiningSlotFor(const AWorkArea* Area, const AWorkingUnitBase* Worker);

	// Server-only. Ensures the worker holds one of the MaxWorkerCount mining slots on this node: the
	// first MaxWorkerCount workers (by Workers order) may mine; any overflow worker is reassigned to
	// another deposit it is allowed to work (else sent Idle). Returns true if the worker may
	// mine here now, false if it was reassigned/idled (caller must NOT start extraction).
	//
	// OutFollowUpSignal: pass a non-null pointer when calling from inside a Mass command-buffer window
	// that is already removing state tags for this entity (UUnitStateProcessor::SwitchState does exactly
	// that). The function then does NOT touch Mass tags itself and instead reports the signal the caller
	// must fire afterwards - UnitSignals::GoToResourceExtraction or UnitSignals::Idle, or NAME_None.
	// Applying the tag directly in that situation is silently undone, because the enclosing
	// RemoveTag<...> is flushed after this AddTag<...>, leaving the entity with NO state tag at all -
	// which is precisely the "worker freezes on arrival at a full deposit" bug.
	bool ReserveMiningSlotOrReassign(class AWorkingUnitBase* Worker, FName* OutFollowUpSignal = nullptr);

	/** Duration after which a worker added to this WorkArea should be sent back to base and removed (defaults to BuildTime if <= 0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float WorkerReturnDelay = 1.f;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UMaterialInterface* OriginalMaterial;
    
	/** Called by a timer to revert the material back to its original state. */
	void RevertMaterial();

	void SetupMID();

	// Internal: timer callback to process overflow workers and shrink the array to MaxWorkerCount
	void OnOverflowTimer();

	// Single timer handle used to process overflow workers
	FTimerHandle OverflowWorkersTimerHandle;

public:
	// Placement constraint: when true, this WorkArea cannot be placed closer than ResourcePlacementDistance to any resource WorkArea (Primary..Legendary)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool DenyPlacementCloseToResources = false;

	// Minimum center-to-center distance to resource WorkAreas when PlacementCloseToResources is enabled
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float ResourcePlacementDistance = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool NeedsBeacon = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool AllowAddingWorkers = true;

	/**
	 * Nur Diagnose: hoechste je gleichzeitig eingetragene Arbeiterzahl und der Zeitpunkt der
	 * Entstehung. Beim Abraeumen der Flaeche wird daraus eine Zeile, die beantwortet, ob eine nie
	 * fertig gewordene Baustelle ueberhaupt je einen Arbeiter gesehen hat.
	 */
	int32 DiagMaxWorkers = 0;
	float DiagGeburtszeit = -1.f;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsExtensionArea = false;

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	AWorkingUnitBase* Origin = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool KillOrigin = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool ResultCanBeSelected = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool InstantDrop = false;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool AreaDropped = false;

	// Server-only marker: set on WorkArea ghosts re-armed by the Shift-chain placement. When such
	// a ghost is dropped, DropWorkAreaForUnit does NOT dispatch the placing worker; it enqueues the
	// area into the build group (PlannedBuilding=false) so the normal ReachedBase auto-assignment
	// (SwitchBuildArea) services it. Only the FIRST area of a chain dispatches the worker directly.
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bSkipWorkerDispatchOnDrop = false;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool GetAreaDropped() const { return AreaDropped; }
};
