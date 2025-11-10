// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Core/UnitData.h"
#include "LevelUnit.h"
#include "TimerManager.h"
#include "AbilityUnit.generated.h"


UCLASS()
class RTSUNITTEMPLATE_API AAbilityUnit : public ALevelUnit
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
	void AccelerateFrom();

public:	
	
	virtual void Tick(float DeltaTime) override;
	
	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

	virtual void PossessedBy(AController* NewController) override;

	virtual void LevelUp_Implementation() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Worker)
	bool IsWorker = false;
	
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = Ability)
	void TeleportToValidLocation(const FVector& Destination, float MaxZDifference = 1000.f, float ZOffset = 70.f);
	
	UFUNCTION(BlueprintCallable, Category = Ability)
	void StartAcceleratingFromDestination(const FVector& NewDestination, const FVector& NewTargetVelocity, float NewAccelerationRate, float NewRequiredDistanceToStart);
	
	// Set Unit States  //////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category = Ability)
	void SetUnitState(TEnumAsByte<UnitData::EState> NewUnitState);

	//FTimerHandle CollisionTimerHandle;
	
	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void IsDead();

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void StartedMoving();

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void StoppedMoving();

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void GotAttacked();

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void GoToResource();

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void GoToBuild();

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void GoToBase();
	
	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void WorkerGoToOther();
	
	UFUNCTION(BlueprintCallable, Category = Ability)
	TEnumAsByte<UnitData::EState> GetUnitState() const;

	UFUNCTION(BlueprintCallable, Category=Ability)
	virtual void GetAbilitiesArrays();

	UFUNCTION(BlueprintCallable, Category=Ability)
	virtual void GetSelectedAbilitiesArray(TSubclassOf<UGameplayAbilityBase>& SAbility);
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = RTSUnitTemplate)
	TEnumAsByte<UnitData::EState> UnitState = UnitData::Idle;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TEnumAsByte<UnitData::EState> UnitStatePlaceholder = UnitData::Patrol;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TEnumAsByte<UnitData::EState> StoredUnitState = UnitData::Idle;
	///////////////////////////////////////////////////////////////////

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=Ability)
	TArray<TSubclassOf<class UGameplayAbilityBase>>SelectableAbilities;
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=Ability)
	TArray<TSubclassOf<class UGameplayAbilityBase>>OffensiveAbilities;
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=Ability)
	TArray<TSubclassOf<class UGameplayAbilityBase>>DefensiveAbilities;
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=Ability)
	TArray<TSubclassOf<class UGameplayAbilityBase>>AttackAbilities;
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category=Ability)
	TArray<TSubclassOf<class UGameplayAbilityBase>>ThrowAbilities;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	EGASAbilityInputID OffensiveAbilityID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	EGASAbilityInputID DefensiveAbilityID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	EGASAbilityInputID AttackAbilityID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	EGASAbilityInputID ThrowAbilityID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	EGASAbilityInputID DefaultAbilityID;
	
	UPROPERTY(EditAnywhere, Category = Ability)
	int32 AutoAbilitySequence[4] = {0, 1, 2, 3};

	UFUNCTION(BlueprintCallable, Category = Ability)
	void SetAutoAbilitySequence(int Index, int32 Value);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	bool AutoApplyAbility = true;

	UFUNCTION(BlueprintCallable, Category = Ability)
	bool IsAbilityAllowed(EGASAbilityInputID AbilityID, int Ability);
	
	UFUNCTION(BlueprintCallable, Category = Ability)
	void SpendAbilityPoints( EGASAbilityInputID AbilityID, int AbilityIndex);

	UFUNCTION(BlueprintCallable, Category = Ability)
	int32 DetermineAbilityID(int32 Level);

	UFUNCTION(BlueprintCallable, Category = Ability)
	void AutoAbility();

	UFUNCTION(BlueprintCallable, Category = Ability)
	void AddAbilityPoint();

	UFUNCTION(BlueprintCallable, Category = Ability)
	void SaveAbilityAndLevelData(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = Ability)
	void LoadAbilityAndLevelData(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = Ability)
	void ResetAbility();

	UFUNCTION(BlueprintCallable, Category = Ability)
	void RollRandomAbilitys();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	int AbilityResetPenalty = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	int AbilityCostIncreaser = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	int MaxAbilityPointsToInvest = 5;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	//TArray<FUnitSpawnData> SummonedUnitsDataSet;

//	UFUNCTION(BlueprintCallable, Category = Ability)
//	void SpawnUnitsFromParameters(FUnitSpawnParameter SpawnParameter, FVector Location, AUnitBase* UnitToChase, int NewTeamId, AWaypoint* Waypoint, int UIndex);


};
