// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "MassEntityTypes.h"
#include "UObject/WeakObjectPtr.h"
#include "MassUnitHoverProcessor.generated.h"

class AMassUnitBase;
class USkeletalMeshComponent;
class UMassSignalSubsystem;

/**
 * Processor that handles hover detection for Mass units and buildings.
 * Runs on the local client and triggers CustomOverlapStart/End on the unit actors via signals.
 */
UCLASS()
class RTSUNITTEMPLATE_API UMassUnitHoverProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassUnitHoverProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void BeginDestroy() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	UPROPERTY()
	FMassEntityHandle LastHoveredEntity;

	UPROPERTY()
	UMassSignalSubsystem* SignalSubsystem = nullptr;

	FMassEntityQuery EntityQuery;
	float AccumulatedTime = 0.f;

	UFUNCTION()
	void HandleCustomOverlapStart(FName SignalName, TArray<FMassEntityHandle>& Entities);

	UFUNCTION()
	void HandleCustomOverlapEnd(FName SignalName, TArray<FMassEntityHandle>& Entities);

	FDelegateHandle CustomOverlapStartDelegateHandle;
	FDelegateHandle CustomOverlapEndDelegateHandle;

	UPROPERTY(EditAnywhere, Category = "RTS|Hover")
	bool bSetCustomDataValue = true;
};
