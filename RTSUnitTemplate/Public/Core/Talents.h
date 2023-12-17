// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Talents.generated.h"

UENUM()
namespace UltimateData
{
	enum EState
	{
		Whirlwind   UMETA(DisplayName = "Whirlwind"),
		EffectArea  UMETA(DisplayName = "EffectArea"),
		EffectChain UMETA(DisplayName = "EffectChain"),
		BleedAttack UMETA(DisplayName = "BleedAttack"),
		Charge UMETA(DisplayName = "Charge"),
		None UMETA(DisplayName = "None"),
	};

}

USTRUCT(BlueprintType)
struct FClassTalents : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	TArray<int> ClassIds;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float RangeOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	int WalkSpeedOffset;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float AttackSpeedScaler;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	int PiercedTargetsOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float ProjectileScaler;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float ProjectileSpeedScaler;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	int ProjectileCount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float HealthReg;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float ShieldReg;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TEnumAsByte<UltimateData::EState> Ultimate;
};

USTRUCT()
struct FClassTalentPoints
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	int ClassId;
	
	UPROPERTY()
	int RangeOffset;

	UPROPERTY()
	int WalkSpeedOffset;
	
	UPROPERTY()
	int AttackSpeedScaler;
	
	UPROPERTY()
	int PiercedTargetsOffset;

	UPROPERTY()
	int ProjectileScaler;

	UPROPERTY()
	int ProjectileSpeedScaler;

	UPROPERTY()
	int ProjectileCount;

	UPROPERTY()
	int HealthReg;

	UPROPERTY()
	int ShieldReg;

	UPROPERTY()
	int Ultimate;
};

USTRUCT()
struct FClassTalentData
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY()
	FClassTalents Talents;
	
	UPROPERTY()
	FClassTalentPoints TalentPoints;
};