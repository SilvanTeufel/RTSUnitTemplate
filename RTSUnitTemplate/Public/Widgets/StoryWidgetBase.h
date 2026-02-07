// Copyright 2025
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "StoryWidgetBase.generated.h"

class UMaterialInterface;

UENUM(BlueprintType)
enum class EStoryCursorType : uint8
{
	None UMETA(DisplayName = "None"),
	Normal UMETA(DisplayName = "Normal (|)"),
	Thick UMETA(DisplayName = "Thick (█)"),
	LowerDash UMETA(DisplayName = "Lower Dash (_)"),
};

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
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Start revealing the provided text and optional image or material.
	UFUNCTION(BlueprintCallable, Category = "Story")
	void StartStory(const FText& InFullText, UTexture2D* InImage = nullptr, UMaterialInterface* InMaterial = nullptr);

	// Reset the widget to initial state and stop timers
	UFUNCTION(BlueprintCallable, Category = "Story")
	void ResetStory();

	// Reveal all remaining text immediately
	UFUNCTION(BlueprintCallable, Category = "Story")
	void RevealAll();

	// Characters shown per second
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story", meta=(ClampMin="1.0", ClampMax="200.0"))
	float CharactersPerSecond = 15.f;

	// Optional: auto start with DefaultText and DefaultImage/DefaultMaterial in NativeConstruct
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	bool bAutoStart = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	FText DefaultText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	TObjectPtr<UTexture2D> DefaultImage = nullptr;

	// Optional default material for the image widget (takes precedence over DefaultImage if set)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	TObjectPtr<UMaterialInterface> DefaultMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	EStoryCursorType CursorType = EStoryCursorType::None;

protected:
	// Optional bindings from BP
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UBorder> StoryBorder;

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
