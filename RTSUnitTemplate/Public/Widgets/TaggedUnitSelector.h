// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "GameplayTagContainer.h" // Include for FGameplayTag
#include "TaggedUnitSelector.generated.h"

/**
 * */
UCLASS()
class RTSUNITTEMPLATE_API UTaggedUnitSelector : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
    ACustomControllerBase* ControllerBase;

    UFUNCTION()
    void InitWidget(ACustomControllerBase* InController);
    
protected:
   
    // A single function to handle clicks from ANY of our TaggedUnitButton widgets.
    UFUNCTION()
    void HandleTaggedUnitButtonClicked(FGameplayTag UnitTag);

protected:
    // We now only need pointers to our custom UTaggedUnitButton widgets.
    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonCtrl1;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonCtrl2;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonCtrl3;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonCtrl4;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonCtrl5;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonCtrl6;
    
    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonAlt1;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonAlt2;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonAlt3;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonAlt4;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonAlt5;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonAlt6;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonCtrlQ;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonCtrlW;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonCtrlE;

    UPROPERTY(meta = (BindWidget))
    class UTaggedUnitButton* TagButtonCtrlR;
};