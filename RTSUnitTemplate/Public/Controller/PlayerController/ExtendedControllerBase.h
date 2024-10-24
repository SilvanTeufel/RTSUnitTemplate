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
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	AWorkArea* CurrentDraggedWorkArea;

	UFUNCTION(Server, Reliable,BlueprintCallable, Category = RTSUnitTemplate)
	void ActivateKeyboardAbilities(AGASUnit* UnitBase, EGASAbilityInputID InputID);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SpawnWorkArea(TSubclassOf<AWorkArea> WorkAreaClass, AWaypoint* Waypoint);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void MoveWorkArea(float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void DropWorkArea();
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	AUnitBase* CurrentDraggedUnitBase;

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
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool CheckClickOnWorkArea(FHitResult Hit_Pawn);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void StopWork();
};
