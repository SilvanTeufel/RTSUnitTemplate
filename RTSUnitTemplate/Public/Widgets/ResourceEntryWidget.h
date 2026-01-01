// ResourceEntryWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameModes/ResourceGameMode.h" // For EResourceType
#include "ResourceEntryWidget.generated.h"

// Forward declarations
class UTextBlock;
class UButton;
class UImage;
class UTexture2D;

// Declare a delegate that the parent widget can bind to.
// This allows the entry widget to tell the parent when a button is clicked.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorkerChangeRequested, EResourceType, ResourceType);

UCLASS()
class RTSUNITTEMPLATE_API UResourceEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Sets the data for this widget and updates its visual representation. */
	void SetResourceData(EResourceType InResourceType, const FText& InResourceName, float InResourceAmount, int32 InWorkerCount, int32 PlayerTeamId, UTexture2D* InIconTexture = nullptr, float InMaxResourceAmount = 0.0f, bool bInIsSupplyLike = false, bool bInCollapseWorkerUI = false);

	/** The type of resource this widget entry represents. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	EResourceType ResourceType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int32 TeamId = 0;

	/** Number of decimal places to display for the resource amount. Default is 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int32 ResourceAmountDecimalPlaces = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bCollapseWorkerUI = false;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UpdateWorkerCount(int32 AmountToAdd);
	
protected:

	// UI elements bound from the UMG Editor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = RTSUnitTemplate)
	UTextBlock* ResourceNameText;

	// Optional icon image (set up in the UMG designer)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidgetOptional), Category = RTSUnitTemplate)
	UImage* ResourceIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = RTSUnitTemplate)
	UTextBlock* ResourceAmountText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = RTSUnitTemplate)
	UTextBlock* WorkerCountText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = RTSUnitTemplate)
	UButton* AddWorkerButton;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = RTSUnitTemplate)
	UButton* RemoveWorkerButton;
};