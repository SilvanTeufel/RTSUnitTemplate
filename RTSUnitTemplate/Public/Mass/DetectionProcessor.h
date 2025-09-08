// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassEntitySubsystem.h"
#include "MassProcessor.h"
#include "MassSignalSubsystem.h"
#include "MassSignalTypes.h"
#include "UnitMassTag.h"
#include "Signals/UnitSignalingProcessor.h"
#include "DetectionProcessor.generated.h"

struct FMassExecutionContext;
struct FTransformFragment;
struct FMassAITargetFragment;
struct FMassCombatStatsFragment;
struct FMassAgentCharacteristicsFragment;
struct FTargetUnitInfo
{
	FMassEntityHandle                          Entity;
	FVector                                    Location;
	FMassAIStateFragment*                      State;
	//FMassAITargetFragment*                     TargetFrag;
	//FMassMoveTargetFragment*                   MoveFrag;
	const FMassCombatStatsFragment*            Stats;
	const FMassAgentCharacteristicsFragment*   Char;
	const FMassSightFragment* Sight;
};
struct FDetectorUnitInfo
{
	FMassEntityHandle                          Entity;
	FVector                                    Location;
	FMassAIStateFragment*                      State;
	FMassAITargetFragment*                     TargetFrag;
	//FMassMoveTargetFragment*                   MoveFrag;
	const FMassCombatStatsFragment*            Stats;
	const FMassAgentCharacteristicsFragment*   Char;
};

UCLASS()
class RTSUNITTEMPLATE_API UDetectionProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UDetectionProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void BeginDestroy() override;

	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	
private:

	UPROPERTY(Transient)
	UWorld* World;

	void HandleUnitPresenceSignal(FName SignalName, TConstArrayView<FMassEntityHandle> Entities);

	void InjectCurrentTargetIfMissing(const FDetectorUnitInfo& DetectorInfo, TArray<FTargetUnitInfo>& InOutTargetUnits, FMassEntityManager& EntityManager);

	FMassEntityQuery EntityQuery;
	
	float TimeSinceLastRun = 0.0f;
	const float ExecutionInterval = 0.2f; // Intervall für die Detektion (z.B. 5x pro Sekunde)

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UMassEntitySubsystem> EntitySubsystem;
	
	FDelegateHandle SignalDelegateHandle;

	TMap<FName, TArray<FMassEntityHandle>> ReceivedSignalsBuffer;
	
	TSet<FMassEntityHandle> SignaledEntitiesProcessedThisTick;
};