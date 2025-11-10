// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilityUnit.h"
#include "NiagaraComponent.h"
#include "Actors/AreaDecalComponent.h"
#include "Actors/SelectionDecalComponent.h"
#include "MassUnitBase.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API AMassUnitBase : public AAbilityUnit
{
	GENERATED_BODY()

public:
	
	AMassUnitBase(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = ISM)
	virtual FVector GetMassActorLocation() const override;
	// The Mass Actor Binding Component
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ISM)
	UMassActorBindingComponent* MassActorBindingComponent;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = ISM)
	UInstancedStaticMeshComponent* ISMComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category= ISM)
	int32 InstanceIndex = INDEX_NONE;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = ISM)
	bool bUseSkeletalMovement = true;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = ISM)
	bool bUseIsmWithActorMovement = true;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsFlying = false;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float FlyHeight = 500.f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool AddEffectTargetFragement = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool AddGameplayEffectFragement = false;

	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USelectionDecalComponent* SelectionIcon;

	//UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	//UAreaDecalComponent* AreaDecalComponent;

	UFUNCTION(BlueprintCallable, Category = Mass)
	bool SetInvisibility(bool NewInvisibility);
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool AddStopMovementTagToEntity();

	UFUNCTION(BlueprintCallable, Category = Mass)
	bool EnableDynamicObstacle(bool Enable);

	UFUNCTION(BlueprintCallable, Category = Mass)
	bool SetUnitAvoidanceEnabled(bool bEnable);
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool RemoveStopGameplayEffectTagToEntity();
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool AddStopGameplayEffectTagToEntity();
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool RemoveTagFromEntity(UScriptStruct* TagToRemove);
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool SwitchEntityTag(UScriptStruct* TagToAdd);

	UFUNCTION(BlueprintCallable, Category = Mass)
	bool SwitchEntityTagByState(TEnumAsByte<UnitData::EState> UState, TEnumAsByte<UnitData::EState> UStatePlaceholder);

	UFUNCTION(BlueprintCallable, Category = Mass)
	bool UpdateEntityStateOnUnload(const FVector& UnloadLocation);

	UFUNCTION(BlueprintCallable, Category = Mass)
	bool ResetTarget();
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool EditUnitDetection(bool HasDetection);
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool EditUnitDetectable(bool IsDetectable);
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool FocusEntityTarget(AUnitBase* TargetUnit);

	UFUNCTION(BlueprintCallable, Category = Mass)
	bool IsUnitDetectable();
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool RemoveFocusEntityTarget();
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool UpdateEntityHealth(float NewHealth);
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool SyncTranslation();

	UFUNCTION(BlueprintCallable, Category = Mass)
	bool SetTranslationLocation(FVector NewLocation);
	
	// Updates MoveTarget and ClientPrediction fragments to reflect a new location and desired speed
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool UpdatePredictionFragment(const FVector& NewLocation, float DesiredSpeed = 0.f);
	
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_UpdateISMInstanceTransform(int32 InstIndex, const FTransform& NewTransform);

	UFUNCTION(BlueprintCallable, Category = Mass)
	void StartAcceleratingTowardsDestination(const FVector& NewDestination, float NewAccelerationRate, float NewRequiredDistanceToStart);

	UFUNCTION(BlueprintCallable, Category = Mass)
	void StartCharge(const FVector& NewDestination, float ChargeSpeed, float ChargeDuration);
	
	bool GetMassEntityData(FMassEntityManager*& OutEntityManager, FMassEntityHandle& OutEntityHandle);

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UNiagaraComponent* Niagara_A;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	FTransform Niagara_A_Start_Transform;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UNiagaraComponent* Niagara_B;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	FTransform Niagara_B_Start_Transform;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	FQuat MeshRotationOffset;
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	void InitializeUnitMode();
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	
	//void InitializeUnitMode();

};
