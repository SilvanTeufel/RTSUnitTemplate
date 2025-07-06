// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "MassProcessor.h"
#include "GamePlayEffectProcessor.generated.h"

struct FCasterData
{
	FVector Position = FVector::ZeroVector;
	float RadiusSq = 0.f;
	int32 TeamId = -1;
	TSubclassOf<UGameplayEffect> FriendlyEffect = nullptr;
	TSubclassOf<UGameplayEffect> EnemyEffect = nullptr;
};
/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UGamePlayEffectProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UGamePlayEffectProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	float ExecutionInterval = 1.f;
	
private:
	//FMassEntityQuery EntityQuery;
	FMassEntityQuery CasterQuery;
	FMassEntityQuery TargetQuery;
	
	float TimeSinceLastRun = 0.0f;
};
