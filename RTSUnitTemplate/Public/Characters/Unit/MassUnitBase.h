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
	
	// The Mass Actor Binding Component
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mass)
	UMassActorBindingComponent* MassActorBindingComponent;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Components")
	UInstancedStaticMeshComponent* ISMComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ISM")
	int32 InstanceIndex = INDEX_NONE;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Unit Mode")
	bool bUseSkeletalMovement = true;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsFlying = false;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float FlyHeight = 500.f;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USelectionDecalComponent* SelectionIcon;

	//UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	//UAreaDecalComponent* AreaDecalComponent;
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool AddGamePlayEffectFragmentToEntity();
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool AddEffectTargetFragmentToEntity();
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool RemoveTagFromEntity(UScriptStruct* TagToRemove);
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool SwitchEntityTag(UScriptStruct* TagToAdd);

	UFUNCTION(BlueprintCallable, Category = Mass)
	bool SwitchEntityTagByState(TEnumAsByte<UnitData::EState> UState, TEnumAsByte<UnitData::EState> UStatePlaceholder);

	UFUNCTION(BlueprintCallable, Category = Mass)
	bool FocusEntityTarget(AUnitBase* TargetUnit);

	UFUNCTION(BlueprintCallable, Category = Mass)
	bool RemoveFocusEntityTarget();
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool UpdateEntityHealth(float NewHealth);
	
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool SyncTranslation();

	UFUNCTION(BlueprintCallable, Category = Mass)
	bool SetTranslationLocation(FVector NewLocation);
	
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
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	
	void InitializeUnitMode();

};
