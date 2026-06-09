// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "Core/UnitData.h"
#include "Engine/DataTable.h"
#include "UnitAnimationProcessor.generated.h"

USTRUCT(BlueprintType)
struct FISMAnimationData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
    TEnumAsByte<UnitData::EState> AnimState = UnitData::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
    float StateCustomDataValue = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
    float TransitionRate = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
    float StartFrame = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
    float EndFrame = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
    float PlayRate = 1.0f;
};

USTRUCT()
struct RTSUNITTEMPLATE_API FUnitAnimationFragment : public FMassFragment
{
    GENERATED_BODY()

    UPROPERTY(Transient)
    float TargetBlendPoint_1 = 0.f;
    
    UPROPERTY(Transient)
    float TargetBlendPoint_2 = 0.f;
    
    UPROPERTY(Transient)
    float CurrentBlendPoint_1 = 0.f;
    
    UPROPERTY(Transient)
    float CurrentBlendPoint_2 = 0.f;
    
    UPROPERTY(Transient)
    float TransitionRate_1 = 0.5f;
    
    UPROPERTY(Transient)
    float TransitionRate_2 = 0.5f;
    
    UPROPERTY(Transient)
    float Resolution_1 = 0.f;
    
    UPROPERTY(Transient)
    float Resolution_2 = 0.f;

    UPROPERTY(Transient)
    class USoundBase* Sound = nullptr;

    UPROPERTY(Transient)
    TEnumAsByte<UnitData::EState> LastProcessedState = UnitData::None;

	UPROPERTY(Transient)
	float PlayRate = 1.0f;

	UPROPERTY(Transient)
	float AnimationPosition = 0.0f;

    UPROPERTY(Transient)
    float TargetStateCustomDataValue = 0.0f;

    UPROPERTY(Transient)
    float PrevTargetStateCustomDataValue = 0.0f;

    UPROPERTY(Transient)
    float PrevStartTime = 0.0f;

    UPROPERTY(Transient)
    float PrevStartFrame = 0.0f;

    UPROPERTY(Transient)
    float PrevEndFrame = 0.0f;

    UPROPERTY(Transient)
    float CurrentStartTime = 0.0f;

    UPROPERTY(Transient)
    float CurrentStartFrame = 0.0f;

    UPROPERTY(Transient)
    float CurrentEndFrame = 0.0f;

    UPROPERTY(Transient)
    float BlendAlpha = 1.0f;

    UPROPERTY(Transient)
    float CurrentPlayRate = 1.0f;

    UPROPERTY(Transient)
    float PrevPlayRate = 1.0f;

    UPROPERTY(Transient)
    class UDataTable* ISMAnimationDataTable = nullptr;
};

UCLASS()
class RTSUNITTEMPLATE_API UUnitAnimationProcessor : public UMassProcessor
{
    GENERATED_BODY()

public:
    UUnitAnimationProcessor();

protected:
    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

protected:
    UPROPERTY(EditAnywhere, Category = "Mass|Visual")
    int32 StateCustomDataIndex = 1;

    UPROPERTY(EditAnywhere, Category = "Mass|Visual")
    int32 TransitionRateCustomDataIndex = 2;

    UPROPERTY(EditAnywhere, Category = "Mass|Visual")
    int32 StartTimeCustomDataIndex = 3;

    UPROPERTY(EditAnywhere, Category = "Mass|Visual")
    int32 StartFrameCustomDataIndex = 4;

    UPROPERTY(EditAnywhere, Category = "Mass|Visual")
    int32 EndFrameCustomDataIndex = 5;

    UPROPERTY(EditAnywhere, Category = "Mass|Visual")
    int32 PrevStateCustomDataIndex = 6;

    UPROPERTY(EditAnywhere, Category = "Mass|Visual")
    int32 PrevStartTimeCustomDataIndex = 7;

    UPROPERTY(EditAnywhere, Category = "Mass|Visual")
    int32 PrevStartFrameCustomDataIndex = 8;

    UPROPERTY(EditAnywhere, Category = "Mass|Visual")
    int32 PrevEndFrameCustomDataIndex = 9;

    UPROPERTY(EditAnywhere, Category = "Mass|Visual")
    int32 BlendAlphaCustomDataIndex = 10;

    UPROPERTY(EditAnywhere, Category = "Mass|Visual")
    int32 PlayRateCustomDataIndex = 11;

    UPROPERTY(EditAnywhere, Category = "Mass|Visual")
    int32 PrevPlayRateCustomDataIndex = 12;

    FMassEntityQuery EntityQuery;
};
