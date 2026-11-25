// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Characters/Unit/UnitBase.h"
#include "GameplayTagContainer.h"
#include "UnitTimerWidget.generated.h"

class UImage;
class UMaterialInterface;

/** Frame look of the cast bar for one kind of unit (e.g. a faction): the first matching entry wins. */
USTRUCT(BlueprintType)
struct RTSUNITTEMPLATE_API FUnitTimerFrameStyle
{
	GENERATED_BODY()

	/** Matches units whose UnitTags contain this tag (parents count, Units.Xeno matches Units.Xeno.Kraken). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag UnitTag;

	/** Or: matches units whose class name contains this text (e.g. "Xeno"). Empty = not used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FString ClassNameContains;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TObjectPtr<UMaterialInterface> FrameMaterial = nullptr;

	/** Fill of the bar (a UI material; FillColorAndOpacity reaches it as vertex colour). None = DefaultFillMaterial. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TObjectPtr<UMaterialInterface> FillMaterial = nullptr;
};

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UUnitTimerWidget : public UUserWidget
{
	GENERATED_BODY()
	virtual void NativeConstruct() override;
	
public:
	void SetOwnerActor(AUnitBase* Unit);

	/**
	 * Optional frame around the bar (an Image behind it, e.g. in an Overlay): shown only while the bar is, with the
	 * FrameMaterial of the first matching FrameStyles entry, else DefaultFrameMaterial (None = keep the brush).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Frame")
	TArray<FUnitTimerFrameStyle> FrameStyles;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Frame")
	TObjectPtr<UMaterialInterface> DefaultFrameMaterial = nullptr;

	/** Fill of the bar when no FrameStyles entry brings one. None = keep the bar's own fill brush. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Frame")
	TObjectPtr<UMaterialInterface> DefaultFillMaterial = nullptr;


protected:
	//void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	TWeakObjectPtr<AUnitBase> OwnerCharacter;

	UPROPERTY(meta = (BindWidget))
		class UProgressBar* TimerBar;

	UPROPERTY(meta = (BindWidgetOptional))
		class UTextBlock* TransportText;

	/** See FrameStyles. */
	UPROPERTY(meta = (BindWidgetOptional))
		UImage* BarFrame = nullptr;

	void UpdateBar();
	void ApplyFrameStyle();

	
public:

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = RTSUnitTemplate)
	bool MyWidgetIsVisible = false;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	FLinearColor CastingColor = FLinearColor::Red; // Replace with your desired color

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	FLinearColor PauseColor = FLinearColor::Yellow;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	FLinearColor TransportColor = FLinearColor::Blue;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	FLinearColor BuildColor = FLinearColor::Black;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	FLinearColor ExtractionColor = FLinearColor::Black;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	bool DisableAutoAttack = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	bool DisableBuild = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float UpdateInterval = 0.15f;
	
	FTimerHandle TickTimerHandle;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void TimerTick();
};
