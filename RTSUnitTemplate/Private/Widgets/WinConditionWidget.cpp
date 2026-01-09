#include "Widgets/WinConditionWidget.h"
#include "Actors/WinLoseConfigActor.h"
#include "EngineUtils.h"

void UWinConditionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UpdateConditionText();
}

void UWinConditionWidget::UpdateConditionText()
{
	if (!ConditionText) return;

	AWinLoseConfigActor* ConfigActor = nullptr;
	for (TActorIterator<AWinLoseConfigActor> It(GetWorld()); It; ++It)
	{
		ConfigActor = *It;
		break;
	}

	if (!ConfigActor)
	{
		ConditionText->SetText(FText::FromString(TEXT("No win condition defined for this map.")));
		return;
	}

	FString Description = TEXT("Goal: ");
	switch (ConfigActor->WinLoseCondition)
	{
	case EWinLoseCondition::None:
		Description += TEXT("None (Sandbox)");
		break;
	case EWinLoseCondition::AllBuildingsDestroyed:
		Description += TEXT("Destroy all enemy buildings to win. Protect your own!");
		break;
	case EWinLoseCondition::TaggedUnitDestroyed:
		Description += FString::Printf(TEXT("Destroy the unit with tag: %s"), *ConfigActor->WinLoseTargetTag.ToString());
		break;
	case EWinLoseCondition::TeamReachedResourceCount:
		Description += TEXT("Gather the required amount of resources:\n");
		Description += ConfigActor->TargetResourceCount.ToFormattedString();
		break;
	case EWinLoseCondition::TeamReachedGameTime:
		Description += FString::Printf(TEXT("Survive for %.0f seconds."), ConfigActor->TargetGameTime);
		break;
	default:
		Description += TEXT("Unknown");
		break;
	}

	ConditionText->SetText(FText::FromString(Description));
}
