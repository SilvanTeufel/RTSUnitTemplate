// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TransportUnit.h"
#include "GameFramework/Character.h"
#include "Components/WidgetComponent.h"
#include "Core/UnitData.h"
#include "Actors/WorkArea.h"
#include "Actors/WorkResource.h"
#include "PathSeekerBase.h"
#include "Core/WorkerData.h"
#include "WorkingUnitBase.generated.h"


UCLASS()
class RTSUNITTEMPLATE_API AWorkingUnitBase : public ATransportUnit
{
	GENERATED_BODY()
private:
	
	FTimerHandle ShowWorkAreaTimerHandle;
	
public:
	virtual void BeginPlay() override;

	virtual void Destroyed() override;

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;
	
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category=Worker)
	void SpawnWorkArea(TSubclassOf<AWorkArea> WorkAreaClass, AWaypoint* Waypoint);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category=Worker)
	void ServerSpawnWorkArea(TSubclassOf<AWorkArea> WorkAreaClass, AWaypoint* Waypoint, FVector HitLocation);

	UFUNCTION(BlueprintCallable, Category=Worker)
	AWorkArea* SpawnWorkAreaReplicated(TSubclassOf<AWorkArea> WorkAreaClass, AWaypoint* Waypoint, FVector SpawnLocation, const  FBuildingCost ConstructionCost, bool IsPaid = false, TSubclassOf<class AUnitBase> ConstructionUnitClass = nullptr, bool IsExtensionArea = false);
	
	UFUNCTION(Client, Reliable)
	void ClientReceiveWorkArea(AWorkArea* ClientArea);

	/**
	 * While this worker is building it can neither be detected nor damaged.
	 *
	 * Off by default and meant for a single faction: the Xeno Brood-Mite is consumed by its own
	 * build, so losing it halfway costs the Xeno the worker AND the building. The other factions
	 * keep their workers and are deliberately left vulnerable.
	 *
	 * Detection reuses the existing stealth mechanic (bCanBeInvisible / bIsInvisible in
	 * FMassAgentCharacteristicsFragment), so a detector unit still finds it - that is the same rule
	 * every other invisible unit follows. The damage block sits in AUnitBase::SetHealth.
	 */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = Worker)
	bool bProtectedWhileBuilding = false;

	/** True between entering and leaving the Build state, only for bProtectedWhileBuilding workers. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = Worker)
	bool bBuildProtectionActive = false;

	/**
	 * Effect scale that matches the footprint of this worker's BuildArea.
	 *
	 * The finish-build effect used a fixed scale, so it looked the same over a small pod and over a
	 * hive. Takes the larger of the area's X/Y extent and expresses it as a multiple of ReferenceSize.
	 * Falls back to BaseScale when there is no build area to measure.
	 */
	UFUNCTION(BlueprintPure, Category = Worker)
	FVector GetBuildAreaEffectScale(float BaseScale = 1.f, float ReferenceSize = 300.f,
	                                float MinScale = 0.5f, float MaxScale = 6.f) const;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = Worker)
	class AWorkArea* ResourcePlace;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = Worker)
	class ABuildingBase* Base;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = Worker)
	class AWorkArea* BuildArea;
	
	UPROPERTY(ReplicatedUsing=OnRep_CarryingResourceType, EditAnywhere, BlueprintReadWrite, Category = Worker)
	EResourceType CarryingResourceType = EResourceType::MAX;

	UFUNCTION()
	void OnRep_CarryingResourceType();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Worker)
	TArray<FWorkResourceVisuals> WorkResourceVisuals;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = Worker)
	AWorkResource* WorkResource;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	EResourceType ExtractingWorkResourceType = EResourceType::Primary;

	// --- Which resources may this worker gather? (Details panel) ---------------------------
	// Off by default, which keeps the historical behavior: the worker may mine EVERY resource
	// type. Switch it on to restrict the worker to MineableResourceTypes.
	// Config only - set it on the Blueprint default. It is not replicated (clients read the same
	// CDO value); change it at runtime on the SERVER only, where the worker AI runs.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Worker|Resources")
	bool bRestrictMineableResources = false;

	// The resource types this worker is allowed to gather. Only used when
	// bRestrictMineableResources is true. An EMPTY list then means "this worker mines nothing".
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Worker|Resources", meta = (EditCondition = "bRestrictMineableResources"))
	TArray<EResourceType> MineableResourceTypes;

	// True if this worker may gather ResourceType. Always true while bRestrictMineableResources
	// is off. EResourceType::MAX ("nothing") is never mineable.
	UFUNCTION(BlueprintPure, Category = "Worker|Resources")
	bool CanMineResourceType(EResourceType ResourceType) const;

	// Convenience wrapper: resolves the WorkArea's type and defers to CanMineResourceType.
	// A null area returns false.
	UFUNCTION(BlueprintPure, Category = "Worker|Resources")
	bool CanMineWorkArea(const AWorkArea* Area) const;

	// The resource type this worker should be routed by right now: what it is carrying if it
	// carries anything, otherwise the type of its assigned ResourcePlace, otherwise MAX
	// ("unknown" - callers then apply no base filtering).
	UFUNCTION(BlueprintPure, Category = "Worker|Resources")
	EResourceType GetRoutingResourceType() const;

	// True if InBase is a valid drop-off for what this worker carries (or is about to fetch).
	// A null base returns false.
	UFUNCTION(BlueprintPure, Category = "Worker|Resources")
	bool CanDeliverToBase(const ABuildingBase* InBase) const;

	// Assigns this worker's mining node and ALWAYS releases the slot it held on the previous node.
	//
	// Use this instead of writing ResourcePlace directly. A bare `Worker->ResourcePlace = X` leaves the
	// worker registered in the old node's Workers array forever, and since AWorkArea::CurrentWorkers
	// (what the HUD draws) is just Workers.Num(), the old node keeps showing phantom miners - a node
	// reading "2/3" with nobody actually working it. Nothing reconciles those arrays afterwards:
	// AWorkArea::OnOverflowTimer is only armed for BuildAreas (AWorkArea::InitWorkerOverflowTimer).
	//
	// bRegisterOnNewPlace additionally enters the worker in the new node's Workers array.
	// Server-side only; the arrays and CurrentWorkers are server-authoritative and replicated.
	UFUNCTION(BlueprintCallable, Category = "Worker|Resources")
	void SetResourcePlace(AWorkArea* NewPlace, bool bRegisterOnNewPlace = false);

	// Releases the slot on the current node (if any) and clears ResourcePlace. Equivalent to
	// SetResourcePlace(nullptr), spelled out for call sites that just mean "stop mining here".
	UFUNCTION(BlueprintCallable, Category = "Worker|Resources")
	void ReleaseResourcePlace();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Worker)
	float ResourceExtractionTime = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Worker)
	bool AutoMining = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Worker)
	bool CanRepair = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Worker)
	bool CanBeRepaired = false;

	// Additional repair parameters
	// Distance buffer a worker needs to be from the FollowUnit (on top of both capsule radii) to start repairing
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Worker)
	float RepairDistance = 100.f;  // Kapselradien plus diesen Abstand - Nutzerwunsch 50 bis 100
	
	// Amount of health restored per second while repairing
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Worker)
	float RepairHealth = 10.f;
	
	UPROPERTY(ReplicatedUsing=OnRep_CurrentDraggedWorkArea, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	AWorkArea* CurrentDraggedWorkArea;

	UFUNCTION()
	void OnRep_CurrentDraggedWorkArea();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UNiagaraComponent* Niagara_Build;

	UFUNCTION(BlueprintImplementableEvent, Category="RTSUnitTemplate")
	void StartBuild();


	UFUNCTION(BlueprintImplementableEvent, Category="RTSUnitTemplate")
	void FinishedBuild();

	virtual void SetCharacterVisibility(bool desiredVisibility) override;
	virtual void SyncAttachedAssetsVisibility() override;
};








