// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

class AWorkArea;
class AActor;
class UStaticMeshComponent;
class USoundBase;
class AUnitBase;
class AAbilityIndicator;

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Controller/PlayerController/WidgetController.h"
#include "ExtendedControllerBase.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API AExtendedControllerBase : public AWidgetController
{
	GENERATED_BODY()
private:
	/** Handle for the timer that logs entity tags after BeginPlay. */
	FTimerHandle LogTagsTimerHandle;
	
public:
	// Set to true when a keyboard ability was just executed; consumed on next left click
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	bool bUsedKeyboardAbilityBeforeClick = false;

	virtual void BeginPlay() override;
	
	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool LogSelectedEntity = false;
	
	void LogSelectedUnitsTags();
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

	UFUNCTION(Client, Reliable)
	void Client_ApplyCustomizations(
		USoundBase* InWaypointSound,
		USoundBase* InRunSound,
		USoundBase* InAbilitySound,
		USoundBase* InAttackSound,
		USoundBase* InDropWorkAreaFailedSound,
		USoundBase* InDropWorkAreaSound);

	// Play a 2D sound only for this owning client (call from server or client)
	UFUNCTION(Client, Reliable)
	void Client_PlaySound2D(USoundBase* Sound, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f);

	void SetAbilityEnabledByKey(AUnitBase* UnitBase, const FString& Key, bool bEnable);
	// New variant that operates on a specific unit (no UFUNCTION to avoid UHT overloading conflicts)
	bool DropWorkAreaForUnit(class AUnitBase* UnitBase, bool bWorkAreaIsSnapped, USoundBase* InDropWorkAreaFailedSound);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagF1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagF2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagF3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagF4;
	
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
	void ActivateAbilitiesByIndex(AGASUnit* UnitBase, EGASAbilityInputID InputID, const FHitResult& HitResult = FHitResult());

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
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	TArray<TSubclassOf<UGameplayAbilityBase>> GetAbilityArrayByIndex();
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void AddAbilityIndex(int Add);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void ApplyMovementInputToUnit(const FVector& Direction, float Scale, AUnitBase* Unit, int TeamId);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void GetClosestUnitTo(FVector Position, int PlayerTeamId, EGASAbilityInputID InputID);
	
	UFUNCTION(Server, Reliable)
	void ServerGetClosestUnitTo(FVector Position, int PlayerTeamId, EGASAbilityInputID InputID);

	UFUNCTION(Client, Reliable)
	void ClientReceiveClosestUnit(AUnitBase* ClosestUnit, EGASAbilityInputID InputID);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void ActivateKeyboardAbilitiesOnCloseUnits(EGASAbilityInputID InputID, FVector CameraLocation, int PlayerTeamId, AHUDBase* HUD);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void ActivateKeyboardAbilitiesOnMultipleUnits(EGASAbilityInputID InputID);

	// Accumulator to throttle streaming SetWorkAreaPosition updates
	UPROPERTY(Transient)
	float WorkAreaStreamAccumulator = 0.f;

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SetWorkAreaPosition(AWorkArea* DraggedArea, FVector NewActorPosition);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void MoveWorkArea_Local(float DeltaSeconds);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void Server_FinalizeWorkAreaPosition(AWorkArea* DraggedArea, FVector NewActorPosition);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ApplyWorkAreaPosition(AWorkArea* DraggedArea, FVector NewActorPosition);

	// Update WorkArea location on client, used for clients with same TeamId on drop
	UFUNCTION(Client, Reliable)
	void Client_UpdateWorkAreaPosition(AWorkArea* DraggedArea, FVector NewActorPosition);

	// Helper to compute a grounded location so the mesh bottom rests on the ground
	FVector ComputeGroundedLocation(AWorkArea* DraggedArea, const FVector& DesiredLocation) const;

	// Internal helpers to simplify MoveWorkArea_Local logic (non-UFUNCTION)
	bool TraceMouseToGround(FVector& OutMouseGround, FHitResult& OutHit) const;
	bool MaintainOrReleaseCurrentSnap(AWorkArea* DraggedWorkArea, const FVector& MouseGround, bool bHit);
	bool TrySnapViaOverlap(AWorkArea* DraggedWorkArea, const FVector& MouseGround, const FHitResult& HitResult);
	bool TrySnapViaProximity(AWorkArea* DraggedWorkArea, const FVector& MouseGround);
	void MoveDraggedAreaFreely(AWorkArea* DraggedWorkArea, const FVector& MouseGround, const FHitResult& HitResult);
	bool MoveWorkArea_Local_Simplified(float DeltaSeconds);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	AActor* CheckForSnapOverlap(AWorkArea* DraggedActor, const FVector& TestLocation);
		
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SnapToActor(AWorkArea* DraggedActor, AActor* OtherActor, UStaticMeshComponent* OtherMesh);
	

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetWorkArea(FVector AreaLocation);
	
	// Local, client-side variant used by Tick; mirrors WorkArea distance/pushback behavior for AbilityIndicator
	void MoveAbilityIndicator_Local(float DeltaSeconds);

	// Client informs server about indicator overlap state so server can use it for authoritative logic
	UFUNCTION(Server, Reliable)
	void Server_SetIndicatorOverlap(class AAbilityIndicator* Indicator, bool bOverlapping);

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	float AbilityIndicatorBlinkTimer = 0.f;

	UFUNCTION(Server, Reliable)
	void SendWorkerToWork(AUnitBase* Worker);

	UFUNCTION(Server, Reliable)
	void SendWorkerToBase(AUnitBase* Worker);
	
	// Spawns the ConstructionUnit for an Extension WorkArea on the server
	UFUNCTION(Server, Reliable)
	void Server_SpawnExtensionConstructionUnit(AUnitBase* Unit, AWorkArea* WA);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool DropWorkArea();
	
	UFUNCTION(Server, Reliable)
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

	UFUNCTION(Server, Reliable)
	void SendWorkerToResource(AWorkingUnitBase* Worker, AWorkArea* WorkArea);

	UFUNCTION(Server, Reliable)
	void SendWorkerToWorkArea(AWorkingUnitBase* Worker, AWorkArea* WorkArea);

	UFUNCTION(Server, Reliable)
	void LoadUnits(const TArray<AUnitBase*>& UnitsToLoad, AUnitBase* Transporter);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool CheckClickOnTransportUnit(FHitResult Hit_Pawn);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool CheckClickOnWorkArea(FHitResult Hit_Pawn);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void StopWorkOnSelectedUnit();

	UFUNCTION(Server, Reliable)
	void StopWork(AWorkingUnitBase* Worker);


	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SelectUnitsWithTag(FGameplayTag Tag, int TeamId);
	
	UFUNCTION(Client, Reliable)
	void Client_UpdateHUDSelection(const TArray<AUnitBase*>& NewSelection, int TeamId);
	
	UFUNCTION(Client, Reliable)
	void Client_DeselectSingleUnit(AUnitBase* UnitToDeselect);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void AddToCurrentUnitWidgetIndex(int Add);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CastEndsEvent(AUnitBase* UnitBase);
	
	// Squad selection RPCs for selecting full squad on client
	UFUNCTION(Server, Reliable)
	void Server_SelectUnitsFromSameSquad(AUnitBase* SelectedUnit);
	
	UFUNCTION(Client, Reliable)
	void Client_SelectUnitsFromSameSquad(const TArray<AUnitBase*>& Units);
	
};
