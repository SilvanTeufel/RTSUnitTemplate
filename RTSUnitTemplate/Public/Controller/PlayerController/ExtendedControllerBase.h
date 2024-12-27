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

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	AUnitBase* CameraUnitWithTag;
	//UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	//AWorkArea* CurrentDraggedWorkArea;
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void ActivateAbilitiesByIndex(AGASUnit* UnitBase, EGASAbilityInputID InputID, const FHitResult& HitResult = FHitResult());

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void ActivateDefaultAbilities(AGASUnit* UnitBase, EGASAbilityInputID InputID, const FHitResult& HitResult = FHitResult());

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void ActivateSecondAbilities(AGASUnit* UnitBase, EGASAbilityInputID InputID);


	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void ActivateThirdAbilities(AGASUnit* UnitBase, EGASAbilityInputID InputID);


	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void ActivateFourthAbilities(AGASUnit* UnitBase, EGASAbilityInputID InputID);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void ActivateAbilities(AGASUnit* UnitBase, EGASAbilityInputID InputID, const TArray<TSubclassOf<UGameplayAbilityBase>>& Abilities);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void ActivateDefaultAbilitiesByTag(EGASAbilityInputID InputID, FGameplayTag Tag);
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	int AbilityArrayIndex = 0;

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
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

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SetWorkAreaPosition(AWorkArea* DraggedArea, FVector NewActorPosition);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SnapToActor(AActor* DraggedActor, AActor* OtherActor);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void MoveWorkArea(float DeltaSeconds);

	UFUNCTION(Server, Reliable)
	void SendWorkerToWork(AUnitBase* Worker);
	
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
	void DestoryWorkAreaOnServer(AWorkArea* WorkArea);
	
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
