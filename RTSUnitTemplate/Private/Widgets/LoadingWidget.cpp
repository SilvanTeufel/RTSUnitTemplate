#include "Widgets/LoadingWidget.h"
#include "Components/ProgressBar.h"
#include "Kismet/GameplayStatics.h"

void ULoadingWidget::SetupLoadingWidget(float InTargetTime)
{
	TargetTime = InTargetTime;
}

void ULoadingWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (TargetTime > 0.f)
	{
		float CurrentTime = GetWorld()->GetTimeSeconds();
		float Progress = FMath::Clamp(CurrentTime / TargetTime, 0.f, 1.f);

		if (LoadingBar)
		{
			LoadingBar->SetPercent(Progress);
		}

		if (CurrentTime >= TargetTime)
		{
			RemoveFromParent();
		}
	}
}
