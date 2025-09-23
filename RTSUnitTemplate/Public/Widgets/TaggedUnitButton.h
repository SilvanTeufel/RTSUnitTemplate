// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "TaggedUnitButton.generated.h"

// Declare a dynamic multicast delegate. This is how this button will tell its owner (the TaggedUnitSelector) that it was clicked.
// It passes along the FGameplayTag associated with this button.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTaggedButtonClicked, FGameplayTag, UnitTag);

/**
 * A reusable button widget that includes an icon and is associated with a Gameplay Tag.
 */
UCLASS()
class RTSUNITTEMPLATE_API UTaggedUnitButton : public UUserWidget
{
	GENERATED_BODY()

public:
	// This is the delegate that other classes can bind to.
	UPROPERTY(BlueprintAssignable, Category = "Button")
	FOnTaggedButtonClicked OnClicked;

	// The Gameplay Tag this button is responsible for selecting.
	// 'ExposeOnSpawn' makes it visible in the parent Widget Blueprint when you add this widget.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate", meta = (ExposeOnSpawn = "true"))
	FGameplayTag UnitTagToSelect;

	// Public function to allow the parent to set the icon texture.
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void SetIcon(UTexture2D* NewIcon);

protected:
	// Overriding NativeConstruct to bind our internal button's OnClicked event.
	virtual void NativeConstruct() override;

public:
	// --- Bound Widgets ---
	// These are the actual UMG components inside this widget's Blueprint.
	UPROPERTY(meta = (BindWidget), EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	class UButton* MainButton;

	UPROPERTY(meta = (BindWidget), EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate")
	class UImage* IconImage;

private:
	// Internal function that gets called when MainButton is clicked.
	UFUNCTION()
	void HandleButtonClicked();
};