#include "Widgets/StoryWidgetBase.h"
#include "TimerManager.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Border.h"

void UStoryWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	// Initialize UI state
	RevealedCount = 0;

	// Dynamic fallback UI: if bindings are missing, create a basic layout
	if (!StoryBorder || !StoryText || !StoryImage)
	{
		if (WidgetTree && !WidgetTree->RootWidget)
		{
			StoryBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StoryBorder"));
			WidgetTree->RootWidget = StoryBorder;
		}

		if (WidgetTree)
		{
			if (StoryBorder)
			{
				// Chat window look: transparent background
				StoryBorder->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.5f));

				if (StoryBorder->GetContent() == nullptr)
				{
					UVerticalBox* ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
					StoryBorder->SetContent(ContentBox);
				}
			}

			UVerticalBox* RootBox = StoryBorder ? Cast<UVerticalBox>(StoryBorder->GetContent()) : Cast<UVerticalBox>(WidgetTree->RootWidget);
			if (!RootBox && WidgetTree->RootWidget && !StoryBorder)
			{
				RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
				WidgetTree->RootWidget = RootBox;
			}

			if (RootBox)
			{
				if (!StoryImage)
				{
					StoryImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("StoryImage"));
					if (UVerticalBoxSlot* ImgSlot = RootBox->AddChildToVerticalBox(StoryImage))
					{
						ImgSlot->SetHorizontalAlignment(HAlign_Center);
					}
				}
				if (!StoryText)
				{
					StoryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StoryText"));
					if (UVerticalBoxSlot* TxtSlot = RootBox->AddChildToVerticalBox(StoryText))
					{
						TxtSlot->SetHorizontalAlignment(HAlign_Fill);
					}
				}
			}
		}
	}

	if (StoryBorder)
	{
		// Ensure chat window look even if bound in BP
		if (StoryBorder->GetBrushColor().A > 0.9f) // If it's fully opaque or default, make it transparent
		{
			StoryBorder->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.5f));
		}
	}

	if (StoryText)
	{
		StoryText->SetAutoWrapText(true);
		StoryText->SetText(FText::GetEmpty());
	}
	if (StoryImage)
	{
		if (DefaultMaterial)
		{
			StoryImage->SetBrushFromMaterial(DefaultMaterial.Get());
		}
		else
		{
			StoryImage->SetBrushFromTexture(DefaultImage.Get());
		}
	}

	if (bAutoStart && (!DefaultText.IsEmpty()))
	{
		StartStory(DefaultText, DefaultImage, DefaultMaterial);
	}
}

void UStoryWidgetBase::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RevealTimerHandle);
	}
	Super::NativeDestruct();
}

void UStoryWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (CursorType != EStoryCursorType::None && StoryText && !FullString.IsEmpty())
	{
		bool bIsRevealing = RevealTimerHandle.IsValid();
		if (bIsRevealing)
		{
			// Blink at 4Hz
			bool bCursorVisible = FMath::FloorToInt(GetWorld()->GetTimeSeconds() * 4.0f) % 2 == 0;

			FString DisplayString = FullString.Left(RevealedCount);
			if (bCursorVisible)
			{
				switch (CursorType)
				{
				case EStoryCursorType::Normal:
					DisplayString += TEXT("|");
					break;
				case EStoryCursorType::Thick:
					DisplayString += TEXT("█");
					break;
				case EStoryCursorType::LowerDash:
					DisplayString += TEXT("_");
					break;
				default:
					DisplayString += TEXT("|");
					break;
				}
			}
			StoryText->SetText(FText::FromString(DisplayString));
		}
	}
}

void UStoryWidgetBase::StartStory(const FText& InFullText, UTexture2D* InImage, UMaterialInterface* InMaterial)
{
	if (StoryImage)
	{
		UMaterialInterface* UseMat = InMaterial ? InMaterial : DefaultMaterial.Get();
		if (UseMat)
		{
			StoryImage->SetBrushFromMaterial(UseMat);
		}
		else
		{
			UTexture2D* UseTex = InImage ? InImage : DefaultImage.Get();
			StoryImage->SetBrushFromTexture(UseTex);
		}
	}

	FullString = InFullText.ToString();
	RevealedCount = 0;
	if (StoryText)
	{
		StoryText->SetText(FText::FromString(TEXT("")));
	}

	if (UWorld* World = GetWorld())
	{
		const float Interval = 1.0f / FMath::Max(CharactersPerSecond, 1.0f);
		World->GetTimerManager().SetTimer(RevealTimerHandle, this, &UStoryWidgetBase::UpdateTextOnce, Interval, true);
	}
}

void UStoryWidgetBase::ResetStory()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RevealTimerHandle);
	}
	FullString.Empty();
	RevealedCount = 0;
	if (StoryText)
	{
		StoryText->SetText(FText::GetEmpty());
	}
	if (StoryImage)
	{
		// Clear both material and texture bindings
		StoryImage->SetBrushFromMaterial(nullptr);
		StoryImage->SetBrushFromTexture(nullptr);
	}
}

void UStoryWidgetBase::RevealAll()
{
	if (!FullString.IsEmpty())
	{
		RevealedCount = FullString.Len();
		if (StoryText)
		{
			StoryText->SetText(FText::FromString(FullString));
		}
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RevealTimerHandle);
	}
}

void UStoryWidgetBase::UpdateTextOnce()
{
	if (FullString.IsEmpty())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(RevealTimerHandle);
		}
		return;
	}

	RevealedCount = FMath::Clamp(RevealedCount + 1, 0, FullString.Len());
	if (StoryText && CursorType == EStoryCursorType::None)
	{
		StoryText->SetText(FText::FromString(FullString.Left(RevealedCount)));
	}

	if (RevealedCount >= FullString.Len())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(RevealTimerHandle);
		}

		if (CursorType != EStoryCursorType::None && StoryText)
		{
			StoryText->SetText(FText::FromString(FullString));
		}
	}
}
