// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "SessionItem.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API USessionItem : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Data")
	FText ItemName;

	UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
	UTextBlock* MyTextBlock;

	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category="Data")
	void SetItemName(FText NewItemName);
};
