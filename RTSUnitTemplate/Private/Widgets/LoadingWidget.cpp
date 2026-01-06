#include "Widgets/LoadingWidget.h"
#include "Components/ProgressBar.h"
#include "Kismet/GameplayStatics.h"

void ULoadingWidget::SetupLoadingWidget(float InDuration)
{
	Duration = InDuration;
	ElapsedTime = 0.f;
}

void ULoadingWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (Duration > 0.f)
	{
		ElapsedTime += InDeltaTime;
		float Progress = FMath::Clamp(ElapsedTime / Duration, 0.f, 1.f);

		if (LoadingBar)
		{
			LoadingBar->SetPercent(Progress);
		}

		if (ElapsedTime >= Duration)
		{
			RemoveFromParent();
		}
	}
}
