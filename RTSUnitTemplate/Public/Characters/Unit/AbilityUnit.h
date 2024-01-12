// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Core/UnitData.h"
#include "PathSeekerBase.h"
#include "AbilityUnit.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API AAbilityUnit : public APathSeekerBase
{
	GENERATED_BODY()

private:
	FTimerHandle AccelerationTimerHandle;
	FVector TargetVelocity;
	FVector CurrentVelocity;
	float AccelerationRate;
	FVector ChargeDestination;
	float RequiredDistanceToStart;
	
	void Accelerate();


public:	

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void TeleportToValidLocation(const FVector& Destination);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void StartAcceleratingTowardsDestination(const FVector& NewDestination, const FVector& NewTargetVelocity, float NewAccelerationRate, float NewRequiredDistanceToStart);

	// Set Unit States  //////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetUnitState(TEnumAsByte<UnitData::EState> NewUnitState);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	TEnumAsByte<UnitData::EState> GetUnitState();

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "UnitState", Keywords = "RTSUnitTemplate UnitState"), Category = RTSUnitTemplate)
	TEnumAsByte<UnitData::EState> UnitState = UnitData::Idle;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "UnitStatePlaceholder", Keywords = "RTSUnitTemplate UnitStatePlaceholder"), Category = RTSUnitTemplate)
	TEnumAsByte<UnitData::EState> UnitStatePlaceholder = UnitData::Patrol;
	///////////////////////////////////////////////////////////////////

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=RTSUnitTemplate)
	TArray<TSubclassOf<class UGameplayAbilityBase>>OffensiveAbilities;
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=RTSUnitTemplate)
	TArray<TSubclassOf<class UGameplayAbilityBase>>DefensiveAbilities;
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=RTSUnitTemplate)
	TArray<TSubclassOf<class UGameplayAbilityBase>>AttackAbilities;
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=RTSUnitTemplate)
	TArray<TSubclassOf<class UGameplayAbilityBase>>ThrowAbilities;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	EGASAbilityInputID OffensiveAbilityID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	EGASAbilityInputID DefensiveAbilityID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	EGASAbilityInputID AttackAbilityID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	EGASAbilityInputID ThrowAbilityID;
};
