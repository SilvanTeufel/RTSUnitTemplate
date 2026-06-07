// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "Core/UnitData.h"
#include "UnitAnimationProcessor.generated.h"

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
    bool bSetCustomDataValue = true;

    FMassEntityQuery EntityQuery;
};
