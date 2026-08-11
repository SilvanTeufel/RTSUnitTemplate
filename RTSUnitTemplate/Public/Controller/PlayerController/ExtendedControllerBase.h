// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

class AWorkArea;
class AActor;
class UStaticMeshComponent;
class USoundBase;
class AUnitBase;
class AAbilityIndicator;
class UGameplayAbilityBase;

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Controller/PlayerController/WidgetController.h"
#include "Core/WorkerData.h"
#include "Core/UnitData.h"
#include "MassEntityTypes.h"
#include "Components/AudioComponent.h"
#include "ExtendedControllerBase.generated.h"

struct FMassEntityManager;

USTRUCT()
struct FExtractionAudioData
{
	GENERATED_BODY()

	UPROPERTY()
	AWorkArea* WorkArea = nullptr;

	UPROPERTY()
	float Distance = MAX_FLT;

	UPROPERTY()
	float Volume = 0.f;

	UPROPERTY()
	float LastSignalTime = 0.f;

	FExtractionAudioData() {}
	FExtractionAudioData(AWorkArea* InArea, float InDist, float InVol, float InTime) 
		: WorkArea(InArea), Distance(InDist), Volume(InVol), LastSignalTime(InTime) {}
};


/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API AExtendedControllerBase : public AWidgetController
{
	GENERATED_BODY()
private:
	/** Handle for the timer that logs entity tags after BeginPlay. */

	UPROPERTY()
	TMap<EResourceType, FExtractionAudioData> SignaledExtractions;
	FDelegateHandle ExtractionSignalHandle;
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool LogSelectedTags = false;
	// Set to true when a keyboard ability was just executed; consumed on next left click
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	bool bUsedKeyboardAbilityBeforeClick = false;

	virtual void BeginPlay() override;
	
	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

	// Called by the engine when the viewport loses focus; key Released events are never delivered
	// in that case, so held ability inputs have to be released here or they stay stuck forever.
	virtual void FlushPressedKeys() override;

	//UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	//AWorkArea* CurrentDraggedWorkArea;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USoundBase* AbilitySound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USoundBase* AttackSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USoundBase* DropWorkAreaFailedSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USoundBase* DropWorkAreaSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sound)
	float ExtractionSoundDistance = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sound)
	float ExtractionSoundFadeInDuration = 1.0f;

protected:
	UPROPERTY()
	TMap<EResourceType, UAudioComponent*> ExtractionAudioComponents;

	void UpdateExtractionSounds(float DeltaSeconds);

	void LogSelectedUnitsMassTags();


	// Separate function for processing held abilities
	void ProcessHeldAbilities();

	UFUNCTION(Category = RTSUnitTemplate)
	void HandleExtractionSignal(FName SignalName, TArray<FMassEntityHandle>& Entities);

public:
	UPROPERTY()
	int32 AbilityIndicatorRefCount = 0;

	UFUNCTION(Client, Reliable, Category = RTSUnitTemplate)
	void Client_ApplyCustomizations(
		USoundBase* InWaypointSound,
		USoundBase* InRunSound,
		USoundBase* InAbilitySound,
		USoundBase* InAttackSound,
		USoundBase* InDropWorkAreaFailedSound,
		USoundBase* InDropWorkAreaSound);

	// Play a 2D sound only for this owning client (call from server or client)
	UFUNCTION(Client, Reliable, Category = RTSUnitTemplate)
	void Client_PlaySound2D(USoundBase* Sound, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f);

	void SetAbilityEnabledByKey(AUnitBase* UnitBase, const FString& Key, bool bEnable);
	// New variant that operates on a specific unit (no UFUNCTION to avoid UHT overloading conflicts)
	bool DropWorkAreaForUnit(class AUnitBase* UnitBase, bool bWorkAreaIsSnapped, USoundBase* InDropWorkAreaFailedSound);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool TryConnectEnergyWall(class AUnitBase* UnitBase, class AWorkArea* DraggedWorkArea, bool bIsSnapped = false);

	UFUNCTION(BlueprintPure, Category = RTSUnitTemplate)
	bool IsCompatibleForEnergyWall(class ABuildingBase* Initiator, class ABuildingBase* Target) const;

	// Spezialisierter Trace für Energiewände (Extensions). Prüft auf Blockaden durch Einheiten, Gebäude oder Hindernisse.
	bool WallTrace(class ABuildingBase* Unit, AActor* TargetActor, FVector& OutStart, FVector& OutEnd, float& OutTraceZOffset, AActor* IgnoreBuilding = nullptr);

	class ABuildingBase* GetBuildingBaseFromActor(AActor* Actor) const;

	UFUNCTION(Server, Reliable, Category = RTSUnitTemplate)
	void Server_DropWorkAreaForUnit(class AUnitBase* UnitBase, bool bWorkAreaIsSnapped, USoundBase* InDropWorkAreaFailedSound, FTransform ClientWorkAreaTransform);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagF1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagF2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagF3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagF4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagF5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagF6;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagCtrl1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagCtrl2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagCtrl3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagCtrl4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagCtrl5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagCtrl6;
	

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagCtrlQ;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagCtrlW;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagCtrlE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagCtrlR;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagAlt1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagAlt2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagAlt3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagAlt4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagAlt5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagAlt6;


	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void ActivateAbilitiesByIndex(AGASUnit* UnitBase, EGASAbilityInputID InputID, int32 InAbilityArrayIndex, const FHitResult& HitResult = FHitResult());

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void ActivateDefaultAbilities(AGASUnit* UnitBase, EGASAbilityInputID InputID, const FHitResult& HitResult = FHitResult());

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void ActivateSecondAbilities(AGASUnit* UnitBase, EGASAbilityInputID InputID, const FHitResult& HitResult = FHitResult());


	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void ActivateThirdAbilities(AGASUnit* UnitBase, EGASAbilityInputID InputID, const FHitResult& HitResult = FHitResult());


	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void ActivateFourthAbilities(AGASUnit* UnitBase, EGASAbilityInputID InputID, const FHitResult& HitResult = FHitResult());
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void ActivateAbilities(AGASUnit* UnitBase, EGASAbilityInputID InputID, const TArray<TSubclassOf<UGameplayAbilityBase>>& Abilities);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void ActivateDefaultAbilitiesByTag(EGASAbilityInputID InputID, FGameplayTag Tag);
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	int AbilityArrayIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = BuildingSnap)
	bool WorkAreaIsSnapped = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BuildingSnap)
	float SnapGap = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BuildingSnap)
	float SnapDistance = 100.f;
	
	// Distance threshold between mouse and snapped actor to release the snap
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BuildingSnap)
	float SnapReleaseDistance = 400.f;

	/** Maximum Z difference (base-to-base) allowed for two buildings to connect with an energy wall. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BuildingSnap)
	float EnergyWallSnapZTolerance = 10.f;

	/** Maximum ground height difference allowed for extension placement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BuildingSnap)
	float ExtensionGroundZThreshold = 10.f;
	
	// The actor we are currently snapped to (if any)
	UPROPERTY(BlueprintReadOnly, Category = BuildingSnap)
	AActor* CurrentSnapActor = nullptr;

	// Cooldown between initiating new snap targets to prevent flicker
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BuildingSnap)
	float SnapCooldownSeconds = 0.3f;

	// Internal time when next snap is allowed (not replicated)
	UPROPERTY(Transient)
	float NextAllowedSnapTime = 0.f;

	// Time the mouse must remain beyond the release threshold before we actually unsnap
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BuildingSnap)
	float UnsnapGraceSeconds = 0.15f;

	// Acquire distance is tighter than release distance to add hysteresis (0.1..1.0)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BuildingSnap, meta=(ClampMin="0.1", ClampMax="1.0"))
	float AcquireHysteresisFactor = 0.85f;

	// Internal timestamp when we first detected being beyond release distance
	UPROPERTY(Transient)
	float LastBeyondReleaseTime = -1.f;
	
	UPROPERTY(BlueprintReadWrite, Category = BuildingSnap)
	float DraggedAreaZOffset = 10.f;

	UPROPERTY(BlueprintReadWrite, Category = BuildingSnap)
	int MaxAbilityArrayIndex = 3;
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void Server_ClearAbilityIndicator();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	TArray<TSubclassOf<UGameplayAbilityBase>> GetAbilityArrayByIndex();

	TArray<TSubclassOf<UGameplayAbilityBase>> GetAbilityArrayForUnit(AGASUnit* Unit, int32 InAbilityArrayIndex = -1);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	TArray<UGameplayAbilityBase*> GetAbilityObjectArrayByIndex();
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void AddAbilityIndex(int Add);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void GetClosestUnitTo(FVector Position, int PlayerTeamId, EGASAbilityInputID InputID);
	
	UFUNCTION(Server, Reliable, Category = RTSUnitTemplate)
	void ServerGetClosestUnitTo(FVector Position, int PlayerTeamId, EGASAbilityInputID InputID);

	UFUNCTION(Client, Reliable, Category = RTSUnitTemplate)
	void ClientReceiveClosestUnit(AUnitBase* ClosestUnit, EGASAbilityInputID InputID);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void ActivateKeyboardAbilitiesOnCloseUnits(EGASAbilityInputID InputID, FVector CameraLocation, int PlayerTeamId, AHUDBase* HUD);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void ActivateKeyboardAbilitiesOnMultipleUnits(EGASAbilityInputID InputID);

	/**
	 * Builds a valid ground hit under the controlled agent pawn, standing in for the cursor hit a human
	 * would produce. An AI controller has no cursor, so GetHitResultUnderCursor returns an invalid hit and
	 * AGASUnit::ActivateAbilityByInputID never calls FireMouseHitAbility - which is where every build
	 * ability actually places its work area. Without this the rule AI can decide to build but never does.
	 * Returns false only when there is no pawn or world.
	 */
	bool GetAbilityHitResultForAI(FHitResult& OutHit) const;

	/**
	 * How many construction sites the AI may have running at once.
	 *
	 * Unit caps count FINISHED buildings, so while a dozen builds are in flight the cap never bites: the
	 * agent kept starting new ones every decision and ended up with 38 open sites, 17 pods against a cap
	 * of 6, its workers morphed away to 5 and every resource spent - so it never afforded a single combat
	 * unit. Only the AI is bounded here; a player opens sites by hand and needs no limit.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|AI", meta=(ClampMin="1"))
	int32 AIMaxConcurrentBuildSites = 3;

	/**
	 * AI placement only. Largest height difference tolerated across a candidate building's footprint.
	 * Anything steeper is a ramp, and a building wedged onto one blocks the route past it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|AI Placement", meta=(ClampMin="0.0"))
	float AIMaxGroundStep = 150.f;

	/**
	 * AI placement only. How far past its own footprint the navmesh must still reach in every direction,
	 * so the agent keeps clear of cliff edges instead of sealing off the path along them.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|AI Placement", meta=(ClampMin="0.0"))
	float AICliffClearance = 350.f;

	/**
	 * AI placement only. A deposit within this range of one of our own bases counts as already served, so an
	 * expansion looks for the next field out instead of stacking another base on the one we already mine.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|AI Placement", meta=(ClampMin="0.0"))
	float ExpansionClaimedRadius = 3500.f;

	void SetAbilityInputHeld(EGASAbilityInputID InputID, bool bIsHeld);

	// Releases every held ability input. Must be called whenever input can stop being delivered
	// (menu opens, focus loss), otherwise a stale entry blocks all deselection in
	// Client_ContinueSelectionAfterAbility.
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void ClearHeldAbilityInputs();

	UFUNCTION(Server, Reliable)
	void Server_StopContinuousAbility(EGASAbilityInputID InputID, const TArray<AUnitBase*>& Units);

	TArray<AUnitBase*> GetAndPrepareAbilityTargets(TSubclassOf<UGameplayAbilityBase> AbilityClass, int32 AbilityIndex);

	// Accumulator to throttle streaming SetWorkAreaPosition updates
	UPROPERTY(Transient)
	float WorkAreaStreamAccumulator = 0.f;


	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SetWorkAreaPosition(AWorkArea* DraggedArea, FTransform NewActorTransform);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void MoveWorkArea_Local(float DeltaSeconds);

	void UpdateExtensionWorkAreaPosition(class AWorkArea* DraggedWorkArea, class ABuildingBase* Unit, float DeltaSeconds);

	void GetSnappedExtensionTransform(class ABuildingBase* Unit, const FVector& MouseLocation, FVector& OutLocation, FRotator& OutRotation);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void Server_FinalizeWorkAreaPosition(AWorkArea* DraggedArea, FTransform NewActorTransform, AUnitBase* UnitBase);

	UFUNCTION(NetMulticast, Reliable, Category = RTSUnitTemplate)
	void Multicast_ApplyWorkAreaPosition(AWorkArea* DraggedArea, FTransform NewActorTransform, AUnitBase* UnitBase);

	// Update WorkArea location on client, used for clients with same TeamId on drop
	UFUNCTION(Client, Reliable, Category = RTSUnitTemplate)
	void Client_UpdateWorkAreaPosition(AWorkArea* DraggedArea, FTransform NewActorTransform, AUnitBase* UnitBase);

	// Helper to compute a grounded location so the mesh bottom rests on the ground
	FVector ComputeGroundedLocation(AWorkArea* DraggedArea, const FVector& DesiredLocation) const;

	// Helper to broadcast WorkArea position update to all team members
	void BroadcastWorkAreaPositionToTeam(AWorkArea* DraggedArea, const FTransform& FinalTransform, AUnitBase* UnitBase);

	// Internal helpers to simplify MoveWorkArea_Local logic (non-UFUNCTION)
	bool TraceMouseToGround(FVector& OutMouseGround, FHitResult& OutHit) const;
	bool MaintainOrReleaseCurrentSnap(AWorkArea* DraggedWorkArea, const FVector& MouseGround, bool bHit);
	bool TrySnapViaOverlap(AWorkArea* DraggedWorkArea, const FVector& MouseGround, const FHitResult& HitResult);
	bool TrySnapViaProximity(AWorkArea* DraggedWorkArea, const FVector& MouseGround);
	void MoveDraggedAreaFreely(AWorkArea* DraggedWorkArea, const FVector& MouseGround, const FHitResult& HitResult);
	bool MoveWorkArea_Local_Simplified(float DeltaSeconds);
	void PerformWorkAreaDistanceResolution(AWorkArea* DraggedWorkArea, bool bWorkAreaIsSnapped);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	AActor* CheckForSnapOverlap(AWorkArea* DraggedActor, const FVector& TestLocation);
		
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SnapToActor(AWorkArea* DraggedActor, AActor* OtherActor, UStaticMeshComponent* OtherMesh);
	

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetWorkArea(FTransform AreaTransform);
	
	// Local, client-side variant used by Tick; mirrors WorkArea distance/pushback behavior for AbilityIndicator
	void MoveAbilityIndicator_Local(float DeltaSeconds);
	void HandleAbilityIndicatorStart(TSubclassOf<AAbilityIndicator> IndicatorClass, AGASUnit* Unit);
	void HandleAbilityIndicatorEnd(AGASUnit* Unit = nullptr);

	// Client informs server about indicator overlap state so server can use it for authoritative logic
	UFUNCTION(Server, Reliable, Category = RTSUnitTemplate)
	void Server_SetIndicatorOverlap(class AAbilityIndicator* Indicator, bool bOverlapping);

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	float AbilityIndicatorBlinkTimer = 0.f;

	UFUNCTION(Server, Reliable, Category = RTSUnitTemplate)
	void SendWorkerToWork(AUnitBase* Worker);

	UFUNCTION(Server, Reliable, Category = RTSUnitTemplate)
	void SendWorkerToBase(AUnitBase* Worker);
	
	// Spawns the ConstructionUnit for an Extension WorkArea on the server
	UFUNCTION(Server, Reliable, Category = RTSUnitTemplate)
	void Server_SpawnExtensionConstructionUnit(AUnitBase* Unit, AWorkArea* WA);
	
	
	UFUNCTION(Server, Reliable, Category = RTSUnitTemplate)
	void DestroyDraggedArea(AWorkingUnitBase* Worker);
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	AUnitBase* CurrentDraggedUnitBase = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	AActor* CurrentDraggedGround;

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	AUnitSpawnPlatform* SpawnPlatform;
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void MoveDraggedUnit(float DeltaSeconds);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void DragUnitBase(AUnitBase* UnitToDrag);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void DropUnitBase();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void DestroyWorkAreaOnServer(AWorkArea* WorkArea);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void DestroyWorkArea();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CancelAbilitiesIfNoBuilding(AUnitBase* Unit);

	UFUNCTION(Server, Reliable, Category = RTSUnitTemplate)
	void SendWorkerToResource(AWorkingUnitBase* Worker, AWorkArea* WorkArea);

	UFUNCTION(Server, Reliable, Category = RTSUnitTemplate)
	void SendWorkerToWorkArea(AWorkingUnitBase* Worker, AWorkArea* WorkArea);

	UFUNCTION(Server, Reliable, Category = RTSUnitTemplate)
	void LoadUnits(const TArray<AUnitBase*>& UnitsToLoad, AUnitBase* Transporter);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool CheckClickOnTransportUnit(FHitResult Hit_Pawn);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool CheckClickOnWorkArea(FHitResult Hit_Pawn);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void StopWorkOnSelectedUnit();

	UFUNCTION(Server, Reliable, Category = RTSUnitTemplate)
	void StopWork(AWorkingUnitBase* Worker);


	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SelectUnitsWithTag(FGameplayTag Tag, int TeamId);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void Server_AssignTagToSelectedUnits(FGameplayTag Tag, const TArray<AUnitBase*>& Units, int TeamId);
	
	UFUNCTION(Client, Reliable, Category = RTSUnitTemplate)
	void Client_UpdateHUDSelection(const TArray<AUnitBase*>& NewSelection, int TeamId);
	
	UFUNCTION(Client, Reliable, Category = RTSUnitTemplate)
	void Client_DeselectSingleUnit(AUnitBase* UnitToDeselect);

	void ClearMassStateTagsLocally(FMassEntityHandle Entity, struct FMassEntityManager& EntityManager);

	void BatchSetRotateToMouseTagLocally(const TArray<AUnitBase*>& Units, bool bAdd, bool bIsContinuous = false);

	UFUNCTION(Server, Reliable, Category = RTSUnitTemplate)
	void Server_BatchSetRotateToMouseTag(const TArray<AUnitBase*>& Units, bool bAdd, bool bIsContinuous = false);

	void BatchSetRunAnimationTagLocally(const TArray<AUnitBase*>& Units, float Duration, bool bAdd);

	UFUNCTION(Server, Reliable, Category = RTSUnitTemplate)
	void Server_BatchSetRunAnimationTag(const TArray<AUnitBase*>& Units, float Duration, bool bAdd);

	static void ApplyRunAnimationTag(FMassEntityManager& EntityManager, FMassEntityHandle Entity, float Duration, UnitData::EState State);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void AddToCurrentUnitWidgetIndex(int Add);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CastEndsEvent(AUnitBase* UnitBase);
	
	// Squad selection RPCs for selecting full squad on client
	UFUNCTION(Server, Reliable, Category = RTSUnitTemplate)
	void Server_SelectUnitsFromSameSquad(AUnitBase* SelectedUnit);
	
	UFUNCTION(Client, Reliable, Category = RTSUnitTemplate)
	void Client_SelectUnitsFromSameSquad(const TArray<AUnitBase*>& Units);

	UPROPERTY(Replicated)
	FVector ReplicatedMouseLocation;

	UFUNCTION(Server, Unreliable, Category = RTSUnitTemplate)
	void Server_UpdateMouseLocation(FVector NewLocation);
	
	void UpdateMouseLocationWithThrottling(FVector NewLocation);

	UPROPERTY(Transient)
	float LastMouseRPCTime = 0.f;

	UPROPERTY(Transient)
	float LastDebugLogTime = 0.f;

	UPROPERTY(Transient)
	FVector LastSentMouseLocation = FVector::ZeroVector;

	TSet<EGASAbilityInputID> HeldAbilityInputs;
};
