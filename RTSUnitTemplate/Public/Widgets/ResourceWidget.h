// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
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

private:
	// Timer to track when to update the resource display
	float UpdateTimer = 0.0f;

	// Interval in seconds for how often to update the resource display
	const float UpdateInterval = 1.0f;
	
protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Function to update the display of team resources on the widget
	void UpdateTeamResourcesDisplay();

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = RTSUnitTemplate)
	int32 TeamId;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TeamIdText;
	// TextFields for each type of resource
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* PrimaryResourceText;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* SecondaryResourceText;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TertiaryResourceText;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* RareResourceText;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* EpicResourceText;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* LegendaryResourceText;
};