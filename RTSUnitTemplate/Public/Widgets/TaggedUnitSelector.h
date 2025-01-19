// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"
#include "TaggedUnitSelector.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UTaggedUnitSelector : public UUserWidget
{
	GENERATED_BODY()

public:
	// Constructor
	UTaggedUnitSelector(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	AExtendedControllerBase* ControllerBase;
	
protected:
	virtual void NativeConstruct() override;

	// --- Ctrl 1-6 ---
	UPROPERTY(meta = (BindWidget))
	class UButton* TagButtonCtrl1;

	UPROPERTY(meta = (BindWidget))
	class UButton* TagButtonCtrl2;

	UPROPERTY(meta = (BindWidget))
	class UButton* TagButtonCtrl3;

	UPROPERTY(meta = (BindWidget))
	class UButton* TagButtonCtrl4;

	UPROPERTY(meta = (BindWidget))
	class UButton* TagButtonCtrl5;

	UPROPERTY(meta = (BindWidget))
	class UButton* TagButtonCtrl6;

	// --- Alt 1-6 ---
	UPROPERTY(meta = (BindWidget))
	class UButton* TagButtonAlt1;

	UPROPERTY(meta = (BindWidget))
	class UButton* TagButtonAlt2;

	UPROPERTY(meta = (BindWidget))
	class UButton* TagButtonAlt3;

	UPROPERTY(meta = (BindWidget))
	class UButton* TagButtonAlt4;

	UPROPERTY(meta = (BindWidget))
	class UButton* TagButtonAlt5;

	UPROPERTY(meta = (BindWidget))
	class UButton* TagButtonAlt6;

	// --- Ctrl Q, W, E, R ---
	UPROPERTY(meta = (BindWidget))
	class UButton* TagButtonCtrlQ;

	UPROPERTY(meta = (BindWidget))
	class UButton* TagButtonCtrlW;

	UPROPERTY(meta = (BindWidget))
	class UButton* TagButtonCtrlE;

	UPROPERTY(meta = (BindWidget))
	class UButton* TagButtonCtrlR;

	// --- Icons (one per button) ---
	UPROPERTY(meta = (BindWidget))
	class UImage* IconCtrl1;

	UPROPERTY(meta = (BindWidget))
	class UImage* IconCtrl2;

	UPROPERTY(meta = (BindWidget))
	class UImage* IconCtrl3;

	UPROPERTY(meta = (BindWidget))
	class UImage* IconCtrl4;

	UPROPERTY(meta = (BindWidget))
	class UImage* IconCtrl5;

	UPROPERTY(meta = (BindWidget))
	class UImage* IconCtrl6;

	UPROPERTY(meta = (BindWidget))
	class UImage* IconAlt1;

	UPROPERTY(meta = (BindWidget))
	class UImage* IconAlt2;

	UPROPERTY(meta = (BindWidget))
	class UImage* IconAlt3;

	UPROPERTY(meta = (BindWidget))
	class UImage* IconAlt4;

	UPROPERTY(meta = (BindWidget))
	class UImage* IconAlt5;

	UPROPERTY(meta = (BindWidget))
	class UImage* IconAlt6;

	UPROPERTY(meta = (BindWidget))
	class UImage* IconCtrlQ;

	UPROPERTY(meta = (BindWidget))
	class UImage* IconCtrlW;

	UPROPERTY(meta = (BindWidget))
	class UImage* IconCtrlE;

	UPROPERTY(meta = (BindWidget))
	class UImage* IconCtrlR;

protected:
	// Override to build the Slate layout manually
	//virtual TSharedRef<SWidget> RebuildWidget() override;

	// Example function to handle a click event (Ctrl1). 
	// You can replicate this pattern for other buttons if desired.
	// --- Click handlers for Ctrl1..6 ---
	UFUNCTION()
	void OnTagButtonCtrl1Clicked();

	UFUNCTION()
	void OnTagButtonCtrl2Clicked();

	UFUNCTION()
	void OnTagButtonCtrl3Clicked();

	UFUNCTION()
	void OnTagButtonCtrl4Clicked();

	UFUNCTION()
	void OnTagButtonCtrl5Clicked();

	UFUNCTION()
	void OnTagButtonCtrl6Clicked();

	// --- Click handlers for CtrlQ, CtrlW, CtrlE, CtrlR ---
	UFUNCTION()
	void OnTagButtonCtrlQClicked();

	UFUNCTION()
	void OnTagButtonCtrlWClicked();

	UFUNCTION()
	void OnTagButtonCtrlEClicked();

	UFUNCTION()
	void OnTagButtonCtrlRClicked();

	// --- Click handlers for Alt1..6 ---
	UFUNCTION()
	void OnTagButtonAlt1Clicked();

	UFUNCTION()
	void OnTagButtonAlt2Clicked();

	UFUNCTION()
	void OnTagButtonAlt3Clicked();

	UFUNCTION()
	void OnTagButtonAlt4Clicked();

	UFUNCTION()
	void OnTagButtonAlt5Clicked();

	UFUNCTION()
	void OnTagButtonAlt6Clicked();
};
