#include "Widgets/LoadingWidget.h"
#include "Components/ProgressBar.h"
#include "Kismet/GameplayStatics.h"

#include "GameFramework/GameStateBase.h"

void ULoadingWidget::SetupLoadingWidget(float InTotalDuration, float InServerWorldTimeStart)
{
	TotalDuration = InTotalDuration;
	ServerWorldTimeStart = InServerWorldTimeStart;
}

void ULoadingWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

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
