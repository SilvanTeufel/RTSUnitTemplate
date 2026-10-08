// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Mass/Formation/UnitFormationSubsystem.h"

#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "Steering/MassSteeringFragments.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Mass/UnitMassTag.h"
#include "Core/RTSUnitTemplateSettings.h"
#include "NavigationSystem.h"


// ---------------------------------------------------------------------------------------------
// Die Stellwerte stehen in Project Settings -> Plugins -> RTS Unit Template -> Formation
// (URTSUnitTemplateSettings). Hier bleibt nur der Laufzeitschalter fuer die Konsole.
// ---------------------------------------------------------------------------------------------
static TAutoConsoleVariable<int32> CVarFormationEnable(
	TEXT("rts.formation.enable"),
	1,
	TEXT("0 = Formation halten aus, unabhaengig von den Project Settings (schneller Vorher-Nachher-Vergleich). ")
	TEXT("1 = es gilt 'Enable Formation Keeping' aus den Project Settings."),
	ECVF_Default);


namespace UE::RTSFormation
{
	/** Snapshot of the settings, taken once per processor run (cheap, and consistent within the run). */
	struct FParams
	{
		float BoostNear = 2.f;
		float BoostFar = 1.1f;
		float BoostFalloffDistance = 800.f;
		float MinSlow = 0.7f;
		float CorrectionDistance = 200.f;
		float DeadZone = 15.f;
		float SmoothRate = 8.f;
		float LateralStrength = 0.5f;
		float LateralDistance = 150.f;
		float DetachDistance = 1500.f;
		float MaxCatchUpDistance = 3000.f;
		float FinalDistance = 600.f;
		float TargetTolerance = 600.f;
		bool bMatchSlowest = true;

		static FParams FromSettings()
		{
			const URTSUnitTemplateSettings* S = URTSUnitTemplateSettings::Get();
			FParams P;
			P.BoostFar = FMath::Max(1.f, S->FormationBoostFar);
			P.BoostNear = FMath::Max(P.BoostFar, S->FormationBoostNear);
			P.BoostFalloffDistance = FMath::Max(1.f, S->FormationBoostFalloffDistance);
			P.MinSlow = FMath::Clamp(S->FormationMinSlow, 0.1f, 1.f);
			P.CorrectionDistance = FMath::Max(1.f, S->FormationCorrectionDistance);
			P.DeadZone = FMath::Max(0.f, S->FormationDeadZone);
			P.SmoothRate = FMath::Max(0.1f, S->FormationSmoothRate);
			P.LateralStrength = FMath::Clamp(S->FormationLateralStrength, 0.f, 1.f);
			P.LateralDistance = FMath::Max(1.f, S->FormationLateralDistance);
			P.DetachDistance = FMath::Max(0.f, S->FormationDetachDistance);
			P.MaxCatchUpDistance = FMath::Max(0.f, S->FormationMaxCatchUpDistance);
			P.FinalDistance = FMath::Max(0.f, S->FormationFinalDistance);
			P.TargetTolerance = FMath::Max(0.f, S->FormationTargetTolerance);
			P.bMatchSlowest = S->bFormationMatchSlowest;
			return P;
		}
	};

	/**
	 * Speed factor from the longitudinal error (positive = behind the slot). Shared by both passes.
	 *
	 * Behind the slot: the allowed boost decays exponentially with the distance (near = strong,
	 * far = weak), and near zero a square-root ramp keeps a unit a few centimetres behind from
	 * sprinting past its slot. With the defaults: 30 cm -> 1.37, 1 m -> 1.63, 2 m -> 1.80 (peak),
	 * 5 m -> 1.58, 10 m -> 1.36, 15 m -> 1.24, 30 m -> 1.12.
	 */
	static float ComputeLongitudinalFactor(const float LongError, const FParams& P)
	{
		if (FMath::Abs(LongError) <= P.DeadZone)
		{
			return 1.f;
		}
		const float Ramp = FMath::Min(1.f, FMath::Sqrt(FMath::Abs(LongError) / P.CorrectionDistance));
		if (LongError > 0.f)
		{
			const float MaxBoost = P.BoostFar + (P.BoostNear - P.BoostFar) * FMath::Exp(-LongError / P.BoostFalloffDistance);
			return 1.f + Ramp * (MaxBoost - 1.f);
		}
		return 1.f - Ramp * (1.f - P.MinSlow);
	}
}

// =============================================================================================
// Subsystem
// =============================================================================================

bool UUnitFormationSubsystem::IsFormationEnabled()
{
	return CVarFormationEnable.GetValueOnAnyThread() != 0 && URTSUnitTemplateSettings::Get()->bFormationEnabled;
}


int32 UUnitFormationSubsystem::GetMinGroupSize()
{
	return FMath::Max(2, URTSUnitTemplateSettings::Get()->FormationMinGroupSize);
}

int32 UUnitFormationSubsystem::RegisterGroup(const TArray<FMassEntityHandle>& Entities, const TArray<FVector>& Targets)
{
	check(IsInGameThread());
	const int32 Count = FMath::Min(Entities.Num(), Targets.Num());
	if (!IsFormationEnabled() || Count < GetMinGroupSize())
	{
		UnregisterEntities(Entities);
		return INDEX_NONE;
	}

	FUnitFormationGroup Group;
	FVector2f Sum = FVector2f::ZeroVector;
	for (int32 i = 0; i < Count; ++i)
	{
		Sum += FVector2f(Targets[i].X, Targets[i].Y);
	}
	Group.TargetCentroid = Sum / static_cast<float>(Count);

	FScopeLock Lock(&PendingLock);
	const int32 GroupId = NextGroupId++;
	if (NextGroupId == MAX_int32)
	{
		NextGroupId = 0;
	}

	for (int32 i = 0; i < Count; ++i)
	{
		FUnitFormationRequest& Request = PendingRequests.AddDefaulted_GetRef();
		Request.Entity = Entities[i];
		Request.GroupId = GroupId;
		Request.Offset = FVector2f(Targets[i].X, Targets[i].Y) - Group.TargetCentroid;
		Group.Radius = FMath::Max(Group.Radius, Request.Offset.Size());
	}
	PendingGroups.Add(GroupId, Group);
	return GroupId;
}

void UUnitFormationSubsystem::UnregisterEntities(const TArray<FMassEntityHandle>& Entities)
{
	check(IsInGameThread());
	FScopeLock Lock(&PendingLock);
	for (const FMassEntityHandle& Entity : Entities)
	{
		FUnitFormationRequest& Request = PendingRequests.AddDefaulted_GetRef();
		Request.Entity = Entity;
		Request.GroupId = INDEX_NONE;
	}
}

void UUnitFormationSubsystem::ConsumePendingRequests()
{
	TArray<FUnitFormationRequest> Requests;
	TMap<int32, FUnitFormationGroup> NewGroups;
	{
		FScopeLock Lock(&PendingLock);
		if (PendingRequests.IsEmpty() && PendingGroups.IsEmpty())
		{
			return;
		}
		Requests = MoveTemp(PendingRequests);
		NewGroups = MoveTemp(PendingGroups);
		PendingRequests.Reset();
		PendingGroups.Reset();
	}

	for (TPair<int32, FUnitFormationGroup>& Pair : NewGroups)
	{
		Groups.Add(Pair.Key, Pair.Value);
	}

	// In Befehlsreihenfolge abarbeiten: ein spaeterer Befehl ueberschreibt einen frueheren.
	for (const FUnitFormationRequest& Request : Requests)
	{
		if (Request.Entity.Index < 0)
		{
			continue;
		}
		if (Request.GroupId == INDEX_NONE)
		{
			RemoveMember(Request.Entity);
			continue;
		}
		if (!Members.IsValidIndex(Request.Entity.Index))
		{
			Members.SetNum(Request.Entity.Index + 1, EAllowShrinking::No);
		}
		FUnitFormationMember& Member = Members[Request.Entity.Index];
		Member.GroupId = Request.GroupId;
		Member.SerialNumber = Request.Entity.SerialNumber;
		Member.Offset = Request.Offset;
		Member.SmoothedFactor = 1.f;
	}
}

void UUnitFormationSubsystem::RemoveMember(const FMassEntityHandle Entity)
{
	if (Members.IsValidIndex(Entity.Index) && Members[Entity.Index].SerialNumber == Entity.SerialNumber)
	{
		Members[Entity.Index].GroupId = INDEX_NONE;
	}
}

// =============================================================================================
// Pass 1: Gather
// =============================================================================================

UUnitFormationGatherProcessor::UUnitFormationGatherProcessor(): EntityQuery()
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	bAutoRegisterWithProcessingPhases = true;
	ExecutionOrder.ExecuteBefore.Add(FName("UnitFormationSteerProcessor"));

	// Server UND Client: der Client rechnet dieselbe Regelung in seiner Vorhersage mit (Client Side
	// Navigation with Server Reconciliation). Nur auf dem Server liess sie den Client mit vollem
	// Tempo gegen den geregelten Server laufen -> der Reconciler zog staendig zurueck (Ruckeln).
	ExecutionFlags = static_cast<uint8>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Client);

	// GameThread wegen der Navmesh-Abfragen (Seitenlenkung). Der Durchlauf ist ohnehin seriell und
	// kostet ~0,05-0,09 ms; die teure Parallelarbeit macht der Steer-Prozessor.
	bRequiresGameThreadExecution = true;
}

void UUnitFormationGatherProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	// Client: Ziel und Tempo der Vorhersage.
	EntityQuery.AddRequirement<FMassClientPredictionFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddTagRequirement<FUnitFormationMemberTag>(EMassFragmentPresence::All);
	EntityQuery.AddSubsystemRequirement<UUnitFormationSubsystem>(EMassFragmentAccess::ReadWrite);
	EntityQuery.RegisterWithProcessor(*this);

	ProcessorRequirements.AddSubsystemRequirement<UUnitFormationSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UUnitFormationGatherProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(UUnitFormationGatherProcessor);


	UUnitFormationSubsystem* Formation = Context.GetMutableSubsystem<UUnitFormationSubsystem>();
	if (!Formation)
	{
		return;
	}

	Formation->ConsumePendingRequests();

	// Auch ohne Gruppe weiterlaufen: noch markierte Einheiten (Gruppe frisch aufgeloest) verlieren
	// unten ihr Tag. Ohne markierte Einheiten laeuft die Schleife ueber null Chunks.
	TMap<int32, FUnitFormationGroup>& Groups = Formation->GetGroupsMutable();

	struct FAccumulator
	{
		FVector2f Sum = FVector2f::ZeroVector;
		FVector2f SumAll = FVector2f::ZeroVector;
		float MinSpeed = TNumericLimits<float>::Max();
		float SumFactor = 0.f;
		int32 FactorCount = 0;
		int32 Count = 0;
		int32 CountAll = 0;
		int32 CountInRange = 0;
	};
	TMap<int32, FAccumulator> Accumulators;
	Accumulators.Reserve(Groups.Num());

	const UE::RTSFormation::FParams Params = UE::RTSFormation::FParams::FromSettings();
	const UWorld* GatherWorld = EntityManager.GetWorld();
	const bool bClient = GatherWorld && GatherWorld->GetNetMode() == NM_Client;
	const float DetachDistance = Params.DetachDistance;
	const float TargetToleranceSq = FMath::Square(Params.TargetTolerance);
	const float MaxCatchUpDistanceSq = FMath::Square(Params.MaxCatchUpDistance);

	// Seitenlenkung darf niemanden vom Navmesh schieben (Klippen hoch, ueber Kanten). Je Einheit ein
	// Navmesh-Strahl Richtung Platz, verteilt auf NavCheckSpread Durchlaeufe. Ohne Navmesh (Client ohne
	// Navigationsdaten) bleibt die Lenkung erlaubt - dort korrigiert ohnehin der Server.
	constexpr uint32 NavCheckSpread = 8;
	const uint32 NavPhase = NavCheckPass++ % NavCheckSpread;
	UNavigationSystemV1* NavSys = GatherWorld ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(const_cast<UWorld*>(GatherWorld)) : nullptr;
	const bool bCheckNav = NavSys && NavSys->GetDefaultNavDataInstance() != nullptr && Params.LateralStrength > 0.f;
	const float LateralProbe = Params.LateralDistance;

	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const int32 NumEntities = ChunkContext.GetNumEntities();
		const TConstArrayView<FTransformFragment> TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = ChunkContext.GetFragmentView<FMassMoveTargetFragment>();
		const TConstArrayView<FMassClientPredictionFragment> PredList = ChunkContext.GetFragmentView<FMassClientPredictionFragment>();
		const bool bUsePrediction = bClient && PredList.Num() > 0;

		// Marschiert der ganze Chunk noch? Tags gelten je Archetyp, also einmal je Chunk.
		// Kampf, Verfolgung, Tod oder Direktsteuerung beenden die Mitgliedschaft.
		const bool bChunkMarching =
			ChunkContext.DoesArchetypeHaveTag<FMassStateRunTag>()
			&& !ChunkContext.DoesArchetypeHaveTag<FMassStateAttackTag>()
			&& !ChunkContext.DoesArchetypeHaveTag<FMassStateChaseTag>()
			&& !ChunkContext.DoesArchetypeHaveTag<FMassStatePauseTag>()
			&& !ChunkContext.DoesArchetypeHaveTag<FMassStateDeadTag>()
			&& !ChunkContext.DoesArchetypeHaveTag<FMassDirectControlTag>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
			const FUnitFormationMember* Member = Formation->FindMember(Entity);
			const FUnitFormationGroup* Group = Member ? Groups.Find(Member->GroupId) : nullptr;

			// Schon beim Befehl geprueft: ist das Formationsziel noch das Ziel der Einheit? Andere
			// Befehlswege (Einzelbefehl, Wegpunkte, KI) setzen MoveTarget direkt, ohne uns abzumelden.
			bool bStillMember = bChunkMarching && Group != nullptr;
			if (bStillMember)
			{
				const FVector2f ExpectedTarget = Group->TargetCentroid + Member->Offset;
				// Client: solange eine Vorhersage laeuft, ist SIE das Ziel - MoveTarget.Center kommt
				// erst mit der Replikation nach und kann noch auf den alten Befehl zeigen.
				const FVector& Center = (bUsePrediction && PredList[i].bHasData) ? PredList[i].Location : MoveTargetList[i].Center;
				bStillMember = FVector2f::DistSquared(ExpectedTarget, FVector2f(Center.X, Center.Y)) <= TargetToleranceSq;
			}

			if (!bStillMember)
			{
				if (Member)
				{
					Formation->RemoveMember(Entity);
				}
				ChunkContext.Defer().RemoveTag<FUnitFormationMemberTag>(Entity);
				continue;
			}

			const FVector& Location = TransformList[i].GetTransform().GetLocation();
			const FVector2f Location2D(Location.X, Location.Y);

			if (bCheckNav && Group->bHasCentroid && (static_cast<uint32>(Entity.Index) % NavCheckSpread) == NavPhase)
			{
				const FVector2f Right(-Group->Heading.Y, Group->Heading.X);
				const float LateralError = FVector2f::DotProduct(Group->Centroid + Member->Offset - Location2D, Right);
				bool bBlocked = false;
				if (FMath::Abs(LateralError) > Params.DeadZone)
				{
					// Strahl seitlich zum Platz plus ein Stueck nach vorn - so weit, wie die Lenkung in
					// den naechsten Durchlaeufen fuehren kann.
					const FVector2f Probe = Right * FMath::Clamp(LateralError, -LateralProbe, LateralProbe) + Group->Heading * (LateralProbe * 0.5f);
					const FVector End(Location.X + Probe.X, Location.Y + Probe.Y, Location.Z);
					FVector Hit;
					bBlocked = UNavigationSystemV1::NavigationRaycast(const_cast<UWorld*>(GatherWorld), Location, End, Hit);
				}
				if (FUnitFormationMember* MutableMember = Formation->FindMemberMutable(Entity))
				{
					MutableMember->bLateralBlocked = bBlocked;
				}
			}

			FAccumulator& Acc = Accumulators.FindOrAdd(Member->GroupId);
			++Acc.CountAll;

			// Ausser Reichweite: bleibt Mitglied (holt sie die Gruppe ein, wird sie wieder geregelt),
			// zaehlt aber weder zum Schwerpunkt noch zum Gruppentempo.
			if (Group->bHasCentroid
				&& FVector2f::DistSquared(Group->Centroid + Member->Offset, Location2D) > MaxCatchUpDistanceSq)
			{
				continue;
			}

			Acc.SumAll += Location2D;
			++Acc.CountInRange;
			const float OwnSpeed = (bUsePrediction && PredList[i].bHasData && PredList[i].PredDesiredSpeed > KINDA_SMALL_NUMBER)
				? PredList[i].PredDesiredSpeed : MoveTargetList[i].DesiredSpeed.Get();
			Acc.MinSpeed = FMath::Min(Acc.MinSpeed, OwnSpeed);

			// Abgehaengt wird gegen den Schwerpunkt des VORIGEN Durchlaufs gemessen; im ersten
			// Durchlauf gibt es keinen, dann zaehlen alle.
			bool bDetached = false;
			if (Group->bHasCentroid)
			{
				const FVector2f Slot = Group->Centroid + Member->Offset;
				const float LongError = FVector2f::DotProduct(Slot - Location2D, Group->Heading);
				bDetached = LongError > DetachDistance;
				if (!bDetached)
				{
					Acc.SumFactor += UE::RTSFormation::ComputeLongitudinalFactor(LongError, Params);
					++Acc.FactorCount;
				}
			}

			if (!bDetached)
			{
				Acc.Sum += Location2D;
				++Acc.Count;
			}
		}
	});


	const float FinalDistance = Params.FinalDistance;
	for (auto It = Groups.CreateIterator(); It; ++It)
	{
		FUnitFormationGroup& Group = It.Value();
		const FAccumulator* Acc = Accumulators.Find(It.Key());

		// Unter zwei Mitglieder gibt es keine Formation mehr zu halten.
		// Gemessen 07.10.2026: ohne Karenz loeste sich JEDE Gruppe aus noch nicht markierten Einheiten
		// im ersten Durchlauf auf, weil das Tag erst verzoegert gesetzt wird. Funktioniert hatte es
		// vorher nur, solange die Einheiten das Tag noch vom letzten Befehl trugen.
		if (!Acc || Acc->CountAll < 2)
		{
			if (++Group.EmptyPasses >= 30)
			{
				It.RemoveCurrent();
			}
			continue;
		}
		Group.EmptyPasses = 0;
		Group.FactorBias = (Acc->FactorCount > 0)
			? FMath::Clamp(Acc->SumFactor / static_cast<float>(Acc->FactorCount) - 1.f, -0.3f, 0.3f)
			: 0.f;

		// Alle ausser Reichweite: den alten Schwerpunkt behalten, nichts regeln.
		if (Acc->CountInRange == 0)
		{
			continue;
		}
		Group.Centroid = (Acc->Count > 0)
			? Acc->Sum / static_cast<float>(Acc->Count)
			: Acc->SumAll / static_cast<float>(Acc->CountInRange);
		Group.bHasCentroid = true;
		Group.ActiveCount = Acc->CountAll;
		Group.MinSpeed = Acc->MinSpeed;

		const FVector2f ToTarget = Group.TargetCentroid - Group.Centroid;
		const float Distance = ToTarget.Size();
		if (Distance > KINDA_SMALL_NUMBER)
		{
			Group.Heading = ToTarget / Distance;
		}
		// Radius gedeckelt: ein einzelnes weit abgelegenes Ziel (gemessen Radius 8370) hielt die Gruppe
		// sonst schon 125 m vor dem Ziel in der Endphase, also ohne Regelung.
		Group.bFinalPhase = Distance < FMath::Max(FinalDistance, FMath::Min(Group.Radius * 1.5f, 3000.f));
	}
}

// =============================================================================================
// Pass 2: Steer
// =============================================================================================

UUnitFormationSteerProcessor::UUnitFormationSteerProcessor(): EntityQuery()
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	bAutoRegisterWithProcessingPhases = true;

	// Nach der Pfadverfolgung (setzt DesiredVelocity), vor Ausweichen und Bewegungsanwendung
	// (lesen DesiredVelocity).
	ExecutionOrder.ExecuteAfter.Add(FName("UnitMovementProcessor"));
	ExecutionOrder.ExecuteAfter.Add(FName("UnitFormationGatherProcessor"));
	ExecutionOrder.ExecuteBefore.Add(FName("Avoidance"));

	ExecutionFlags = static_cast<uint8>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Client);
	bRequiresGameThreadExecution = false;
}

void UUnitFormationSteerProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassSteeringFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassClientPredictionFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddTagRequirement<FUnitFormationMemberTag>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassDirectControlTag>(EMassFragmentPresence::None);
	EntityQuery.AddSubsystemRequirement<UUnitFormationSubsystem>(EMassFragmentAccess::ReadWrite);
	EntityQuery.RegisterWithProcessor(*this);

	ProcessorRequirements.AddSubsystemRequirement<UUnitFormationSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UUnitFormationSteerProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(UUnitFormationSteerProcessor);


	if (!UUnitFormationSubsystem::IsFormationEnabled())
	{
		return;
	}

	UUnitFormationSubsystem* Formation = Context.GetMutableSubsystem<UUnitFormationSubsystem>();
	if (!Formation)
	{
		return;
	}
	if (Formation->GetGroups().IsEmpty())
	{
		return;
	}

	const UE::RTSFormation::FParams Params = UE::RTSFormation::FParams::FromSettings();
	const UWorld* SteerWorld = EntityManager.GetWorld();
	const bool bClient = SteerWorld && SteerWorld->GetNetMode() == NM_Client;
	const float MinSlow = Params.MinSlow;
	const float MaxBoost = Params.BoostNear;
	const float DetachDistance = Params.DetachDistance;
	const bool bMatchSlowest = Params.bMatchSlowest;
	const float SmoothAlpha = FMath::Clamp(Context.GetDeltaTimeSeconds() * Params.SmoothRate, 0.f, 1.f);
	const float DeadZone = Params.DeadZone;
	const float LateralStrength = Params.LateralStrength;
	const float LateralDistance = Params.LateralDistance;
	const float MaxCatchUpDistanceSq = FMath::Square(Params.MaxCatchUpDistance);

	// Parallel: jede Chunk-Aufgabe liest nur eigene Fragmente und die in Pass 1 fertige,
	// unveraenderliche Gruppentabelle. Geschrieben wird nur SmoothedFactor der EIGENEN Entitaeten -
	// verschiedene Indizes, und das Feld wird waehrenddessen nicht vergroessert.
	EntityQuery.ParallelForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const int32 NumEntities = ChunkContext.GetNumEntities();
		const TConstArrayView<FTransformFragment> TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = ChunkContext.GetFragmentView<FMassMoveTargetFragment>();
		const TArrayView<FMassSteeringFragment> SteeringList = ChunkContext.GetMutableFragmentView<FMassSteeringFragment>();
		const TConstArrayView<FMassClientPredictionFragment> PredList = ChunkContext.GetFragmentView<FMassClientPredictionFragment>();
		const bool bUsePrediction = bClient && PredList.Num() > 0;


		for (int32 i = 0; i < NumEntities; ++i)
		{
			FUnitFormationMember* Member = Formation->FindMemberMutable(ChunkContext.GetEntity(i));
			if (!Member)
			{
				continue;
			}
			const FUnitFormationGroup* Group = Formation->FindGroup(Member->GroupId);
			if (!Group || !Group->bHasCentroid)
			{
				continue;
			}

			const FVector& Location = TransformList[i].GetTransform().GetLocation();
			const FVector2f Slot = Group->Centroid + Member->Offset;

			// Zu weit weg: gar nicht regeln, die Einheit laeuft in ihrem eigenen Tempo (Balance).
			if (FVector2f::DistSquared(Slot, FVector2f(Location.X, Location.Y)) > MaxCatchUpDistanceSq)
			{
				Member->SmoothedFactor = 1.f;
				continue;
			}

			const float LongError = FVector2f::DotProduct(Slot - FVector2f(Location.X, Location.Y), Group->Heading);

			// Boost faellt mit dem Abstand exponentiell ab (siehe ComputeLongitudinalFactor). Fuer
			// Nicht-Abgehaengte wird der Mittelwert der Gruppe herausgerechnet, damit der Block im
			// Gruppentempo bleibt; Abgehaengte zaehlen nicht zu diesem Mittel.
			float TargetFactor = UE::RTSFormation::ComputeLongitudinalFactor(LongError, Params);
			if (LongError <= DetachDistance)
			{
				TargetFactor -= Group->FactorBias;
			}
			TargetFactor = FMath::Clamp(TargetFactor, MinSlow, MaxBoost);
			if (Group->bFinalPhase)
			{
				// Kurz vor dem Ziel gar nicht mehr regeln: jede Einheit laeuft in ihrem eigenen Tempo auf
				// ihren Endplatz. Vorher "nur aufholen, nie bremsen" - bei kurzen Befehlen (ganze Strecke
				// Endphase) lief die Gruppe dadurch im Schnitt 20 % zu schnell (gemessen 970 statt 800 cm/s),
				// der Client kam nicht hinterher, und es war ein Balanceproblem.
				TargetFactor = 1.f;
			}

			Member->SmoothedFactor += (TargetFactor - Member->SmoothedFactor) * SmoothAlpha;

			FMassSteeringFragment& Steering = SteeringList[i];
			// Client: das Grundtempo ist das der Vorhersage - genau das hat ExecuteClient benutzt.
			const float OwnSpeed = (bUsePrediction && PredList[i].bHasData && PredList[i].PredDesiredSpeed > KINDA_SMALL_NUMBER)
				? PredList[i].PredDesiredSpeed : MoveTargetList[i].DesiredSpeed.Get();
			// DesiredVelocity == 0 heisst: Pfadsuche laeuft, angekommen oder blockiert. Das bleibt so.
			if (OwnSpeed <= KINDA_SMALL_NUMBER || Steering.DesiredVelocity.SizeSquared() < 1.f)
			{
				continue;
			}

			const float BaseSpeed = (bMatchSlowest && !Group->bFinalPhase && Group->MinSpeed > 0.f)
				? FMath::Min(OwnSpeed, Group->MinSpeed)
				: OwnSpeed;
			const float Speed = FMath::Min(BaseSpeed * Member->SmoothedFactor, OwnSpeed * MaxBoost);

			// Richtung behalten, Betrag neu setzen. Bewusst nicht multiplizieren: der
			// UnitMovementProcessor schreibt DesiredVelocity nicht in jedem Bild neu, ein Faktor
			// wuerde sich sonst aufschaukeln.
			FVector Direction = Steering.DesiredVelocity.GetSafeNormal();

			// Seitlich zum Platz lenken. Nur solange der Pfad ungefaehr in Marschrichtung zeigt
			// (cos > 0,5): um eine Ecke herum folgt die Einheit dem Pfad, sonst lenkt sie in die Wand.
			// Nicht in der Endphase (dort laeuft sie ohnehin auf ihren Endplatz) und nicht abgehaengt.
			if (LateralStrength > 0.f && !Group->bFinalPhase && LongError <= DetachDistance && !Member->bLateralBlocked)
			{
				FVector2f PathDir2D(Direction.X, Direction.Y);
				if (PathDir2D.Normalize() && FVector2f::DotProduct(PathDir2D, Group->Heading) > 0.5f)
				{
					const FVector2f Right(-Group->Heading.Y, Group->Heading.X);
					const float LateralError = FVector2f::DotProduct(Slot - FVector2f(Location.X, Location.Y), Right);
					if (FMath::Abs(LateralError) > DeadZone)
					{
						const float Correction = FMath::Clamp(LateralError / LateralDistance, -1.f, 1.f) * LateralStrength;
						FVector2f NewDir2D = PathDir2D + Right * Correction;
						if (NewDir2D.Normalize())
						{
							Direction = FVector(NewDir2D.X, NewDir2D.Y, 0.f);
						}
					}
				}
			}

			// Server: Betrag fest setzen. Client: RELATIV skalieren - ExecuteClient hat das Tempo schon
			// durch die Abstandsbremse (RTS.ClientMaxVoraus) gedrueckt, und die muss erhalten bleiben.
			const float AppliedSpeed = bClient
				? Steering.DesiredVelocity.Size() * (Speed / OwnSpeed)
				: Speed;
			Steering.DesiredVelocity = Direction * AppliedSpeed;
		}
	});
}
