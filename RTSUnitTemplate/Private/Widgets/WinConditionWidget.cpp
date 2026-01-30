#include "Widgets/WinConditionWidget.h"
#include "Actors/WinLoseConfigActor.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "EngineUtils.h"

void UWinConditionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	for (TActorIterator<AWinLoseConfigActor> It(GetWorld()); It; ++It)
	{
		AWinLoseConfigActor* ConfigActor = *It;
		if (ConfigActor)
		{
			ConfigActor->OnWinConditionChanged.RemoveDynamic(this, &UWinConditionWidget::OnWinConditionChanged);
			ConfigActor->OnWinConditionChanged.AddDynamic(this, &UWinConditionWidget::OnWinConditionChanged);
			ConfigActor->OnYouWonTheGame.RemoveDynamic(this, &UWinConditionWidget::UpdateConditionText);
			ConfigActor->OnYouWonTheGame.AddDynamic(this, &UWinConditionWidget::UpdateConditionText);
			ConfigActor->OnYouLostTheGame.RemoveDynamic(this, &UWinConditionWidget::UpdateConditionText);
			ConfigActor->OnYouLostTheGame.AddDynamic(this, &UWinConditionWidget::UpdateConditionText);
		}
	}

	ACameraControllerBase* MyPC = Cast<ACameraControllerBase>(GetOwningPlayer());
	if (MyPC)
	{
		MyPC->OnTeamIdChanged.RemoveDynamic(this, &UWinConditionWidget::OnTeamIdChanged);
		MyPC->OnTeamIdChanged.AddDynamic(this, &UWinConditionWidget::OnTeamIdChanged);
	}

	UpdateConditionText();
}

void UWinConditionWidget::OnTeamIdChanged(int32 NewTeamId)
{
	UpdateConditionText();
}

void UWinConditionWidget::OnWinConditionChanged(AWinLoseConfigActor* Config, EWinLoseCondition NewCondition)
{
	UpdateConditionText();
}

void UWinConditionWidget::UpdateConditionText()
{
	if (!ConditionText) return;

	ACameraControllerBase* MyPC = Cast<ACameraControllerBase>(GetOwningPlayer());
	int32 MyTeamId = MyPC ? MyPC->SelectableTeamId : -1;

	AWinLoseConfigActor* BestConfig = nullptr;
	AWinLoseConfigActor* GlobalConfig = nullptr;

	for (TActorIterator<AWinLoseConfigActor> It(GetWorld()); It; ++It)
	{
		AWinLoseConfigActor* Config = *It;
		if (!Config) continue;

		if (Config->TeamId == MyTeamId)
		{
			BestConfig = Config;
			break;
		}

		if (Config->TeamId == 0)
		{
			GlobalConfig = Config;
		}
	}

	AWinLoseConfigActor* ConfigActor = BestConfig ? BestConfig : GlobalConfig;

	if (!ConfigActor)
	{
		if (MyTeamId == -1)
		{
			ConditionText->SetText(WaitingForTeamText);
		}
		else
		{
			ConditionText->SetText(NoConditionText);
		}
		return;
	}

	FText DescriptionBody;
	EWinLoseCondition CurrentCondition = ConfigActor->GetCurrentWinCondition();
	switch (CurrentCondition)
	{
	case EWinLoseCondition::None:
		DescriptionBody = NoneConditionText;
		break;
	case EWinLoseCondition::AllBuildingsDestroyed:
		DescriptionBody = AllBuildingsDestroyedText;
		break;
	case EWinLoseCondition::TaggedUnitDestroyed:
		{
			FString TagString = ConfigActor->WinLoseTargetTag.ToString();
			int32 LastDotIndex;
			if (TagString.FindLastChar('.', LastDotIndex))
			{
				TagString = TagString.RightChop(LastDotIndex + 1);
			}
			DescriptionBody = FText::Format(TaggedUnitDestroyedText, FText::FromString(TagString));
		}
		break;
	case EWinLoseCondition::TeamReachedResourceCount:
		DescriptionBody = FText::Format(FText::FromString(TEXT("{0}{1}")), ResourcesConditionText, FText::FromString(ConfigActor->TargetResourceCount.ToFormattedString()));
		break;
	case EWinLoseCondition::TeamReachedGameTime:
		DescriptionBody = FText::Format(SurvivalConditionText, FText::AsNumber(FMath::RoundToInt(ConfigActor->TargetGameTime)));
		break;
	default:
		DescriptionBody = UnknownConditionText;
		break;
	}

	FText FinalText = FText::Format(FText::FromString(TEXT("{0}{1}")), GoalPrefix, DescriptionBody);

	if (ConfigActor->WinConditions.Num() > 1)
	{
		FinalText = FText::Format(ProgressFormatText, FText::AsNumber(ConfigActor->CurrentWinConditionIndex + 1), FText::AsNumber(ConfigActor->WinConditions.Num()), FinalText);
	}

	ConditionText->SetText(FinalText);
}
