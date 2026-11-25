// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
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
class UButton;

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

	/** Reads the numbers from the game state now (the entries call it after "+"/"-"). */
	void UpdateWidget();

protected:
	
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

	/**
	 * Steps of the "worker range" row under the resources (uu, 0 = unlimited): automatically assigned workers only
	 * go to resources this close to one of the team's bases, otherwise they stay idle. Click cycles the steps; the
	 * choice is kept per machine. Empty = no row.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Behavior")
	TArray<float> WorkerRangeSteps = { 3000.f, 5000.f, 8000.f, 12000.f, 0.f };

	/** Step used until the player picks one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Behavior", meta = (ClampMin = "0"))
	int32 DefaultWorkerRangeStep = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Behavior", meta = (ClampMin = "6"))
	int32 WorkerRangeFontSize = 11;

protected:
	UFUNCTION()
	void HandleWorkerRangeClicked();

private:
	void EnsureWorkerRangeRow();
	void ApplyWorkerRange(bool bSendToServer);

	UPROPERTY(Transient)
	TObjectPtr<UButton> WorkerRangeButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WorkerRangeText;

	int32 WorkerRangeStep = INDEX_NONE;
};