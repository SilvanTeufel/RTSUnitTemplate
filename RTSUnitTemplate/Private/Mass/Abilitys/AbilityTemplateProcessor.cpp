// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/Abilitys/AbilityTemplateProcessor.h"

#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassActorSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Characters/Unit/AbilityUnit.h"
#include "System/AbilityTemplateSubsystem.h"
#include "Engine/World.h"

UAbilityTemplateProcessor::UAbilityTemplateProcessor()
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	// Hier werden GAS-Daten fremder Actor angefasst - das gehoert auf den GameThread.
	bRequiresGameThreadExecution = true;
}

void UAbilityTemplateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
	EntityQuery.RegisterWithProcessor(*this);
}

void UAbilityTemplateProcessor::Execute(FMassEntityManager& EntityManager,
                                        FMassExecutionContext& Context)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Langsam laufen lassen: die Vorlage aendert sich selten, und es ist nur zu pruefen,
	// ob eine Einheit inzwischen Punkte hat.
	TimeSinceLastRun += Context.GetDeltaTimeSeconds();
	if (TimeSinceLastRun < ExecutionInterval)
	{
		return;
	}
	TimeSinceLastRun -= ExecutionInterval;

	UAbilityTemplateSubsystem* Vorlagen = World->GetSubsystem<UAbilityTemplateSubsystem>();
	if (!Vorlagen)
	{
		return;
	}

	int32 Angewandt = 0;
	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const int32 NumEntities = ChunkContext.GetNumEntities();
		const auto ActorList = ChunkContext.GetFragmentView<FMassActorFragment>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			AAbilityUnit* Unit = Cast<AAbilityUnit>(
				const_cast<AActor*>(ActorList[i].Get()));
			if (!IsValid(Unit))
			{
				continue;
			}
			// Keine Punktpruefung mehr (02.09.2026): die Vorlage soll unabhaengig vom
			// Punktestand greifen, sonst bekaeme ein Teil der Klasse sie nie.

			TArray<FGameplayTag> Tags;
			Vorlagen->SammleTags(Unit->TeamId, Tags);
			for (const FGameplayTag& Tag : Tags)
			{
				// Beide Tag-Begriffe zulassen: die Slot-Knoepfe des Choosers merken sich den
				// TalentTag der Einheit, die Tier-Knoepfe einen Units.Tier.N aus UnitTags.
				// ALevelUnit::DoesAttributeTreeNodeMatchUnit prueft ebenfalls beides.
				if (!(Unit->TalentTag == Tag || Unit->UnitTags.HasTag(Tag)))
				{
					continue;
				}
				const FTierAbilityTemplate* Vorlage = Vorlagen->Hole(Unit->TeamId, Tag);
				if (!Vorlage)
				{
					continue;
				}

				for (int32 Slot = 0; Slot < 4; ++Slot)
				{
					const EGASAbilityInputID Gewuenscht = Vorlage->Hole(Slot);
					if (Gewuenscht == EGASAbilityInputID::None)
					{
						continue;
					}
					// Nur setzen, wenn der Slot noch frei ist - eine bewusste Abweichung
					// des Spielers an einer einzelnen Einheit soll nicht ueberschrieben werden.
					EGASAbilityInputID Aktuell = EGASAbilityInputID::None;
					switch (Slot)
					{
					case 0:  Aktuell = Unit->OffensiveAbilityID; break;
					case 1:  Aktuell = Unit->DefensiveAbilityID; break;
					case 2:  Aktuell = Unit->AttackAbilityID;    break;
					default: Aktuell = Unit->ThrowAbilityID;     break;
					}
					if (Aktuell != EGASAbilityInputID::None)
					{
						continue;
					}
					if (Unit->ApplyAbilityFromTemplate(Gewuenscht, Slot))
					{
						++Angewandt;
					}
				}
			}
		}
	});

	if (Angewandt > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Vorlage] %d Faehigkeiten automatisch nachgetragen."),
			Angewandt);
	}
}
