// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Characters/Unit/UnitBase.h"
#include "ConstructionUnit.generated.h"

class AWorkArea;

UCLASS()
class RTSUNITTEMPLATE_API AConstructionUnit : public AUnitBase
{
	GENERATED_BODY()
public:
	AConstructionUnit(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// The worker assigned to construct this site
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = Construction)
	AUnitBase* Worker = nullptr;

	// The work/build area for this construction site
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = Construction)
	AWorkArea* WorkArea = nullptr;
};
