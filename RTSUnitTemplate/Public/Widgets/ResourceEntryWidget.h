// ResourceEntryWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameModes/ResourceGameMode.h" // For EResourceType
#include "ResourceEntryWidget.generated.h"

// Forward declarations
class UTextBlock;
class UButton;

// Declare a delegate that the parent widget can bind to.
// This allows the entry widget to tell the parent when a button is clicked.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorkerChangeRequested, EResourceType, ResourceType);

UCLASS()
class RTSUNITTEMPLATE_API UResourceEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Sets the data for this widget and updates its visual representation. */
	void SetResourceData(EResourceType InResourceType, const FText& InResourceName, float InResourceAmount, int32 InWorkerCount, int32 PlayerTeamId);

	/** The type of resource this widget entry represents. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resource")
	EResourceType ResourceType;

	// Delegates for button clicks
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnWorkerChangeRequested OnAddWorkerRequested;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnWorkerChangeRequested OnRemoveWorkerRequested;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int32 TeamId = 0;

	UFUNCTION(BlueprintCallable, Category = "Resource")
	void UpdateWorkerCount(int32 AmountToAdd);
protected:
	virtual void NativeConstruct() override;

	// UI elements bound from the UMG Editor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* ResourceNameText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* ResourceAmountText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* WorkerCountText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	UButton* AddWorkerButton;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	UButton* RemoveWorkerButton;

private:
	// Functions to handle button clicks internally
	UFUNCTION()
	void HandleAddWorkerClicked();

	UFUNCTION()
	void HandleRemoveWorkerClicked();
};