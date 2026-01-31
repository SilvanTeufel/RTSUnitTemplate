#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/RichTextBlockDecorator.h"
#include "Core/WorkerData.h"
#include "WinConditionWidget.generated.h"

class URichTextBlock;
class UTexture2D;
class UImage;
struct FBuildingCost;

/**
 * Widget that explains the current map's win condition to the player.
 */
UCLASS()
class RTSUNITTEMPLATE_API UWinConditionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* ConditionText;

	UPROPERTY(meta = (BindWidgetOptional))
	URichTextBlock* RichConditionText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UI")
	TMap<EResourceType, FText> ResourceDisplayNames;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UI")
	TMap<EResourceType, UTexture2D*> ResourceIcons;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UI")
	bool bShowOnlyIconsAndCost = false;

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
	FText TaggedUnitsDestroyedText = FText::FromString(TEXT("Eliminate the targets: {0}"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UI")
	FText TaggedUnitsSpawnedText = FText::FromString(TEXT("Build units: {0}"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UI")
	FText TaggedUnitsSpawnedFormat = FText::FromString(TEXT("{0}/{1} {2}"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UI")
	FText ResourcesConditionText = FText::FromString(TEXT("Gather the required amount of resources:\n"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UI")
	FText SurvivalConditionText = FText::FromString(TEXT("Survive for {0} seconds."));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UI")
	FText UnknownConditionText = FText::FromString(TEXT("Unknown"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UI")
	FText ProgressFormatText = FText::FromString(TEXT("[{0}/{1}] {2}"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidgetOptional), Category = RTSUnitTemplate)
	UImage* ResourceIconPrimary;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidgetOptional), Category = RTSUnitTemplate)
	UTextBlock* ResourceNamePrimary;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidgetOptional), Category = RTSUnitTemplate)
	UTextBlock* ResourceAmountPrimary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidgetOptional), Category = RTSUnitTemplate)
	UImage* ResourceIconSecondary;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidgetOptional), Category = RTSUnitTemplate)
	UTextBlock* ResourceNameSecondary;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidgetOptional), Category = RTSUnitTemplate)
	UTextBlock* ResourceAmountSecondary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidgetOptional), Category = RTSUnitTemplate)
	UImage* ResourceIconTertiary;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidgetOptional), Category = RTSUnitTemplate)
	UTextBlock* ResourceNameTertiary;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidgetOptional), Category = RTSUnitTemplate)
	UTextBlock* ResourceAmountTertiary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidgetOptional), Category = RTSUnitTemplate)
	UImage* ResourceIconRare;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidgetOptional), Category = RTSUnitTemplate)
	UTextBlock* ResourceNameRare;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidgetOptional), Category = RTSUnitTemplate)
	UTextBlock* ResourceAmountRare;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidgetOptional), Category = RTSUnitTemplate)
	UImage* ResourceIconEpic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidgetOptional), Category = RTSUnitTemplate)
	UTextBlock* ResourceNameEpic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidgetOptional), Category = RTSUnitTemplate)
	UTextBlock* ResourceAmountEpic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidgetOptional), Category = RTSUnitTemplate)
	UImage* ResourceIconLegendary;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidgetOptional), Category = RTSUnitTemplate)
	UTextBlock* ResourceNameLegendary;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidgetOptional), Category = RTSUnitTemplate)
	UTextBlock* ResourceAmountLegendary;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void StartUpdateTimer();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void StopTimer();

protected:
	virtual void NativeConstruct() override;

	FTimerHandle UpdateTimerHandle;
	const float UpdateInterval = 1.0f;

	UFUNCTION()
	void UpdateConditionText();

	UFUNCTION()
	void OnTeamIdChanged(int32 NewTeamId);

	UFUNCTION()
	void OnWinConditionChanged(AWinLoseConfigActor* Config, EWinLoseCondition NewCondition);

	UFUNCTION()
	void OnTagProgressUpdated(AWinLoseConfigActor* Config);

	void UpdateResourceWidgets(const FBuildingCost& Cost, bool bVisible);

	FText FormatResourceCost(const FBuildingCost& Cost, bool bIncludeTags);
};
