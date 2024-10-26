// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Controller/PlayerController/WidgetController.h"
#include "ExtendedControllerBase.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API AExtendedControllerBase : public AWidgetController
{
	GENERATED_BODY()

public:

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;
	
	//UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	//AWorkArea* CurrentDraggedWorkArea;
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void ActivateKeyboardAbilities(AGASUnit* UnitBase, EGASAbilityInputID InputID);

	UFUNCTION(BlueprintCallable, Category = "Unit")
	void GetClosestUnitTo(FVector Position, int PlayerTeamId, EGASAbilityInputID InputID);
	
	UFUNCTION(Server, Reliable)
	void ServerGetClosestUnitTo(FVector Position, int PlayerTeamId, EGASAbilityInputID InputID);

	UFUNCTION(Client, Reliable)
	void ClientReceiveClosestUnit(AUnitBase* ClosestUnit, EGASAbilityInputID InputID);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void ActivateKeyboardAbilitiesOnCloseUnits(EGASAbilityInputID InputID, FVector CameraLocation, int PlayerTeamId, AHUDBase* HUD);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void ActivateKeyboardAbilitiesOnMultipleUnits(EGASAbilityInputID InputID);
	/*
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate) // Server, Reliable, 
	void SpawnWorkArea(TSubclassOf<AWorkArea> WorkAreaClass, AWaypoint* Waypoint);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	AWorkArea* GetDraggedWorkArea();
	*/
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SetWorkAreaPosition(AWorkArea* DraggedArea, FVector NewActorPosition);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void MoveWorkArea(float DeltaSeconds);

	UFUNCTION(Server, Reliable)
	void SendWorkerToWork(AUnitBase* Worker, AWorkArea* DraggedArea);
	
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

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void DestoryWorkArea();
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void LeftClickPressed();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void LeftClickReleased();
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "RightClickPressed", Keywords = "RTSUnitTemplate RightClickPressed"), Category = RTSUnitTemplate)
	void RightClickPressed();

	UFUNCTION(Server, Reliable)
	void SendWorkerToResource(AWorkingUnitBase* Worker, AWorkArea* WorkArea);

	UFUNCTION(Server, Reliable)
	void SendWorkerToWorkArea(AWorkingUnitBase* Worker, AWorkArea* WorkArea);
	
		
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool CheckClickOnWorkArea(FHitResult Hit_Pawn);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void StopWorkOnSelectedUnit();

	UFUNCTION(Server, Reliable)
	void StopWork(AWorkingUnitBase* Worker);
};
