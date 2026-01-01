// ResourceWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameModes/ResourceGameMode.h" // For EResourceType
#include "ResourceWidget.generated.h"

// Forward declarations
class UTextBlock;
class UVerticalBox;
class UPanelWidget;
class UResourceEntryWidget;
class UTexture2D;

UCLASS()
class RTSUNITTEMPLATE_API UResourceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UResourceWidget(const FObjectInitializer& ObjectInitializer);

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
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = RTSUnitTemplate)
	UPanelWidget* ResourceEntriesBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget), Category = RTSUnitTemplate)
	UTextBlock* TeamIdText;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget Setup")
	TSubclassOf<UResourceEntryWidget> ResourceEntryWidgetClass;

private:
	FTimerHandle UpdateTimerHandle;
	
	const float UpdateInterval = 1.0f;

public:
	// Optional per-resource icon overrides editable in the ResourceWidget
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Appearance")
	TMap<EResourceType, UTexture2D*> ResourceIcons;

	// Optional per-resource display name overrides
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Appearance")
	TMap<EResourceType, FText> ResourceDisplayNames;

	// If > 0, limit how many resource entries are populated
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Behavior")
	int32 MaxResourcesToDisplay = -1;

	// Optional per-resource worker UI collapse overrides
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Behavior")
	TMap<EResourceType, bool> ResourceWorkerUICollapseOverrides;
};