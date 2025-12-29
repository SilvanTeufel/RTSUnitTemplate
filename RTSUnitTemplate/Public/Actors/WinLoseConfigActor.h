#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Core/UnitData.h"
#include "WinLoseConfigActor.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API AWinLoseConfigActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AWinLoseConfigActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	EWinLoseCondition WinLoseCondition = EWinLoseCondition::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	FGameplayTag WinLoseTargetTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	TSubclassOf<class UWinLoseWidget> WinLoseWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	TSoftObjectPtr<UWorld> WinLoseTargetMapName;

protected:
	virtual void BeginPlay() override;

};
