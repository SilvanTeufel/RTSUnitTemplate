// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilityUnit.h"
#include "GameFramework/Character.h"
#include "Components/WidgetComponent.h"
#include "Core/UnitData.h"
#include "Actors/WorkArea.h"
#include "Actors/WorkResource.h"
#include "PathSeekerBase.h"
#include "WorkingUnitBase.generated.h"


UCLASS()
class RTSUNITTEMPLATE_API AWorkingUnitBase : public AAbilityUnit
{
	GENERATED_BODY()
	
public:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Worker)
	class AWorkArea* ResourcePlace;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Worker)
	class AWorkArea* Base;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Worker)
	class AWorkArea* BuildArea;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Worker)
	TSubclassOf<class AWorkResource> WorkResourceClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Worker)
	AWorkResource* WorkResource;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Worker)
	float ResourceExtractionTime = 2.f;
	
};








