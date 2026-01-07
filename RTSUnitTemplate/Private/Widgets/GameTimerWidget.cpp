#include "Widgets/GameTimerWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Controller/PlayerController/CameraControllerBase.h"

#include "GameStates/ResourceGameState.h"

void UGameTimerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (GameTime)
	{
		float StartTime = 0.f;
		float TotalGameTime = 0.f;

		if (GetWorld() && GetWorld()->GetGameState())
		{
			TotalGameTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
			if (AResourceGameState* GS = Cast<AResourceGameState>(GetWorld()->GetGameState()))
			{
				StartTime = GS->MatchStartTime;
			}
		}

		if (StartTime < 0.f)
		{
			GameTime->SetText(FText::FromString(TEXT("00:00")));
			return;
		}

		float DisplayTime = FMath::Max(0.f, TotalGameTime - StartTime);

		int32 Minutes = FMath::FloorToInt(DisplayTime / 60.f);
		int32 Seconds = FMath::FloorToInt(FMath::Fmod(DisplayTime, 60.f));

		FString FormattedTime = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		GameTime->SetText(FText::FromString(FormattedTime));
	}
}
