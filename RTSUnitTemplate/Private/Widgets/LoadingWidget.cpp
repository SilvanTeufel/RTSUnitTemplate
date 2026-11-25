// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Widgets/LoadingWidget.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Blueprint/WidgetTree.h"
#include "Framework/Application/SlateApplication.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Kismet/GameplayStatics.h"

#include "GameFramework/GameStateBase.h"

void ULoadingWidget::SetupLoadingWidget(float InTotalDuration, float InServerWorldTimeStart)
{
	TotalDuration = InTotalDuration;
	ServerWorldTimeStart = InServerWorldTimeStart;
}

void ULoadingWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (bKeepButtonsResponsive && WidgetTree)
	{
		WidgetTree->ForEachWidget([](UWidget* Widget)
		{
			if (UButton* Button = Cast<UButton>(Widget))
			{
				Button->SetClickMethod(EButtonClickMethod::MouseDown);
				Button->SetTouchMethod(EButtonTouchMethod::DownAndUp);
			}
		});
		if (APlayerController* PC = GetOwningPlayer())
		{
			PC->SetShowMouseCursor(true);
		}
	}
	if (!bStyleLoadingBar || !LoadingBar)
	{
		return;
	}
	// Dark glass with a thin frame in the HUD's cyan, the fill a glowing strip inset by the frame.
	FProgressBarStyle Style = LoadingBar->GetWidgetStyle();
	Style.SetBackgroundImage(FSlateRoundedBoxBrush(BarBackgroundColor, 3.f, BarFrameColor, 1.5f));
	FSlateRoundedBoxBrush Fill(BarFillColor, 2.f, FLinearColor(0.75f, 0.97f, 1.f, 0.9f), 1.f);
	Fill.Margin = FMargin(0.f);
	Style.SetFillImage(Fill);
	Style.SetMarqueeImage(Fill);
	LoadingBar->SetWidgetStyle(Style);
	LoadingBar->SetFillColorAndOpacity(FLinearColor::White);
	LoadingBar->SetBorderPadding(FVector2D(3.f, 3.f));
}

void ULoadingWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// A capture held by the game viewport swallows every click on the buttons. With no mouse button
	// down nothing needs it any more, so hand it back (checked a few times per second).
	CaptureCheckTime += InDeltaTime;
	if (bKeepButtonsResponsive && CaptureCheckTime >= 0.2f && FSlateApplication::IsInitialized())
	{
		CaptureCheckTime = 0.f;
		FSlateApplication& Slate = FSlateApplication::Get();
		if (Slate.GetPressedMouseButtons().Num() == 0 && Slate.HasAnyMouseCaptor())
		{
			Slate.ReleaseAllPointerCapture();
		}
	}

	if (TotalDuration > 0.f)
	{
		float CurrentServerTime = 0.f;
		if (GetWorld() && GetWorld()->GetGameState())
		{
			CurrentServerTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
		}

		float Elapsed = CurrentServerTime - ServerWorldTimeStart;
		float Progress = FMath::Clamp(Elapsed / TotalDuration, 0.f, 1.f);

		if (LoadingBar)
		{
			LoadingBar->SetPercent(Progress);
		}

		if (Elapsed >= TotalDuration)
		{
			RemoveFromParent();
		}
	}
}
