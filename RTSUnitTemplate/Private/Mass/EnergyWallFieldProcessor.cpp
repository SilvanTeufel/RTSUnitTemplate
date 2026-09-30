// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Mass/EnergyWallFieldProcessor.h"

#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassActorSubsystem.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Actors/EnergyWall.h"
#include "Characters/Unit/UnitBase.h"

namespace
{
	/** Eine Wand, so wie der Prozessor sie fuer diesen Durchlauf braucht. */
	struct FWallVolume
	{
		AEnergyWall* Wall = nullptr;
		FTransform   ToWorld;      // Weltmatrix der Box
		FTransform   ToLocal;      // deren Umkehrung, fuer die Punktprobe
		FVector      HalfSize = FVector::ZeroVector;
		int32        TeamId = 0;
	};
}

UEnergyWallFieldProcessor::UEnergyWallFieldProcessor()
{
	// NACH der Avoidance-Gruppe. Das ist der ganze Sinn: der Trennschub des
	// UnitSeparationProcessor steht dann schon in der Kraft, und dieser Prozessor kann den
	// Anteil, der in die Wand zeigt, wieder herausnehmen. Liefe er davor, wuerde der Trennschub
	// die Einheit anschliessend erneut hineindruecken.
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Avoidance;
	ExecutionOrder.ExecuteAfter.Add(TEXT("UnitSeparationProcessor"));
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
	bAutoRegisterWithProcessingPhases = true;
	// Liest Aktoren (Waende, ASC) - gehoert auf den Spielfaden.
	bRequiresGameThreadExecution = true;
}

void UEnergyWallFieldProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
	// ReadWrite, nicht ReadOnly: ein schreibgeschuetzter Blick liefert const AActor*, und der
	// Ability-Effekt braucht den Aktor nicht-const.
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
	EntityQuery.RegisterWithProcessor(*this);
}

void UEnergyWallFieldProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = Context.GetWorld();
	if (!World)
	{
		return;
	}

	TimeSinceLastRun += Context.GetDeltaTimeSeconds();
	if (ExecutionInterval > 0.f && TimeSinceLastRun < ExecutionInterval)
	{
		return;
	}
	const float StepTime = TimeSinceLastRun;
	TimeSinceLastRun = 0.f;

	// 1. Die aktuelle Geometrie aller stehenden Waende einsammeln.
	//
	// Bewusst jeden Durchlauf neu: RegisterObstacle vermisst die Box zur Laufzeit, sobald sich
	// die Tuerme bewegen oder die Wand waechst. Ein zwischengespeicherter Quader waere nach dem
	// ersten Umbau falsch, und zwar unsichtbar falsch.
	TArray<FWallVolume> Walls;
	for (TActorIterator<AEnergyWall> It(World); It; ++It)
	{
		AEnergyWall* Wall = *It;
		if (!IsValid(Wall) || !Wall->IsWallActive() || !Wall->NavObstacleBox)
		{
			continue;
		}

		FWallVolume Volume;
		Volume.Wall     = Wall;
		Volume.ToWorld  = Wall->NavObstacleBox->GetComponentTransform();
		Volume.ToLocal  = Volume.ToWorld.Inverse();
		Volume.HalfSize = Wall->NavObstacleBox->GetScaledBoxExtent();
		Volume.TeamId   = Wall->TeamId;
		Walls.Add(Volume);

		if (Debug)
		{
			DrawDebugBox(World, Volume.ToWorld.GetLocation(), Volume.HalfSize,
				Volume.ToWorld.GetRotation(), FColor::Cyan, false, ExecutionInterval * 2.f, 0, 3.f);
		}
	}

	if (Walls.Num() == 0)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	const float  ReapplyInterval = FMath::Max(0.f, EffectReapplyInterval);

	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const int32 Num = ChunkContext.GetNumEntities();
		const auto Stats  = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
		const auto Traits = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
		auto Actors = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
		auto Transforms   = ChunkContext.GetMutableFragmentView<FTransformFragment>();
		auto Forces       = ChunkContext.GetMutableFragmentView<FMassForceFragment>();

		for (int32 i = 0; i < Num; ++i)
		{
			FTransform UnitTransform = Transforms[i].GetTransform();
			const FVector UnitWorld  = UnitTransform.GetLocation();
			const float   Radius     = FMath::Max(1.f, Traits[i].CapsuleRadius);

			for (const FWallVolume& Volume : Walls)
			{
				// 2. Punktprobe im LOKALEN Raum des Quaders. Damit gilt dieselbe Rechnung fuer
				// schraege Waende wie fuer achsparallele - eine Weltachsen-Box waere bei einer
				// diagonal gespannten Wand deutlich zu gross.
				const FVector Local = Volume.ToLocal.TransformPosition(UnitWorld);

				// Der Radius zaehlt in X und Y mit: eine Einheit beruehrt die Wand schon, bevor
				// ihr Mittelpunkt drin steckt. In Z nicht - dort entscheidet die Hoehe der Wand.
				const FVector Grown(Volume.HalfSize.X + Radius, Volume.HalfSize.Y + Radius, Volume.HalfSize.Z);

				if (FMath::Abs(Local.X) >= Grown.X ||
					FMath::Abs(Local.Y) >= Grown.Y ||
					FMath::Abs(Local.Z) >= Grown.Z)
				{
					continue;
				}

				// 3. Gameplay-Effekt - das, was frueher OnOverlapBegin tun sollte und nie tat,
				// weil Einheiten keine Kollision haben.
				if (AActor* UnitActor = Actors[i].GetMutable())
				{
					if (AUnitBase* Unit = Cast<AUnitBase>(UnitActor))
					{
						const TPair<FMassEntityHandle, TWeakObjectPtr<AActor>> Key(
							ChunkContext.GetEntity(i), Volume.Wall);
						double& Last = LastEffectTime.FindOrAdd(Key, -1.0e9);

						if (Now - Last >= ReapplyInterval)
						{
							Last = Now;
							const TSubclassOf<UGameplayEffect> EffectToApply =
								(Unit->TeamId == Volume.TeamId)
									? Volume.Wall->FriendlyEffectClass
									: Volume.Wall->EnemyEffectClass;

							if (EffectToApply)
							{
								if (UAbilitySystemComponent* ASC = Unit->GetAbilitySystemComponent())
								{
									FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
									EffectContext.AddInstigator(Volume.Wall, Volume.Wall);
									FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectToApply, 1.0f, EffectContext);
									if (Spec.IsValid())
									{
										ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
									}
								}
							}
						}
					}
				}

				// 4. Hinausschieben. Die Seite mit der GERINGSTEN verbleibenden Durchdringung ist
				// die naechstgelegene - dorthin geht der kuerzeste Weg nach draussen. Nur X und Y
				// kommen in Frage: nach oben oder unten zu schieben hiesse, Einheiten in die Luft
				// oder in den Boden zu setzen.
				const float DepthX = Grown.X - FMath::Abs(Local.X);
				const float DepthY = Grown.Y - FMath::Abs(Local.Y);

				FVector LocalNormal = FVector::ZeroVector;
				float   Depth = 0.f;
				if (DepthX <= DepthY)
				{
					LocalNormal = FVector(Local.X >= 0.f ? 1.f : -1.f, 0.f, 0.f);
					Depth = DepthX;
				}
				else
				{
					LocalNormal = FVector(0.f, Local.Y >= 0.f ? 1.f : -1.f, 0.f);
					Depth = DepthY;
				}

				const FVector WorldNormal = Volume.ToWorld.TransformVectorNoScale(LocalNormal).GetSafeNormal();
				if (WorldNormal.IsNearlyZero())
				{
					continue;
				}

				// Harte Versetzung: erledigt den Fall, dass die Einheit bereits drin steckt.
				const FVector Corrected = UnitWorld + WorldNormal * (Depth + PushOutMargin);
				UnitTransform.SetLocation(FVector(Corrected.X, Corrected.Y, UnitWorld.Z));
				Transforms[i].GetMutableTransform() = UnitTransform;

				// Und die Kraft, die WEITER hineinzeigt, loeschen - sonst schiebt der Trennschub
				// im naechsten Bild erneut. Genau so wurden Einheiten bisher von ihren eigenen
				// Nachbarn durch die Wand gedrueckt.
				FVector& Force = Forces[i].Value;
				const float Inward = FVector::DotProduct(Force, -WorldNormal);
				if (Inward > 0.f)
				{
					Force += WorldNormal * Inward;
				}
				Force += WorldNormal * CounterForce * FMath::Max(StepTime, KINDA_SMALL_NUMBER);

				if (Debug)
				{
					DrawDebugLine(World, UnitWorld + FVector(0, 0, 20.f),
						UnitWorld + FVector(0, 0, 20.f) + WorldNormal * 150.f,
						FColor::Orange, false, ExecutionInterval * 2.f, 0, 3.f);
				}

				// Eine Wand je Durchlauf genuegt: die Einheit steht nach der Versetzung ohnehin
				// woanders, und zwei Waende gleichzeitig zu bedienen wuerde sie zwischen ihnen
				// hin und her werfen.
				break;
			}
		}
	});

	// Eintraege abgelaufener Einheiten und abgeraeumter Waende nicht ewig mitschleppen.
	if (LastEffectTime.Num() > 4096)
	{
		for (auto It = LastEffectTime.CreateIterator(); It; ++It)
		{
			if (!It.Key().Value.IsValid() || Now - It.Value() > 60.0)
			{
				It.RemoveCurrent();
			}
		}
	}
}
