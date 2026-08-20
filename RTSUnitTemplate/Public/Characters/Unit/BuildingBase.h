// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilityUnit.h"
#include "NavModifierVolume.h"
#include "GameFramework/Character.h"
#include "Components/WidgetComponent.h"
#include "Core/UnitData.h"
#include "Actors/WorkArea.h"
#include "Actors/WorkResource.h"
#include "PathSeekerBase.h"
#include "UnitBase.h"
#include "BuildingBase.generated.h"

class AEnergyWall;

UENUM(BlueprintType)
enum class EExtensionSnapMethod : uint8
{
	None      UMETA(DisplayName = "Fixed Offset (No Snap)"),
	Snap1Way  UMETA(DisplayName = "1-Way Snap (Fix Direction)"),
	Snap2Way  UMETA(DisplayName = "2-Way Snap (180 Degree)"),
	Snap4Way  UMETA(DisplayName = "4-Way Snap (90 Degree)"),
	Snap8Way  UMETA(DisplayName = "8-Way Snap (45 Degree)")
};

UCLASS()
class RTSUNITTEMPLATE_API ABuildingBase : public AUnitBase
{
	GENERATED_BODY()
	
public:

	// Constructor declaration
	ABuildingBase(const FObjectInitializer& ObjectInitializer);
	
	// Rotate a Niagara component to face the Origin (AWorkingUnitBase) of the WorkArea, with an optional rotation offset.
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = Construction)
	void MulticastRotateNiagaraToOrigin(UNiagaraComponent* NiagaraToRotate, const FRotator& RotationOffset, float InRotateDuration, float InRotationEaseExponent, ERotationAxis AxisSelection = ERotationAxis::Full);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool RunTimeCustomDepthSwitch = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool HasWaypoint = true;

	// --- Rally / waypoint line origin (HUD) ---------------------------------
	// Local-space offset (rotated by the building's actor rotation) from which the rally/waypoint
	// line is drawn in AHUDBase::DrawSelectedBuildingWaypointLinks. Lets designers lift the line off
	// the pivot to a sensible spot (door, flag, roof). Zero = legacy pivot behavior. If this is left
	// at zero AND WaypointLineOriginSocket does not resolve, the HUD's WPLineDefaultOriginOffset is
	// used instead.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Waypoint")
	FVector WaypointLineOriginOffset = FVector::ZeroVector;

	// Optional socket/bone on this building's primary skeletal mesh (ACharacter::GetMesh()). When set
	// and it resolves on a FINISHED building, its world location overrides WaypointLineOriginOffset.
	// Ignored for construction sites and for ISM-only buildings whose skeletal mesh has no such socket
	// — those fall back to the offset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Waypoint")
	FName WaypointLineOriginSocket = NAME_None;

	// Optional local-space offset (rotated by the WAYPOINT's actor rotation) applied to the END of the
	// rally line. Symmetric target control; zero keeps the historical waypoint-actor location.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Waypoint")
	FVector WaypointLineTargetOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool CancelsAbilityOnRightClick = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsBase = false;

	/**
	 * Resource capacity this building adds to its team while it stands, and gives back when it dies.
	 *
	 * Mainly for supply: units cost Rare, which is supply-like, so without a building that raises the Rare
	 * cap a faction simply cannot train anything once it reaches the starting limit. The Singularian Reactor
	 * does this from its own Blueprint; expressing it as data means any building can, without duplicating
	 * that graph. Leave at zero for buildings that grant nothing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Supply")
	FBuildingCost SupplyCapacityGain;

	/** Upper bound for the capacity this building may contribute to, matching the Reactor's own cap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Supply")
	float SupplyCapacityLimit = 150.f;

	// --- Rally point ------------------------------------------------------------------------
	// Where units this building produces go once they exist. Without one they are left standing on
	// the spawn spot, which is inside the base, so they block the next production and the workers
	// squeezing past them.

	/** False means "no explicit point set", and GetRallyPointLocation falls back to the base edge. */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Rally")
	bool bHasRallyPoint = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Rally")
	FVector RallyPointLocation = FVector::ZeroVector;

	/** Set when the rally point was placed on a deposit: produced workers start mining there directly. */
	UPROPERTY(BlueprintReadWrite, Category = "RTSUnitTemplate|Rally")
	TObjectPtr<class AWorkArea> RallyResourceArea = nullptr;

	/** How far out the default (unset) rally point sits - the "edge of the base". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Rally")
	float DefaultRallyDistance = 900.f;

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Rally")
	void SetRallyPoint(FVector NewLocation, class AWorkArea* ResourceArea);

	/** The explicit point when one is set, otherwise a point on the base edge facing away from home. */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Rally")
	FVector GetRallyPointLocation() const;

	/** Sends a newly produced unit on its way; workers to the rallied deposit, everything else to the point. */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Rally")
	void ApplyRallyPointToUnit(AUnitBase* NewUnit);

	/**
	 * Gives the granted capacity back.
	 *
	 * Public because Destroyed() is not enough: a building that is killed in combat only goes to the
	 * Dead state - the actor stays around as a ruin - so the capacity was never handed back and a team
	 * kept the supply of buildings it had already lost. AUnitBase calls this from the death path too;
	 * bSupplyCapacityApplied makes the double call harmless.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Supply")
	void ReleaseSupplyCapacity();

	/**
	 * Transporter buildings that only produce while staffed (Antimatter, MetabolicSiphon) pull their crew
	 * in themselves shortly after completion. The AI could never do it reliably: its click lands under its
	 * camera, and the workers it selects get re-tasked by the next decision tick before they arrive.
	 * Only fills up to MaxTransportUnits and only takes this team's own workers.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Transport")
	bool bAutoLoadNearbyWorkers = false;

	/** Search radius for bAutoLoadNearbyWorkers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Transport",
	          meta = (EditCondition = "bAutoLoadNearbyWorkers"))
	float AutoLoadWorkerRadius = 6000.f;

	/** Delay before the auto-load runs; TeamId is only assigned after spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Transport",
	          meta = (EditCondition = "bAutoLoadNearbyWorkers"))
	float AutoLoadDelaySeconds = 1.5f;

	/**
	 * How long into a match the auto-load keeps re-asking who owns this team before settling on
	 * "human player".
	 *
	 * Controllers are not all in place during the first seconds: a building placed in the level starts
	 * its timer before the AI controller for its team exists. Measured in Level_AITest_Xeno_AH - at
	 * 1.5 s team 2 answers "player", twenty seconds later the same team answers "AI". A building the AI
	 * actually constructs is finished long after that and answers correctly on the first try, so this
	 * only costs level-placed transporters a few seconds.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Transport",
	          meta = (EditCondition = "bAutoLoadNearbyWorkers"))
	float AutoLoadOwnerResolveSeconds = 10.f;

	/**
	 * The worker that finished this building, handed over by the build-completion path.
	 *
	 * Only used for the human player's auto-load: a player who places a reactor expects the one worker
	 * they sent to walk in, not four of them pulled off their resource nodes without being asked. Weak,
	 * because that worker may well be dead by the time the load timer fires.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|Transport")
	TWeakObjectPtr<AUnitBase> BuilderWorker;

	/**
	 * Everyone who worked on this building's site, handed over at completion.
	 *
	 * A site can have several workers, and a player who sent three of them expects all three to walk
	 * in - not one, with the rest wandering back to the nearest ore. BuilderWorker (the one who
	 * finished it) stays as the fallback for sites that never registered a worker list.
	 */
	// Ohne BlueprintReadOnly: UHT laesst TArray<TWeakObjectPtr<>> nicht ins Blueprint durch
	// ("is not supported by blueprint"). Als reines UPROPERTY bleibt es fuer die Serialisierung
	// sichtbar, und gebraucht wird es ohnehin nur im C++-Ladeweg.
	UPROPERTY()
	TArray<TWeakObjectPtr<AUnitBase>> BuilderWorkers;

	/**
	 * Loads own workers once the building is up.
	 *
	 * AI teams get filled to MaxTransportUnits - see bAutoLoadNearbyWorkers for why they cannot do it by
	 * hand. Human teams only get BuilderWorker, because for them this is a convenience, not a crutch.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Transport")
	void AutoLoadNearbyWorkers();

	/** True if the controller owning this building's team is flagged bIsAi. Twin of AWorkArea::IsOwnedByAiTeam. */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Transport")
	bool IsOwnedByAiTeam() const;

protected:
	/** Grants SupplyCapacityGain once the team id is known. */
	void ApplySupplyCapacity();

	/** Guards against granting twice and against giving back what was never granted. */
	bool bSupplyCapacityApplied = false;

public:

	// --- Which resources may be delivered here? (Details panel) ----------------------------
	// Only meaningful while IsBase is true. Off by default, which keeps the historical
	// behavior: this base accepts EVERY resource type.
	// Config only - set it on the Blueprint default. It is not replicated (clients read the same
	// CDO value); change it at runtime on the SERVER only, where the worker AI runs.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Resources")
	bool bRestrictAcceptedResources = false;

	// The resource types a worker may drop off here. Only used when bRestrictAcceptedResources
	// is true. An EMPTY list then means "this base accepts nothing".
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Resources", meta = (EditCondition = "bRestrictAcceptedResources"))
	TArray<EResourceType> AcceptedResourceTypes;

	// True if a worker may deliver ResourceType to this base. Always true while
	// bRestrictAcceptedResources is off. EResourceType::MAX ("carrying nothing") always passes,
	// so an empty-handed worker can still walk home to be re-dispatched.
	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Resources")
	bool AcceptsResourceType(EResourceType ResourceType) const;

	// Per-building adjustment to controller SnapGap (can be negative). Effective gap is clamped to >= 0.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BuildingSnap)
	float SnapGapAdjustment = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float BeaconRange = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector ExtensionOffset = FVector(20.f, 20.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool ExtensionGroundTrace = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	EExtensionSnapMethod ExtensionSnapMethod = EExtensionSnapMethod::Snap4Way;

	/**
	 * Extra yaw for extensions of THIS building, on top of the snap direction.
	 *
	 * The snap turns an extension so its local +X faces away from the host. Some buildings want it
	 * turned across that instead - the BioIntegrator attaches on its short, angular end, where the
	 * extension only sits flush when it is rotated 90 degrees. This rotates the facing only; the
	 * extension stays on the side the snap picked. The work area carries its own rotation over into
	 * ServerMeshRotationBuilding, so the finished building turns with the preview - one value, both.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float ExtensionRotationOffset = 0.f;

	/**
	 * Derive the extension distance from the two footprints instead of the length of ExtensionOffset.
	 *
	 * A single fixed offset per host cannot be right when one building constructs extensions of very
	 * different size - the CybernaticFactory places a 177 and a 623 unit wide one on the same edge,
	 * so any single value leaves one of them either buried or floating. With this on, ExtensionOffset
	 * only says WHICH axis and direction to use (and keeps its Z); the distance comes from the real
	 * meshes. Off by default so existing projects keep their hand-tuned values.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bExtensionAutoDistance = false;

	/** Clearance between host and extension when bExtensionAutoDistance is on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate,
	          meta = (EditCondition = "bExtensionAutoDistance"))
	float ExtensionGap = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool ExtensionMovementAllowed = false;
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	ABuildingBase* Extension = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	ABuildingBase* Origin = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<AEnergyWall> EnergyWallClass;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<AEnergyWall*> EnergyWallArray;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SpawnEnergyWall(TSubclassOf<AEnergyWall> InEnergyWallClass, ABuildingBase* InOrigin);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetEnergyWallsActive(bool bActive);

	UFUNCTION(BlueprintPure, Category = RTSUnitTemplate)
	bool GetEnergyWallActive() const;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetBeaconRange(float NewRange);
	
	virtual void Tick(float DeltaTime) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void BeginPlay() override;
	
	virtual void Destroyed() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void SyncAttachedAssetsVisibility() override;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void OnOverlapBegin(
		UPrimitiveComponent* OverlappedComp, 
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp, 
		int32 OtherBodyIndex,
		bool bFromSweep, 
		const FHitResult& SweepResult
	);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleBaseArea(AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode, bool CanAffordConstruction);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SwitchResourceArea(AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode, int32 RecursionCount = 0);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool SwitchBuildArea(AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void DespawnWorkResource(AWorkResource* ResourceToDespawn);


	virtual void SetEnemyVisibility(AActor* DetectingActor, bool bVisible) override;

	// Returns true if this building's location is within range of any Beacon (any BuildingBase with BeaconRange > 0)
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool IsInBeaconRange() const;

	// Utility: Returns true if the given world location is within range of any Beacon (any BuildingBase with BeaconRange > 0)
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	static bool IsLocationInBeaconRange(UWorld* World, const FVector& Location);
};








