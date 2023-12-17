// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/Unit/HealingUnit.h"
#include "Core/Talents.h"
#include "Core/TalentSaveGame.h"
#include "TalentedUnit.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API ATalentedUnit : public AHealingUnit
{
	GENERATED_BODY()
private:
	virtual void BeginPlay() override;
	
public:
	ATalentedUnit(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	int ClassId;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	class UDataTable* TalentTable;
	
	//TArray<FClassTalents> TalentsArray;
	
	TArray<FClassTalentData> TalentDataArray;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	bool DisableSaveGame = false;
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateWeponTalentData", Keywords = "RTSUnitTemplate CreateWeponTalentData"), Category = RTSUnitTemplate)
	void CreateTalentData();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "IncrementTalentPoint", Keywords = "RTSUnitTemplate IncrementTalentPoint"), Category = RTSUnitTemplate)
	void IncrementTalentPoint(const FString& TalentKey);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetTalentPoints();
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void ResetTalentPoint();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	int MaxTalentPoints = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	int MaxAvailableTalentPoints = 25;
	
	TArray<int> AvailableTalentPoints;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void AddTalentPoint(int Amount);
	
	UFUNCTION(Category = RTSUnitTemplate)
	void SaveTalentPoints(const TArray<FClassTalentData>& DataArray);

	UFUNCTION(Category = RTSUnitTemplate)
	TArray<FClassTalentData> LoadTalentPoints();
	
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	//class UWidgetComponent* TimerWidgetComp;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	//FVector TimerWidgetCompLocation = FVector (0.f, 0.f, -180.f);

	//UFUNCTION(Category = RTSUnitTemplate)
	//void SetupTimerWidget();

	// Changed Parameters for Talents

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float WalkSpeedOffset = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float PauseDuration = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float PiercedTargetsOffset = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float ProjectileCount = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float HealthReg = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float ShieldReg = 0.f;

	TEnumAsByte<UltimateData::EState> Ultimate = UltimateData::None;
};
