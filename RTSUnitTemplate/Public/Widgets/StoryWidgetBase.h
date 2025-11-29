// Copyright 2025
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "StoryWidgetBase.generated.h"

/**
 * Simple storytelling widget base that can reveal text letter-by-letter and show an image.
 *
 * Create a BP subclass to design the layout and bind the following optional widgets:
 *  - TextBlock (Name: StoryText)
 *  - Image (Name: StoryImage)
 */
UCLASS(BlueprintType, Blueprintable)
class RTSUNITTEMPLATE_API UStoryWidgetBase : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Start revealing the provided text and optional image.
	UFUNCTION(BlueprintCallable, Category = "Story")
	void StartStory(const FText& InFullText, UTexture2D* InImage = nullptr);

	// Reset the widget to initial state and stop timers
	UFUNCTION(BlueprintCallable, Category = "Story")
	void ResetStory();

	// Reveal all remaining text immediately
	UFUNCTION(BlueprintCallable, Category = "Story")
	void RevealAll();

	// Characters shown per second
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story", meta=(ClampMin="1.0", ClampMax="200.0"))
	float CharactersPerSecond = 15.f;

	// Optional: auto start with DefaultText and DefaultImage in NativeConstruct
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	bool bAutoStart = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	FText DefaultText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	TObjectPtr<UTexture2D> DefaultImage = nullptr;

protected:
	// Optional bindings from BP
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> StoryText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> StoryImage;

private:
	FTimerHandle RevealTimerHandle;
	FString FullString;
	int32 RevealedCount = 0;

	void UpdateTextOnce();
};
