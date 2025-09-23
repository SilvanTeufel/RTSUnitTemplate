// ResourceWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameModes/ResourceGameMode.h" // For EResourceType
#include "ResourceWidget.generated.h"

// Forward declarations
class UTextBlock;
class UVerticalBox;
class UResourceEntryWidget;

UCLASS()
class RTSUNITTEMPLATE_API UResourceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
    
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetTeamId(int32 Id);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void UpdateTeamIdText();
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void StartUpdateTimer();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void StopTimer();

protected:
	void UpdateWidget();
	
	void PopulateResourceList();
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int32 TeamId;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	UPanelWidget* ResourceEntriesBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* TeamIdText;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget Setup")
	TSubclassOf<UResourceEntryWidget> ResourceEntryWidgetClass;

private:
	FTimerHandle UpdateTimerHandle;
	
	const float UpdateInterval = 1.0f;
};