// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"


#include "MassEntitySubsystem.h"
#include "MassExecutor.h"
#include "MassCommandBuffer.h"
#include "MassNavigationFragments.h"
#include "MassMovementFragments.h"
#include "Actors/SelectionCircleActor.h"
#include "Engine/World.h"        // Include for UWorld, GEngine
#include "Engine/Engine.h"       // Include for GEngine


#include "CustomControllerBase.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API ACustomControllerBase : public AExtendedControllerBase
{
	GENERATED_BODY()
protected:
	// /** If true, the formation will be recalculated on the next move command, even if the selection hasn't changed. */
	// UPROPERTY(BlueprintReadWrite, Category = "RTS")
	bool bForceFormationRecalculation = true;
	// 
	// /** Stores the calculated offset for each unit from the formation's center point. This preserves the formation shape. */
	TMap<AUnitBase*, FVector> UnitFormationOffsets;
	// 
	// /** A snapshot of the last group of units for which a formation was calculated. Used to detect changes in selection. */
	TArray<TWeakObjectPtr<AUnitBase>> LastFormationUnits;

	
public:
	UFUNCTION(NetMulticast, Reliable)
	void Multi_SetMyTeamUnits(const TArray<AActor*>& AllUnits);
	
	UFUNCTION(NetMulticast, Reliable)
	void Multi_SetCamLocation(FVector NewLocation);

	UFUNCTION(NetMulticast, Reliable)
	void Multi_HideEnemyWaypoints();

	UFUNCTION(NetMulticast, Reliable)
	void Multi_InitFogOfWar();

	
	UFUNCTION(Client, Reliable)
	void AgentInit();
	
	UFUNCTION(Server, Reliable, BlueprintCallable,  Category = RTSUnitTemplate)
	void CorrectSetUnitMoveTarget(
		UObject* WorldContextObject,
		AUnitBase* Unit,
		const FVector& NewTargetLocation,
		float DesiredSpeed = 300.0f,
		float AcceptanceRadius = 50.0f,
		bool AttackT = false);


	UFUNCTION(Server, Reliable, BlueprintCallable,  Category = RTSUnitTemplate)
	void CorrectSetUnitMoveTargetForAbility(
		UObject* WorldContextObject,
		AUnitBase* Unit,
		const FVector& NewTargetLocation,
		float DesiredSpeed = 300.0f,
		float AcceptanceRadius = 50.0f,
		bool AttackT = false);

	UFUNCTION(Server, Reliable)
	void LoadUnitsMass(const TArray<AUnitBase*>& UnitsToLoad, AUnitBase* Transporter);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool CheckClickOnTransportUnitMass(FHitResult Hit_Pawn);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void RightClickPressedMass();


	/** Returns the world location of a unit, handling Actor vs. ISM-instance. */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	FVector GetUnitWorldLocation(const AUnitBase* Unit) const;
	
	/** Computes offsets for an N-unit grid formation centered at (0,0). */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	TArray<FVector> ComputeSlotOffsets(int32 NumUnits) const;
	/** Builds an N×N cost matrix of squared distances from unitsto slots. */
	TArray<TArray<float>> BuildCostMatrix(
		const TArray<FVector>& UnitPositions,
		const TArray<FVector>& SlotOffsets,
		const FVector& TargetCenter) const;
	
	/** Solves the assignment problem (Hungarian) on the given cost matrix. */
	TArray<int32> SolveHungarian(const TArray<TArray<float>>& Matrix) const;
	/** Determines if the formation needs to be recalculated. */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool ShouldRecalculateFormation() const;
	
	/** Recalculates and stores unit formation offsets around TargetCenter. */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void RecalculateFormation(const FVector& TargetCenter);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void RunUnitsAndSetWaypointsMass(FHitResult Hit);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void LeftClickPressedMass();

	UFUNCTION(Server, Reliable, Blueprintable,  Category = RTSUnitTemplate)
	void LeftClickAttackMass(AUnitBase* Unit, FVector Location, bool AttackT);

	UFUNCTION(Server, Reliable, Blueprintable,  Category = RTSUnitTemplate)
	void LeftClickAMoveUEPFMass(AUnitBase* Unit, FVector Location, bool AttackT);


	UFUNCTION(Server, Reliable,  Category = RTSUnitTemplate)
	void Server_ReportUnitVisibility(APerformanceUnit* Unit, bool bVisible);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void LeftClickReleasedMass();
	// Updates the Fog Mask using visible units
	UFUNCTION()
	void UpdateFogMaskWithCircles(const TArray<FMassEntityHandle>& Entities);

	UFUNCTION()
	void UpdateMinimap(const TArray<FMassEntityHandle>& Entities);
	
	UPROPERTY()
	ASelectionCircleActor* SelectionCircleActor;
	
	UFUNCTION()
	void UpdateSelectionCircles();

	UFUNCTION(NetMulticast, Reliable)
	void Multi_SetupPlayerMiniMap();

	UFUNCTION(Client, Reliable)
	void Client_ReceiveCooldown(int32 AbilityIndex, float RemainingTime);

	UFUNCTION(Server, Reliable)
	void Server_RequestCooldown(AUnitBase* Unit, int32 AbilityIndex, UGameplayAbilityBase* Ability);
};
