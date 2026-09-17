// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Widgets/GameTimerWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Controller/PlayerController/CameraControllerBase.h"

#include "GameStates/ResourceGameState.h"

void UGameTimerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (KillCount)
	{
		// Das eigene Team steht am PlayerController, nicht am Widget - und erst recht nicht
		// im GameState, der die Verluste ja nur je Team fuehrt.
		int32 Abschuesse = 0;
		if (const AResourceGameState* GS = Cast<AResourceGameState>(GetWorld() ? GetWorld()->GetGameState() : nullptr))
		{
			int32 MeinTeam = 0;
			if (const ACameraControllerBase* PC = Cast<ACameraControllerBase>(GetOwningPlayer()))
			{
				MeinTeam = PC->SelectableTeamId;
			}
			Abschuesse = GS->GetKillsForTeam(MeinTeam);
		}

		if (Abschuesse != ZuletztAngezeigt)
		{
			ZuletztAngezeigt = Abschuesse;
			KillCount->SetText(FText::FromString(
				KillCountPrefix.ToString() + FString::FromInt(Abschuesse)));
		}
	}

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
