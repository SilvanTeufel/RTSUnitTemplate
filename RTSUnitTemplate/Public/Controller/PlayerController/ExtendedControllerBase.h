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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagF1 = FGameplayTag::RequestGameplayTag(FName("Key.F1"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagF2 = FGameplayTag::RequestGameplayTag(FName("Key.F2"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagF3 = FGameplayTag::RequestGameplayTag(FName("Key.F3"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagF4 = FGameplayTag::RequestGameplayTag(FName("Key.F4"));
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagCtrl1 = FGameplayTag::RequestGameplayTag(FName("Key.Ctrl1"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagCtrl2 = FGameplayTag::RequestGameplayTag(FName("Key.Ctrl2"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagCtrl3 = FGameplayTag::RequestGameplayTag(FName("Key.Ctrl3"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagCtrl4 = FGameplayTag::RequestGameplayTag(FName("Key.Ctrl4"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagCtrl5 = FGameplayTag::RequestGameplayTag(FName("Key.Ctrl5"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagCtrl6 = FGameplayTag::RequestGameplayTag(FName("Key.Ctrl6"));
	

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagCtrlQ = FGameplayTag::RequestGameplayTag(FName("Key.CtrlQ"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagCtrlW = FGameplayTag::RequestGameplayTag(FName("Key.CtrlW"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagCtrlE = FGameplayTag::RequestGameplayTag(FName("Key.CtrlE"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagCtrlR = FGameplayTag::RequestGameplayTag(FName("Key.CtrlR"));


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagAlt1 = FGameplayTag::RequestGameplayTag(FName("Key.Alt1"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagAlt2 = FGameplayTag::RequestGameplayTag(FName("Key.Alt2"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagAlt3 = FGameplayTag::RequestGameplayTag(FName("Key.Alt3"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagAlt4 = FGameplayTag::RequestGameplayTag(FName("Key.Alt4"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagAlt5 = FGameplayTag::RequestGameplayTag(FName("Key.Alt5"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag KeyTagAlt6 = FGameplayTag::RequestGameplayTag(FName("Key.Alt6"));


	
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
	
	UPROPERTY(BlueprintReadWrite, Category = BuildingSnap)
	float SnapGap = 50.f;

	UPROPERTY(BlueprintReadWrite, Category = BuildingSnap)
	float SnapDistance = 100.f;
	
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

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SetWorkAreaPosition(AWorkArea* DraggedArea, FVector NewActorPosition);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	AActor* CheckForSnapOverlap(AWorkArea* DraggedActor, const FVector& TestLocation);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SnapToActor(AWorkArea* DraggedActor, AActor* OtherActor);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SnapToActorVisited(AWorkArea* DraggedActor, AActor* OtherActor, TSet<AActor*>& AlreadyVisited);
	
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
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
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


	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SelectUnitsWithTag(FGameplayTag Tag, int TeamId);

	UFUNCTION(Client, Reliable)
	void Client_UpdateHUDSelection(const TArray<AUnitBase*>& NewSelection, int TeamId);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void AddToCurrentUnitWidgetIndex(int Add);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void CastEndsEvent(AUnitBase* UnitBase);
	
};
