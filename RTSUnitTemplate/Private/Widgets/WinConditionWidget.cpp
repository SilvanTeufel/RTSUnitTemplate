#include "Widgets/WinConditionWidget.h"
#include "Actors/WinLoseConfigActor.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "EngineUtils.h"
#include "Components/RichTextBlock.h"
#include "Components/Image.h"

static FString SplitPascalCase(const FString& InString)
{
	FString Result;
	for (int32 i = 0; i < InString.Len(); ++i)
	{
		if (i > 0 && FChar::IsUpper(InString[i]) && !FChar::IsUpper(InString[i - 1]))
		{
			Result.AppendChar(' ');
		}
		Result.AppendChar(InString[i]);
	}
	return Result;
}

void UWinConditionWidget::UpdateResourceWidgets(const FBuildingCost& Cost, bool bVisible)
{
	auto UpdateWidgetPair = [&](UImage* Icon, UTextBlock* AmountText, UTextBlock* NameText, int32 Amount, EResourceType Type)
	{
		bool bShouldShow = bVisible && Amount > 0;
		
		if (Icon)
		{
			if (bShouldShow)
			{
				UTexture2D* const* FoundIcon = ResourceIcons.Find(Type);
				if (FoundIcon && *FoundIcon)
				{
					Icon->SetBrushFromTexture(*FoundIcon);
					Icon->SetVisibility(ESlateVisibility::Visible);
				}
				else
				{
					Icon->SetVisibility(ESlateVisibility::Collapsed);
				}
			}
			else
			{
				Icon->SetVisibility(ESlateVisibility::Collapsed);
			}
		}

		if (AmountText)
		{
			if (bShouldShow)
			{
				AmountText->SetText(FText::AsNumber(Amount));
				AmountText->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				AmountText->SetVisibility(ESlateVisibility::Collapsed);
			}
		}

		if (NameText)
		{
			bool bShowName = bShouldShow && !bShowOnlyIconsAndCost;
			if (bShowName)
			{
				FText Name = ResourceDisplayNames.Contains(Type) ? ResourceDisplayNames[Type] : UEnum::GetDisplayValueAsText(Type);
				NameText->SetText(Name);
				NameText->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				NameText->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	};

	UpdateWidgetPair(ResourceIconPrimary, ResourceAmountPrimary, ResourceNamePrimary, Cost.PrimaryCost, EResourceType::Primary);
	UpdateWidgetPair(ResourceIconSecondary, ResourceAmountSecondary, ResourceNameSecondary, Cost.SecondaryCost, EResourceType::Secondary);
	UpdateWidgetPair(ResourceIconTertiary, ResourceAmountTertiary, ResourceNameTertiary, Cost.TertiaryCost, EResourceType::Tertiary);
	UpdateWidgetPair(ResourceIconRare, ResourceAmountRare, ResourceNameRare, Cost.RareCost, EResourceType::Rare);
	UpdateWidgetPair(ResourceIconEpic, ResourceAmountEpic, ResourceNameEpic, Cost.EpicCost, EResourceType::Epic);
	UpdateWidgetPair(ResourceIconLegendary, ResourceAmountLegendary, ResourceNameLegendary, Cost.LegendaryCost, EResourceType::Legendary);
}

FText UWinConditionWidget::FormatResourceCost(const FBuildingCost& Cost, bool bIncludeTags)
{
	TArray<FText> Parts;
	auto AddPart = [&](EResourceType Type, int32 Amount)
	{
		if (Amount <= 0) return;

		FText Name = ResourceDisplayNames.Contains(Type) ? ResourceDisplayNames[Type] : UEnum::GetDisplayValueAsText(Type);

		FString IconTag = TEXT("");
		if (bIncludeTags)
		{
			FString TypeName = StaticEnum<EResourceType>()->GetNameStringByValue((int64)Type);
			// Strip potential enum prefix (e.g. EResourceType::Primary -> Primary)
			int32 LastColon;
			if (TypeName.FindLastChar(':', LastColon))
			{
				TypeName = TypeName.RightChop(LastColon + 1);
			}
			
			// Tag format for RichTextBlock: <img id="EnumName"/>
			IconTag = FString::Printf(TEXT("<img id=\"%s\"/>"), *TypeName);
		}

		if (bShowOnlyIconsAndCost)
		{
			if (bIncludeTags)
			{
				Parts.Add(FText::Format(FText::FromString(TEXT("{0} {1}")), FText::FromString(IconTag), FText::AsNumber(Amount)));
			}
			else
			{
				Parts.Add(FText::AsNumber(Amount));
			}
		}
		else
		{
			if (bIncludeTags)
			{
				// Icon Name Amount
				Parts.Add(FText::Format(FText::FromString(TEXT("{0} {1} {2}")), FText::FromString(IconTag), Name, FText::AsNumber(Amount)));
			}
			else
			{
				Parts.Add(FText::Format(FText::FromString(TEXT("{0}: {1}")), Name, FText::AsNumber(Amount)));
			}
		}
	};

	AddPart(EResourceType::Primary, Cost.PrimaryCost);
	AddPart(EResourceType::Secondary, Cost.SecondaryCost);
	AddPart(EResourceType::Tertiary, Cost.TertiaryCost);
	AddPart(EResourceType::Rare, Cost.RareCost);
	AddPart(EResourceType::Epic, Cost.EpicCost);
	AddPart(EResourceType::Legendary, Cost.LegendaryCost);

	return FText::Join(FText::FromString(TEXT("\n")), Parts);
}

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
			ConfigActor->OnTagProgressUpdated.RemoveDynamic(this, &UWinConditionWidget::OnTagProgressUpdated);
			ConfigActor->OnTagProgressUpdated.AddDynamic(this, &UWinConditionWidget::OnTagProgressUpdated);
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

	StartUpdateTimer();
	UpdateConditionText();
}

void UWinConditionWidget::StartUpdateTimer()
{
	if (!GetWorld()) return;
	GetWorld()->GetTimerManager().SetTimer(UpdateTimerHandle, this, &UWinConditionWidget::UpdateConditionText, UpdateInterval, true);
}

void UWinConditionWidget::StopTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(UpdateTimerHandle);
	}
}

void UWinConditionWidget::OnTeamIdChanged(int32 NewTeamId)
{
	UpdateConditionText();
}

void UWinConditionWidget::OnWinConditionChanged(AWinLoseConfigActor* Config, EWinLoseCondition NewCondition)
{
	UpdateConditionText();
}

void UWinConditionWidget::OnTagProgressUpdated(AWinLoseConfigActor* Config)
{
	UpdateConditionText();
}

void UWinConditionWidget::UpdateConditionText()
{
	if (!ConditionText && !RichConditionText) return;

	ACameraControllerBase* MyPC = Cast<ACameraControllerBase>(GetOwningPlayer());
	int32 MyTeamId = MyPC ? MyPC->SelectableTeamId : -1;

	AWinLoseConfigActor* ConfigActor = AWinLoseConfigActor::GetWinLoseConfigForTeam(this, MyTeamId);

	// Reset resource widgets by default
	UpdateResourceWidgets(FBuildingCost(), false);

	if (!ConfigActor)
	{
		FText StatusText = (MyTeamId == -1) ? WaitingForTeamText : NoConditionText;
		
		if (ConditionText)
		{
			ConditionText->SetText(StatusText);
		}
		
		if (RichConditionText)
		{
			RichConditionText->SetText(StatusText);
		}
		return;
	}

	FText DescriptionBody;
	FWinConditionData WinData = ConfigActor->GetCurrentWinConditionData();
	EWinLoseCondition CurrentCondition = WinData.Condition;
	switch (CurrentCondition)
	{
	case EWinLoseCondition::None:
		DescriptionBody = NoneConditionText;
		break;
	case EWinLoseCondition::AllBuildingsDestroyed:
		DescriptionBody = AllBuildingsDestroyedText;
		break;
	case EWinLoseCondition::TaggedUnitsDestroyed:
		{
			TArray<FText> TagTexts;
			for (const FTagProgress& Progress : ConfigActor->TagProgress)
			{
				FString TagString = Progress.Tag.ToString();
				int32 LastDotIndex;
				if (TagString.FindLastChar('.', LastDotIndex))
				{
					TagString = TagString.RightChop(LastDotIndex + 1);
				}
				TagString = SplitPascalCase(TagString);

				int32 DestroyedCount = Progress.TotalCount - Progress.AliveCount;
				TagTexts.Add(FText::Format(FText::FromString(TEXT("{0}/{1} {2}")), FText::AsNumber(DestroyedCount), FText::AsNumber(Progress.TotalCount), FText::FromString(TagString)));
			}
			FText CombinedTags = FText::Join(FText::FromString(TEXT("\n")), TagTexts);
			DescriptionBody = FText::Format(TaggedUnitsDestroyedText, CombinedTags);
		}
		break;
	case EWinLoseCondition::TaggedUnitsSpawned:
		{
			TArray<FText> TagTexts;
			for (const FTagProgress& Progress : ConfigActor->TagProgress)
			{
				FString TagString = Progress.Tag.ToString();
				int32 LastDotIndex;
				if (TagString.FindLastChar('.', LastDotIndex))
				{
					TagString = TagString.RightChop(LastDotIndex + 1);
				}
				TagString = SplitPascalCase(TagString);

				TagTexts.Add(FText::Format(TaggedUnitsSpawnedFormat, FText::AsNumber(Progress.TotalCount), FText::AsNumber(Progress.TargetCount), FText::FromString(TagString)));
			}
			FText CombinedTags = FText::Join(FText::FromString(TEXT("\n")), TagTexts);
			DescriptionBody = FText::Format(TaggedUnitsSpawnedText, CombinedTags);
		}
		break;
	case EWinLoseCondition::TeamReachedResourceCount:
		{
			bool bHasIndividualWidgets = ResourceAmountPrimary || ResourceAmountSecondary || ResourceAmountTertiary || 
										ResourceAmountRare || ResourceAmountEpic || ResourceAmountLegendary;

			if (bHasIndividualWidgets)
			{
				DescriptionBody = ResourcesConditionText;
			}
			else
			{
				DescriptionBody = FText::Format(FText::FromString(TEXT("{0}{1}")), ResourcesConditionText, FormatResourceCost(WinData.TargetResourceCount, RichConditionText != nullptr));
			}
			UpdateResourceWidgets(WinData.TargetResourceCount, true);
		}
		break;
	case EWinLoseCondition::TeamReachedGameTime:
		DescriptionBody = FText::Format(SurvivalConditionText, FText::AsNumber(FMath::RoundToInt(WinData.TargetGameTime)));
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

	if (RichConditionText)
	{
		RichConditionText->SetText(FinalText);
		RichConditionText->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (ConditionText)
		{
			ConditionText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else if (ConditionText)
	{
		ConditionText->SetText(FinalText);
		ConditionText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}
