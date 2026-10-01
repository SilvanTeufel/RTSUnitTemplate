// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Mass/EnergyWallFieldProcessor.h"

#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassActorSubsystem.h"
#include "Steering/MassSteeringFragments.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Actors/EnergyWall.h"
#include "Characters/Unit/BuildingBase.h"
#include "Characters/Unit/UnitBase.h"

namespace
{
	/** Eine Wand, so wie der Prozessor sie fuer diesen Durchlauf braucht. */
	struct FWallVolume
	{
		AEnergyWall* Wall = nullptr;
		FTransform   ToWorld;      // Weltmatrix der Box
		FTransform   ToLocal;      // deren Umkehrung, nur noch fuer die Hoehenprobe
		FVector      HalfSize = FVector::ZeroVector;
		int32        TeamId = 0;

		// Die Wand als Strecke in XY (01.10.2026). Endpunkte sind die Turmmittelpunkte, NICHT
		// die Enden der NavObstacleBox - die ragt um MinPadding+AgentRadiusPadding (200 uu) ueber
		// jeden Turm hinaus, und genau in diese unsichtbare "Klinge" hat der Trennschub die
		// Angreifer eines Turms hineingedrueckt.
		FVector2D    A = FVector2D::ZeroVector;
		FVector2D    Dir = FVector2D(1.f, 0.f);   // Einheitsvektor A -> B
		float        Length = 0.f;
		float        HalfThickness = 0.f;
		FVector2D    BoundsMin = FVector2D::ZeroVector;   // Grobtest: Huelle der Strecke
		FVector2D    BoundsMax = FVector2D::ZeroVector;
	};
}

UEnergyWallFieldProcessor::UEnergyWallFieldProcessor()
{
	// NACH ALLEN DREI Kraftquellen der Avoidance-Gruppe (01.10.2026). Das ist der ganze Sinn:
	// Trennschub (UnitSeparationProcessor), weiches Ausweichen (UnitSoftAvoidanceProcessor) und
	// das bewegte Ausweichen (UnitMovingAvoidanceProcessor) stehen dann schon in der Kraft, und
	// dieser Prozessor kann den Anteil, der in die Wand zeigt, wieder herausnehmen.
	// Bisher stand hier nur der Trennschub. UnitMovingAvoidanceProcessor SETZT die Kraft aber
	// (Force.Value = ..., nicht +=) - lief er zufaellig danach, war die Korrektur dieses
	// Prozessors schlicht ueberschrieben, und welche Reihenfolge herauskam, entschied der
	// Abhaengigkeitsloeser, nicht der Code.
	// Zyklusfrei: keiner der drei nennt diesen Prozessor in ExecuteBefore/After, und die
	// Bewegungsanwendung (UnitApplyMassMovementProcessor, Gruppe Movement) laeuft ohnehin nach
	// der ganzen Avoidance-Gruppe - sie sieht also die korrigierte Kraft und Geschwindigkeit.
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Avoidance;
	ExecutionOrder.ExecuteAfter.Add(TEXT("UnitSeparationProcessor"));
	ExecutionOrder.ExecuteAfter.Add(TEXT("UnitSoftAvoidanceProcessor"));
	ExecutionOrder.ExecuteAfter.Add(TEXT("UnitMovingAvoidanceProcessor"));
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
	bAutoRegisterWithProcessingPhases = true;
	// Liest Aktoren (Waende, Tuerme, ASC) - gehoert auf den Spielfaden. Fremde Entitaeten
	// werden NICHT angefasst, nur die Fragmente des eigenen Chunks.
	bRequiresGameThreadExecution = true;
}

void UEnergyWallFieldProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	// Optional, nicht Pflicht (01.10.2026): eine Pflichtanforderung wuerde jede Einheit ohne
	// dieses Fragment stillschweigend aus der Abfrage werfen - und damit aus der Wand-Sperre.
	// Fehlt es, bleibt fuer diese Einheit nur die Kraft- und Lagekorrektur.
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassSteeringFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
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
	TimeSinceLastRun = 0.f;

	// Dieselbe Begrenzung wie in UnitApplyMassMovementProcessor (Min(0.1, dt)) - die Feder
	// unten wird gegen genau den Zeitschritt stabilisiert, mit dem die Kraft danach integriert wird.
	const float FrameDelta = FMath::Clamp(Context.GetDeltaTimeSeconds(), 0.001f, 0.1f);

	// Debuglinien leben ein Bild (-1) bei Bildtakt, sonst zwei Takte.
	const float DebugLifeTime = ExecutionInterval > 0.f ? ExecutionInterval * 2.f : -1.f;

	// 1. Die aktuelle Geometrie aller stehenden Waende einsammeln.
	//
	// Bewusst jeden Durchlauf neu: RegisterObstacle vermisst die Box zur Laufzeit, sobald sich
	// die Tuerme bewegen oder die Wand waechst. Ein zwischengespeicherter Quader waere nach dem
	// ersten Umbau falsch, und zwar unsichtbar falsch.
	TArray<FWallVolume, TInlineAllocator<16>> Walls;
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
		// Die echte halbe Wanddicke - das, was RegisterObstacle als Box-X-Halbmass setzt
		// (Lerp(MinThickness, MaxThickness) * 0.5, also 2.5-5 uu). Das Polster fuer das
		// Navigationsnetz steckt nur in Y und spielt hier keine Rolle mehr.
		Volume.HalfThickness = FMath::Max(0.f, static_cast<float>(Volume.HalfSize.X)) + ExtraHalfThickness;

		// Endpunkte: die beiden Turmmittelpunkte. Fehlt ein Turm (abgerissen, noch nicht
		// repliziert), wird die Strecke aus der Box abgeleitet - laenger als die echte Wand,
		// aber stetig und ohne Absturz. Eine Wand ohne Tuerme loest sich ohnehin gleich auf.
		const ABuildingBase* BuildingA = Wall->GetBuildingA();
		const ABuildingBase* BuildingB = Wall->GetBuildingB();
		FVector2D EndA, EndB;
		if (IsValid(BuildingA) && IsValid(BuildingB))
		{
			const FVector LocA = BuildingA->GetActorLocation();
			const FVector LocB = BuildingB->GetActorLocation();
			EndA = FVector2D(LocA.X, LocA.Y);
			EndB = FVector2D(LocB.X, LocB.Y);
		}
		else
		{
			const FVector Center = Volume.ToWorld.GetLocation();
			const FVector AxisY  = Volume.ToWorld.GetUnitAxis(EAxis::Y) * Volume.HalfSize.Y;
			EndA = FVector2D(Center.X - AxisY.X, Center.Y - AxisY.Y);
			EndB = FVector2D(Center.X + AxisY.X, Center.Y + AxisY.Y);
		}

		const FVector2D AB = EndB - EndA;
		Volume.A      = EndA;
		Volume.Length = static_cast<float>(AB.Size());
		Volume.Dir    = Volume.Length > KINDA_SMALL_NUMBER ? AB / Volume.Length : FVector2D(1.f, 0.f);
		Volume.BoundsMin = FVector2D(FMath::Min(EndA.X, EndB.X), FMath::Min(EndA.Y, EndB.Y));
		Volume.BoundsMax = FVector2D(FMath::Max(EndA.X, EndB.X), FMath::Max(EndA.Y, EndB.Y));
		Walls.Add(Volume);

		if (Debug)
		{
			const float Z = Volume.ToWorld.GetLocation().Z;
			DrawDebugLine(World, FVector(EndA, Z), FVector(EndB, Z), FColor::Cyan, false, DebugLifeTime, 0, 3.f);
		}
	}

	if (Walls.Num() == 0)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	const float  ReapplyInterval = FMath::Max(0.f, EffectReapplyInterval);
	const float  Band = FMath::Max(0.f, ContactBand);
	const float  Slop = FMath::Max(0.f, HardCorrectionSlop);

	// Stabilitaetsgrenzen der Feder. UnitApplyMassMovementProcessor integriert
	// v += Kraft * dt * 4 und danach p += v * dt. Eine Federkraft k*pen verschiebt damit in einem
	// Schritt um k*pen*4*dt^2. Ist das groesser als pen, schiesst die Einheit ueber die Wand
	// hinaus und kommt im naechsten Bild zurueck - genau das Saegezahnzittern, das hier weg soll.
	// Deshalb hoechstens die HALBE Durchdringung je Schritt, auch bei schlechter Bildrate.
	const float StepFactor   = 4.f * FrameDelta;
	const float MaxStiffness = 0.5f / (StepFactor * FrameDelta);
	const float MaxDamping   = 0.5f / StepFactor;
	const float Stiffness    = FMath::Clamp(SpringStiffness, 0.f, MaxStiffness);
	const float Damping      = FMath::Clamp(SpringDamping, 0.f, MaxDamping);

	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const int32 Num = ChunkContext.GetNumEntities();
		const auto Stats  = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
		const auto Traits = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
		auto Actors = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
		auto Transforms   = ChunkContext.GetMutableFragmentView<FTransformFragment>();
		auto Forces       = ChunkContext.GetMutableFragmentView<FMassForceFragment>();
		// Optional - leer, wenn das Archetyp das Fragment nicht hat.
		auto Velocities   = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
		auto Steerings    = ChunkContext.GetMutableFragmentView<FMassSteeringFragment>();
		const bool bHasVelocity = Velocities.Num() > 0;
		const bool bHasSteering = Steerings.Num() > 0;

		for (int32 i = 0; i < Num; ++i)
		{
			FVector       UnitWorld = Transforms[i].GetTransform().GetLocation();
			const float   Radius    = FMath::Max(1.f, Traits[i].CapsuleRadius);
			bool          bMoved    = false;

			// Alle Waende in Reichweite, nicht nur die erste (01.10.2026): mit weicher Feder
			// statt hartem Versetzen wirft eine Ecke aus zwei Waenden die Einheit nicht mehr
			// hin und her - beide nehmen nur ihren eigenen Normalanteil heraus.
			for (const FWallVolume& Volume : Walls)
			{
				const float Contact = Volume.HalfThickness + Radius;
				const float Reach   = Contact + Band;

				// 2a. Grobtest gegen die Huelle der Strecke - vier Vergleiche, erspart den Rest
				// fuer fast alle Einheiten, damit der Prozessor in jedem Bild laufen darf.
				if (UnitWorld.X < Volume.BoundsMin.X - Reach || UnitWorld.X > Volume.BoundsMax.X + Reach ||
					UnitWorld.Y < Volume.BoundsMin.Y - Reach || UnitWorld.Y > Volume.BoundsMax.Y + Reach)
				{
					continue;
				}

				// 2b. Hoehe weiterhin aus der Box - dort entscheidet die Hoehe der Wand, und eine
				// Einheit auf einer Bruecke darueber soll nicht abgestossen werden.
				const FVector Local = Volume.ToLocal.TransformPosition(UnitWorld);
				if (FMath::Abs(Local.Z) >= Volume.HalfSize.Z)
				{
					continue;
				}

				// 2c. Abstand zur STRECKE zwischen den Turmmittelpunkten. Hinter einem Turm klemmt
				// der naechste Punkt auf dessen Mittelpunkt - die Normale wird dort radial und
				// laeuft stetig um den Turm herum. Die alte Box-Logik waehlte die Seite mit der
				// geringsten Durchdringung: Vorzeichenwechsel bei Local.X == 0 und ein Sprung
				// um 90 Grad auf der Diagonalen DepthX == DepthY. Beides gibt es hier nicht.
				const FVector2D P(UnitWorld.X, UnitWorld.Y);
				const FVector2D AP = P - Volume.A;
				const float     T  = FMath::Clamp(static_cast<float>(FVector2D::DotProduct(AP, Volume.Dir)), 0.f, Volume.Length);
				const FVector2D Delta = P - (Volume.A + Volume.Dir * T);
				const float     Dist  = static_cast<float>(Delta.Size());
				if (Dist >= Reach)
				{
					continue;
				}

				FVector2D Normal2D;
				if (Dist > NormalEpsilon)
				{
					Normal2D = Delta / Dist;
				}
				else
				{
					// Mittelpunkt liegt (fast) genau auf der Strecke - die Geometrie gibt keine
					// Seite vor. Die Seite, von der die Einheit kam: gegen ihre Geschwindigkeit.
					// Ohne Geschwindigkeit immer die positive Senkrechte. Deterministisch, damit
					// zwei aufeinanderfolgende Bilder nicht verschiedene Seiten waehlen.
					const FVector2D Perp(-Volume.Dir.Y, Volume.Dir.X);
					float Side = 0.f;
					if (bHasVelocity)
					{
						const FVector& Vel = Velocities[i].Value;
						Side = static_cast<float>(-FVector2D::DotProduct(Perp, FVector2D(Vel.X, Vel.Y)));
					}
					Normal2D = (Side >= 0.f) ? Perp : -Perp;
				}
				const FVector N(Normal2D.X, Normal2D.Y, 0.f);

				const float Gap = Dist - Contact;   // > 0: noch Luft, < 0: steckt drin
				const float Pen = -Gap;

				// 3. Gameplay-Effekt - das, was frueher OnOverlapBegin tun sollte und nie tat,
				// weil Einheiten keine Kollision haben. Ausloesen bei Beruehrung, nicht schon im
				// Annaeherungsband.
				if (Gap < EffectContactMargin)
				{
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
				}

				// 4. Gleiten statt Abprallen. Im Band vor der Wand wird der Anteil, der IN die
				// Wand zeigt, aus Geschwindigkeit, Kraft und Sollgeschwindigkeit herausgenommen -
				// anteilig, von 0 am Bandrand bis 1 bei Beruehrung, damit auch das stetig bleibt.
				// Der Rest bleibt stehen: Trennschub und Laufwunsch quer zur Wand fuehren die
				// Einheit an ihr ENTLANG, statt sie hineinzudruecken.
				// Die Sollgeschwindigkeit gehoert dazu, weil UnitApplyMassMovementProcessor eine
				// Kraft, die gegen die Laufrichtung zeigt, seitlich umlenkt (bTangentialAvoidance):
				// liefe die Einheit weiter in die Wand, wuerde die Gegenfeder unten zum Seitenschub.
				float Weight = 1.f;
				if (Gap > 0.f)
				{
					Weight = Band > KINDA_SMALL_NUMBER ? 1.f - FMath::Clamp(Gap / Band, 0.f, 1.f) : 0.f;
				}

				if (Weight > 0.f)
				{
					FVector& Force = Forces[i].Value;
					const float ForceN = FVector::DotProduct(Force, N);
					if (ForceN < 0.f)
					{
						Force -= N * (ForceN * Weight);
					}

					if (bHasVelocity)
					{
						FVector& Vel = Velocities[i].Value;
						const float VelN = FVector::DotProduct(Vel, N);
						if (VelN < 0.f)
						{
							Vel -= N * (VelN * Weight);
						}
					}

					if (bHasSteering && bSlideDesiredVelocity)
					{
						FVector& Desired = Steerings[i].DesiredVelocity;
						const float DesiredN = FVector::DotProduct(Desired, N);
						if (DesiredN < 0.f)
						{
							Desired -= N * (DesiredN * Weight);
						}
					}
				}

				// 5. Steckt die Einheit drin: weiche Feder mit Daempfung statt hartem Versetzen.
				// Die Daempfung bremst nur die Bewegung NACH AUSSEN (die nach innen ist oben schon
				// entfernt) - sonst schiesst die Feder die Einheit ueber das Band hinaus.
				if (Pen > 0.f)
				{
					const float OutwardSpeed = bHasVelocity
						? FMath::Max(0.f, FVector::DotProduct(Velocities[i].Value, N))
						: 0.f;
					const float Spring = Stiffness * Pen - Damping * OutwardSpeed;
					if (Spring > 0.f)
					{
						Forces[i].Value += N * Spring;
					}

					// Hart versetzt wird nur noch der Rest jenseits des Schlupfs - der Fall, dass
					// eine Einheit tief drin steckt (gespawnt, Wand neu errichtet, Massengedraenge).
					// Kleine Durchdringung loest die Feder allein, ohne Sprung.
					if (Pen > Slop)
					{
						UnitWorld += N * (Pen - Slop);
						bMoved = true;
					}
				}

				if (Debug)
				{
					DrawDebugLine(World, UnitWorld + FVector(0, 0, 20.f),
						UnitWorld + FVector(0, 0, 20.f) + N * 150.f,
						Pen > 0.f ? FColor::Red : FColor::Orange, false, DebugLifeTime, 0, 3.f);
				}
			}

			if (bMoved)
			{
				// Nur X/Y wurden verschoben - Z steht wie zuvor (N hat keinen Z-Anteil).
				Transforms[i].GetMutableTransform().SetLocation(UnitWorld);
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
