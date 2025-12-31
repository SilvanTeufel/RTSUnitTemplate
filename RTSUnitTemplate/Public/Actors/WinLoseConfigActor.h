#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Core/UnitData.h"
#include "WinLoseConfigActor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWinLoseEvent);

UCLASS()
class RTSUNITTEMPLATE_API AWinLoseConfigActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AWinLoseConfigActor();

	UPROPERTY(BlueprintAssignable, Category = "RTSUnitTemplate|WinLose")
	FOnWinLoseEvent OnYouWonTheGame;

	UPROPERTY(BlueprintAssignable, Category = "RTSUnitTemplate|WinLose")
	FOnWinLoseEvent OnYouLostTheGame;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	float WinLoseDelay = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	EWinLoseCondition WinLoseCondition = EWinLoseCondition::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	int32 TeamId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	FBuildingCost TargetResourceCount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	float TargetGameTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	FGameplayTag WinLoseTargetTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	TSubclassOf<class UWinLoseWidget> WinLoseWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	TSoftObjectPtr<UWorld> WinLoseTargetMapName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FName DestinationSwitchTagToEnable;

protected:
	virtual void BeginPlay() override;

};
