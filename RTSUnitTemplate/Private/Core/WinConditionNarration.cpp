// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Core/WinConditionNarration.h"

#include "Actors/WinLoseConfigActor.h"
#include "Characters/Unit/UnitBase.h"
#include "EngineUtils.h"

namespace
{
	/** "BroodColossus" -> "Brood Colossus". */
	FString NarrSplitPascalCase(const FString& In)
	{
		FString Result;
		for (int32 i = 0; i < In.Len(); ++i)
		{
			if (i > 0 && FChar::IsUpper(In[i]) && !FChar::IsUpper(In[i - 1]) && In[i - 1] != ' ')
			{
				Result.AppendChar(' ');
			}
			Result.AppendChar(In[i] == '_' ? ' ' : In[i]);
		}
		return Result.TrimStartAndEnd();
	}

	/** The readable name of a tag: its last part, "Units.Xeno.BroodColossus" -> "Brood Colossus". */
	FString NarrTagName(const FGameplayTag& Tag)
	{
		// "Character.CameraUnit.0": the number names nothing - the part before it does.
		TArray<FString> Parts;
		Tag.GetTagName().ToString().ParseIntoArray(Parts, TEXT("."));
		while (Parts.Num() > 1 && Parts.Last().IsNumeric())
		{
			Parts.Pop();
		}
		return Parts.Num() > 0 ? NarrSplitPascalCase(Parts.Last()) : FString();
	}

	/**
	 * What the player sees for a tag: the Name of a unit in the world that carries it (as in the tooltips),
	 * else the tag. "Character.LoseCondition" or "Character.CameraUnit.0" say nothing aloud - the unit does.
	 */
	FString NarrUnitName(const AWinLoseConfigActor* Config, const FGameplayTag& Tag)
	{
		if (UWorld* World = Config ? Config->GetWorld() : nullptr)
		{
			for (TActorIterator<AUnitBase> It(World); It; ++It)
			{
				if (IsValid(*It) && It->UnitTags.HasTag(Tag) && !It->Name.IsEmpty() && It->Name != TEXT("Unit"))
				{
					return It->Name;
				}
			}
		}
		return NarrTagName(Tag);
	}

	/** "the Hive" - but "The Hive Sovereign" stays one "the". */
	FString NarrThe(const FString& Name)
	{
		return Name.StartsWith(TEXT("The "), ESearchCase::IgnoreCase) ? TEXT("the ") + Name.RightChop(4) : TEXT("the ") + Name;
	}

	/** Simple English plural of the last word: Stalker -> Stalkers, Colossus -> Colossuses, Ally -> Allies. */
	FString NarrPlural(const FString& Name)
	{
		const FString Lower = Name.ToLower();
		if (Lower.EndsWith(TEXT("s")) || Lower.EndsWith(TEXT("x")) || Lower.EndsWith(TEXT("z")) || Lower.EndsWith(TEXT("ch")) || Lower.EndsWith(TEXT("sh")))
		{
			return Name + TEXT("es");
		}
		if (Lower.EndsWith(TEXT("y")) && Lower.Len() > 1 && !FString(TEXT("aeiou")).Contains(Lower.Mid(Lower.Len() - 2, 1)))
		{
			return Name.LeftChop(1) + TEXT("ies");
		}
		return Name + TEXT("s");
	}

	/** "A", "A and B", "A, B and C". */
	FString NarrJoinList(const TArray<FString>& Items)
	{
		if (Items.Num() == 0) return FString();
		if (Items.Num() == 1) return Items[0];
		return FString::Join(TArrayView<const FString>(Items.GetData(), Items.Num() - 1), TEXT(", ")) + TEXT(" and ") + Items.Last();
	}

	FString NarrDuration(float Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::RoundToInt(Seconds));
		const int32 Minutes = Total / 60;
		const int32 Rest = Total % 60;
		auto Unit = [](int32 N, const TCHAR* One, const TCHAR* Many) { return FString::Printf(TEXT("%d %s"), N, N == 1 ? One : Many); };
		if (Minutes > 0 && Rest > 0) return Unit(Minutes, TEXT("minute"), TEXT("minutes")) + TEXT(" and ") + Unit(Rest, TEXT("second"), TEXT("seconds"));
		if (Minutes > 0) return Unit(Minutes, TEXT("minute"), TEXT("minutes"));
		return Unit(Rest, TEXT("second"), TEXT("seconds"));
	}

	/** Ends with a full stop (or ! / ?). */
	FString NarrSentence(FString S)
	{
		S.TrimStartAndEndInline();
		if (!S.IsEmpty() && !S.EndsWith(TEXT(".")) && !S.EndsWith(TEXT("!")) && !S.EndsWith(TEXT("?")))
		{
			S += TEXT(".");
		}
		return S;
	}

	/** How many of Tag there are to destroy / to build (the progress the server replicates, else the setting). */
	int32 NarrTagCount(const AWinLoseConfigActor* Config, const FGameplayTag& Tag, int32 Fallback)
	{
		for (const FTagProgress& Progress : Config->TagProgress)
		{
			if (Progress.Tag == Tag)
			{
				return FMath::Max(Progress.TargetCount, Progress.TotalCount);
			}
		}
		return Fallback;
	}
}

TArray<FString> WinConditionNarration::BuildSentences(const AWinLoseConfigActor* Config, bool bGameStart,
	EWinConditionNarrationStyle Style, const TMap<EResourceType, FText>* ResourceNames)
{
	TArray<FString> Out;
	if (!Config)
	{
		return Out;
	}
	// No (valid) stage: a plain match - won when every other team is out, so "destroy them" is the goal.
	const FWinConditionData Data = Config->GetCurrentWinConditionData();
	const bool bMatchGoal = Data.Condition == EWinLoseCondition::None && Data.CustomDescription.IsEmpty();
	if (bMatchGoal && Config->LoseCondition == EWinLoseCondition::None)
	{
		return Out; // Sandbox: nothing to win, nothing to lose.
	}
	// The same line in the three voices: plain orders, the Singularian collective, the Xeno brood.
	auto Say = [Style](const TCHAR* Plain, const TCHAR* Singularian, const TCHAR* Xeno) -> FString
	{
		return Style == EWinConditionNarrationStyle::Singularian ? Singularian : Style == EWinConditionNarrationStyle::Xeno ? Xeno : Plain;
	};
	const bool bVoiced = Style != EWinConditionNarrationStyle::Plain;
	const int32 Stages = Config->WinConditions.Num();
	const int32 Stage = FMath::Clamp(Config->CurrentWinConditionIndex, 0, FMath::Max(Stages - 1, 0)) + 1;
	// Counting phases aloud only helps while there are few; a survival map has dozens of time stages.
	const bool bCountStages = Stages > 1 && Stages <= 8;

	// Opening.
	if (bGameStart)
	{
		Out.Add(Say(TEXT("Listen closely. Here is your objective."), TEXT("Commander, our analysis of this operation is complete."),
			TEXT("Hear the hive, children of the brood.")));
		if (bCountStages)
		{
			Out.Add(FString::Format(*Say(TEXT("This mission has {0} stages."), TEXT("It will unfold in {0} phases. We begin with phase one."),
				TEXT("Our hunger comes in {0} waves. The first stirs now.")), { Stages }));
		}
	}
	else if (bCountStages)
	{
		Out.Add(FString::Format(*Say(TEXT("Well done. Stage {0} of {1} begins now."), TEXT("Phase complete. We proceed to phase {0} of {1}."),
			TEXT("The hive grows. Wave {0} of {1} begins.")), { Stage, Stages }));
	}
	else
	{
		Out.Add(Say(TEXT("Well done. Your objective has changed."), TEXT("Phase complete. Our directive has changed."),
			TEXT("The hive grows. A new hunger stirs.")));
	}

	// The objective. A designer's own description wins.
	if (!Data.CustomDescription.IsEmpty())
	{
		TArray<FString> Lines;
		Data.CustomDescription.ToString().ParseIntoArrayLines(Lines);
		for (const FString& Line : Lines)
		{
			Out.Add(NarrSentence(Line));
		}
	}
	else switch (bMatchGoal ? EWinLoseCondition::AllBuildingsDestroyed : Data.Condition)
	{
	case EWinLoseCondition::AllBuildingsDestroyed:
		Out.Add(Say(TEXT("Destroy every enemy building to win."), TEXT("Erase every enemy structure. None of them may remain."),
			TEXT("Devour every enemy structure. Leave nothing standing.")));
		break;

	case EWinLoseCondition::TaggedUnitsDestroyed:
	{
		TArray<FString> Targets;
		for (const FGameplayTag& Tag : Data.WinLoseTargetTags)
		{
			const FString Name = NarrUnitName(Config, Tag);
			const int32 Count = NarrTagCount(Config, Tag, 1);
			Targets.Add(Count > 1 ? FString::Printf(TEXT("all %d %s"), Count, *NarrPlural(Name)) : NarrThe(Name));
		}
		if (Targets.Num() == 0)
		{
			Out.Add(Say(TEXT("Eliminate the marked targets."), TEXT("Terminate the marked targets."), TEXT("Consume the marked prey.")));
		}
		else
		{
			Out.Add(NarrSentence(Say(TEXT("Eliminate "), TEXT("Terminate "), TEXT("Consume ")) + NarrJoinList(Targets)));
			if (bVoiced) Out.Add(Say(TEXT(""), TEXT("Their existence is an error in our calculations."), TEXT("Their flesh will feed the brood.")));
		}
		break;
	}

	case EWinLoseCondition::TeamReachedResourceCount:
	{
		TArray<FString> Parts;
		auto Add = [&](EResourceType Type, int32 Amount)
		{
			if (Amount <= 0) return;
			const FText* Name = ResourceNames ? ResourceNames->Find(Type) : nullptr;
			Parts.Add(FString::Printf(TEXT("%d %s"), Amount, *(Name && !Name->IsEmpty() ? Name->ToString() : UEnum::GetDisplayValueAsText(Type).ToString())));
		};
		const FBuildingCost& Cost = Data.TargetResourceCount;
		Add(EResourceType::Primary, Cost.PrimaryCost);
		Add(EResourceType::Secondary, Cost.SecondaryCost);
		Add(EResourceType::Tertiary, Cost.TertiaryCost);
		Add(EResourceType::Rare, Cost.RareCost);
		Add(EResourceType::Epic, Cost.EpicCost);
		Add(EResourceType::Legendary, Cost.LegendaryCost);
		if (Parts.Num() > 0)
		{
			Out.Add(NarrSentence(Say(TEXT("Gather "), TEXT("Accumulate "), TEXT("Gorge on ")) + NarrJoinList(Parts)));
			Out.Add(Say(TEXT("Build up your economy to get there."), TEXT("The collective requires these resources."), TEXT("The hive must feed.")));
		}
		break;
	}

	case EWinLoseCondition::TeamReachedGameTime:
	{
		// Stage times are absolute match times: after a time stage, only the rest is left to hold.
		float Seconds = Data.TargetGameTime;
		const int32 Index = Stage - 1;
		if (Config->WinConditions.IsValidIndex(Index - 1) && Config->WinConditions[Index - 1].Condition == EWinLoseCondition::TeamReachedGameTime)
		{
			Seconds -= Config->WinConditions[Index - 1].TargetGameTime;
		}
		const bool bMore = Index > 0 && Seconds < Data.TargetGameTime;
		Out.Add(NarrSentence((bMore ? Say(TEXT("Hold out for another "), TEXT("Endure for another "), TEXT("Survive another "))
			: Say(TEXT("Hold out for "), TEXT("Endure for "), TEXT("Survive for "))) + NarrDuration(Seconds)));
		Out.Add(Say(TEXT("Keep your base alive until then."), TEXT("Survival is the only acceptable outcome."), TEXT("The brood must not perish.")));
		break;
	}

	case EWinLoseCondition::TaggedUnitsSpawned:
	{
		TArray<FString> Parts;
		for (const FGameplayTagCount& Entry : Data.TargetTagCounts)
		{
			const FString Name = NarrTagName(Entry.Tag);
			Parts.Add(Entry.Count > 1 ? FString::Printf(TEXT("%d %s"), Entry.Count, *NarrPlural(Name)) : FString::Printf(TEXT("one %s"), *Name));
		}
		if (Parts.Num() > 0)
		{
			Out.Add(NarrSentence(Say(TEXT("Build "), TEXT("Construct "), TEXT("Grow ")) + NarrJoinList(Parts)));
			if (bVoiced) Out.Add(Say(TEXT(""), TEXT("Our strength must grow."), TEXT("The swarm must swell.")));
		}
		break;
	}

	case EWinLoseCondition::TeamReachedLocation:
		Out.Add(Say(TEXT("Move one of your units to the marked location."), TEXT("Transfer one of our units to the marked coordinates."),
			TEXT("Send one of the brood to the marked place.")));
		break;

	default:
		break;
	}

	// How the operation can fail - said once, at the start.
	if (bGameStart)
	{
		switch (Config->LoseCondition)
		{
		case EWinLoseCondition::AllBuildingsDestroyed:
			Out.Add(Say(TEXT("If you lose all of your buildings, the mission is lost."), TEXT("Should all of our structures fall, the operation fails."),
				TEXT("If every nest falls, the brood dies.")));
			break;

		case EWinLoseCondition::TaggedUnitsDestroyed:
		{
			TArray<FString> Guarded;
			for (const FGameplayTag& Tag : Config->WinLoseTargetTags)
			{
				Guarded.Add(NarrThe(NarrUnitName(Config, Tag)));
			}
			if (Guarded.Num() > 0)
			{
				Out.Add(NarrSentence(Say(TEXT("Protect "), TEXT("Preserve "), TEXT("Shield ")) + NarrJoinList(Guarded)
					+ Say(TEXT(". If it falls, the mission is lost"), TEXT(" at all costs. Its loss would end us"), TEXT(". If it dies, the hive dies with it"))));
			}
			break;
		}

		case EWinLoseCondition::TeamReachedGameTime:
			if (Config->TargetGameTime > 0.f)
			{
				Out.Add(NarrSentence(Say(TEXT("You have "), TEXT("Our window is "), TEXT("The hive has ")) + NarrDuration(Config->TargetGameTime)
					+ Say(TEXT(" before the mission fails"), TEXT(". Then it closes forever"), TEXT(" before it starves"))));
			}
			break;

		default:
			break;
		}
	}
	return Out;
}

TArray<FString> WinConditionNarration::ToMessages(const TArray<FString>& Sentences, int32 MaxChars)
{
	TArray<FString> Messages;
	FString Current;
	for (const FString& S : Sentences)
	{
		if (!Current.IsEmpty() && Current.Len() + 1 + S.Len() > MaxChars)
		{
			Messages.Add(Current);
			Current.Reset();
		}
		Current = Current.IsEmpty() ? S : Current + TEXT(" ") + S;
	}
	if (!Current.IsEmpty())
	{
		Messages.Add(Current);
	}
	return Messages;
}
