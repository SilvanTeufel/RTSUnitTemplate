#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "WinConditionWidget.generated.h"

/**
 * Widget that explains the current map's win condition to the player.
 */
UCLASS()
class RTSUNITTEMPLATE_API UWinConditionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	UTextBlock* ConditionText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UI")
	FText WaitingForTeamText = FText::FromString(TEXT("Waiting for Team Assignment..."));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UI")
	FText NoConditionText = FText::FromString(TEXT("No win condition defined for this map."));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UI")
	FText GoalPrefix = FText::FromString(TEXT("Goal: "));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UI")
	FText NoneConditionText = FText::FromString(TEXT("None (Sandbox)"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UI")
	FText AllBuildingsDestroyedText = FText::FromString(TEXT("Destroy all enemy buildings to win. Protect your own!"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UI")
	FText TaggedUnitDestroyedText = FText::FromString(TEXT("Eliminate the target: {0}"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UI")
	FText ResourcesConditionText = FText::FromString(TEXT("Gather the required amount of resources:\n"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UI")
	FText SurvivalConditionText = FText::FromString(TEXT("Survive for {0} seconds."));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UI")
	FText UnknownConditionText = FText::FromString(TEXT("Unknown"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UI")
	FText ProgressFormatText = FText::FromString(TEXT("[{0}/{1}] {2}"));

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void UpdateConditionText();

	UFUNCTION()
	void OnTeamIdChanged(int32 NewTeamId);

	UFUNCTION()
	void OnWinConditionChanged(AWinLoseConfigActor* Config, EWinLoseCondition NewCondition);
};
