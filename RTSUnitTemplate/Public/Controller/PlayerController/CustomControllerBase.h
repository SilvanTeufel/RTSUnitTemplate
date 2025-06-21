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
public:
	UFUNCTION(NetMulticast, Reliable)
	void Multi_SetMyTeamUnits(const TArray<AActor*>& AllUnits);

	UFUNCTION(NetMulticast, Reliable)
	void Multi_ShowWidgetsWhenLocallyControlled();

	UFUNCTION(NetMulticast, Reliable)
	void Multi_SetCamLocation(FVector NewLocation);

	UFUNCTION(NetMulticast, Reliable)
	void Multi_HideEnemyWaypoints();

	UFUNCTION(NetMulticast, Reliable)
	void Multi_InitFogOfWar();

	
	UFUNCTION(Client, Reliable)
	void AgentInit();
	
	UFUNCTION(Server, Reliable, Blueprintable,  Category = RTSUnitTemplate)
	void CorrectSetUnitMoveTarget(
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
};
