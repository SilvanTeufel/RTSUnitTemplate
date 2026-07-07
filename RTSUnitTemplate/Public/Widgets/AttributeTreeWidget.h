// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "AttributeTreeWidget.generated.h"

class ALevelUnit;
class SAttributeTreeWidget;

/**
 * UMG wrapper that hosts the radial SAttributeTreeWidget (Slate) and bridges it to the
 * replicated ALevelUnit attribute-tree state. Drive it with SetTargetUnit().
 */
UCLASS()
class RTSUNITTEMPLATE_API UAttributeTreeWidget : public UWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void SetTargetUnit(ALevelUnit* InUnit);

	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate")
	ALevelUnit* GetTargetUnit() const { return TargetUnit; }

	// --- Style ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Style")
	float RingSpacing = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Style")
	float NodeRadius = 26.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Style", meta = (ClampMin = "0.0"))
	float TooltipDelaySeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Style")
	FLinearColor LockedColor = FLinearColor(0.10f, 0.10f, 0.12f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Style")
	FLinearColor AvailableColor = FLinearColor(0.20f, 0.65f, 1.00f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Style")
	FLinearColor PartialColor = FLinearColor(0.95f, 0.75f, 0.15f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Style")
	FLinearColor FullColor = FLinearColor(0.20f, 0.85f, 0.35f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Style")
	FLinearColor LinkColor = FLinearColor(0.35f, 0.35f, 0.40f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Style")
	FLinearColor RingColor = FLinearColor(1.f, 1.f, 1.f, 0.06f);

	// --- Text sizes ---
	/** Font size of the per-node "invested/max" count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Text", meta = (ClampMin = "4"))
	int32 CountFontSize = 8;

	/** Font size of the empty-state hint and the Reset button. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Text", meta = (ClampMin = "4"))
	int32 NodeFontSize = 9;

	// --- Tooltip ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Tooltip", meta = (ClampMin = "4"))
	int32 TooltipTitleFontSize = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Tooltip", meta = (ClampMin = "4"))
	int32 TooltipBodyFontSize = 13;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Tooltip", meta = (ClampMin = "50"))
	float TooltipMaxWidth = 420.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Tooltip", meta = (ClampMin = "0"))
	float TooltipPadding = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Tooltip", meta = (ClampMin = "0"))
	float TooltipIconSize = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Tree|Tooltip", meta = (ClampMin = "0"))
	float TooltipIconGap = 8.f;

	// --- UWidget interface ---
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** Rebuild the node model from the target's AttributeTreeDataTable. */
	void RefreshNodes();

	// Slate live-state / action callbacks
	int32 HandleGetNodePoints(FName NodeId) const;
	int32 HandleGetAvailablePoints() const;
	bool HandleIsUnlocked(FName NodeId) const;
	void HandleInvest(FName NodeId);
	void HandleReset();

	UPROPERTY()
	ALevelUnit* TargetUnit = nullptr;

	TSharedPtr<SAttributeTreeWidget> MyTree;
};
