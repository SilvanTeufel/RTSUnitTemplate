// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "MassProcessor.h"
#include "MassRotateToMouseProcessor.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API UMassRotateToMouseProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UMassRotateToMouseProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
private:
	void HandleMouseUpdateSignal(FName SignalName, TConstArrayView<FMassEntityHandle> Entities);
	FMassEntityQuery EntityQuery;
};
