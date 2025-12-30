#include "Widgets/GameTimerWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Controller/PlayerController/CameraControllerBase.h"

void UGameTimerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (GameTime)
	{
		float StartTime = 0.f;
		if (ACameraControllerBase* PC = Cast<ACameraControllerBase>(GetOwningPlayer()))
		{
			StartTime = PC->GameTimerStartTime;
		}

		float TotalGameTime = GetWorld()->GetTimeSeconds();
		float DisplayTime = FMath::Max(0.f, TotalGameTime - StartTime);

		int32 Minutes = FMath::FloorToInt(DisplayTime / 60.f);
		int32 Seconds = FMath::FloorToInt(FMath::Fmod(DisplayTime, 60.f));

		FString FormattedTime = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		GameTime->SetText(FText::FromString(FormattedTime));
	}
}
