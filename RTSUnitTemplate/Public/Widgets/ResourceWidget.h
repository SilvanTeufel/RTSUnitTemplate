// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "ResourceWidget.generated.h"

/**
 * Widget class for displaying team resources in the game.
 */
UCLASS()
class RTSUNITTEMPLATE_API UResourceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UpdateTeamIdText();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetTeamId(int Id)
	{
		TeamId = Id;
		UpdateTeamIdText();
	}


	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void AddWorkerToResource(EResourceType ResourceType);

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void RemoveWorkerFromResource(EResourceType ResourceType);

private:
	// Timer to track when to update the resource display
	float UpdateTimer = 0.0f;

	// Interval in seconds for how often to update the resource display
	const float UpdateInterval = 1.0f;

	FTimerHandle UpdateTimerHandle;

public:
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void StartUpdateTimer();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void StopTimer();
	
protected:
	//virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
	void UpdateWidget();
	
	// Function to update the display of team resources on the widget
	void UpdateTeamResourcesDisplay();

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = RTSUnitTemplate)
	int32 TeamId;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TeamIdText;
	// TextFields for each type of resource
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = RTSUnitTemplate)
	class UTextBlock* PrimaryResourceText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = RTSUnitTemplate)
	class UTextBlock* SecondaryResourceText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = RTSUnitTemplate)
	class UTextBlock* TertiaryResourceText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = RTSUnitTemplate)
	class UTextBlock* RareResourceText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = RTSUnitTemplate)
	class UTextBlock* EpicResourceText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = RTSUnitTemplate)
	class UTextBlock* LegendaryResourceText;


	void UpdateWorkerCountDisplay();
	
    // Resource TextBlocks for displaying current worker counts
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = "RTSUnitTemplate")
    UTextBlock* PrimaryWorkerCount;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = "RTSUnitTemplate")
    UTextBlock* SecondaryWorkerCount;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = "RTSUnitTemplate")
    UTextBlock* TertiaryWorkerCount;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = "RTSUnitTemplate")
    UTextBlock* RareWorkerCount;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = "RTSUnitTemplate")
    UTextBlock* EpicWorkerCount;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = "RTSUnitTemplate")
    UTextBlock* LegendaryWorkerCount;

    // Buttons for adding workers
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = "RTSUnitTemplate")
    UButton* AddPrimaryWorkerButton;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = "RTSUnitTemplate")
    UButton* AddSecondaryWorkerButton;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = "RTSUnitTemplate")
    UButton* AddTertiaryWorkerButton;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = "RTSUnitTemplate")
    UButton* AddRareWorkerButton;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = "RTSUnitTemplate")
    UButton* AddEpicWorkerButton;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = "RTSUnitTemplate")
    UButton* AddLegendaryWorkerButton;

    // Buttons for removing workers
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = "RTSUnitTemplate")
    UButton* RemovePrimaryWorkerButton;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = "RTSUnitTemplate")
    UButton* RemoveSecondaryWorkerButton;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = "RTSUnitTemplate")
    UButton* RemoveTertiaryWorkerButton;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = "RTSUnitTemplate")
    UButton* RemoveRareWorkerButton;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = "RTSUnitTemplate")
    UButton* RemoveEpicWorkerButton;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = "RTSUnitTemplate")
    UButton* RemoveLegendaryWorkerButton;
	
};