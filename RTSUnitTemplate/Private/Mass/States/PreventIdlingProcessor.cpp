// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/PreventIdlingProcessor.h"

#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "ProfilingDebugging/CsvProfiler.h"

UPreventIdlingProcessor::UPreventIdlingProcessor()
	: PatrolIdleQuery(), IdleQuery()
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bAutoRegisterWithProcessingPhases = true;

	// Es werden nur eigene Fragmente gelesen und Signale verzoegert abgesetzt -
	// kein Zugriff auf fremde Entities, also kein Game-Thread-Zwang.
	bRequiresGameThreadExecution = false;
}

void UPreventIdlingProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	// Die Forderung nach FMassPatrolFragment filtert nebenbei alles heraus, was
	// gar keine Patrouille kennt - Gebaeude und Effektflaechen fallen so von
	// selbst aus der Abfrage.
	auto Setup = [&EntityManager](FMassEntityQuery& Query)
	{
		Query.Initialize(EntityManager);
		Query.AddTagRequirement<FMassPreventIdlingTag>(EMassFragmentPresence::All);

		Query.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
		Query.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
		Query.AddRequirement<FMassPatrolFragment>(EMassFragmentAccess::ReadOnly);

		// Wer kaempft, castet oder gerade getroffen wird, wird nicht angefasst.
		Query.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
		Query.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
		Query.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
		Query.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
		Query.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
		Query.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
	};

	Setup(PatrolIdleQuery);
	PatrolIdleQuery.AddTagRequirement<FMassStatePatrolIdleTag>(EMassFragmentPresence::All);
	PatrolIdleQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::None);
	PatrolIdleQuery.RegisterWithProcessor(*this);

	Setup(IdleQuery);
	IdleQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::All);
	IdleQuery.AddTagRequirement<FMassStatePatrolIdleTag>(EMassFragmentPresence::None);
	IdleQuery.RegisterWithProcessor(*this);
}

void UPreventIdlingProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);
	SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UPreventIdlingProcessor::RunQuery(FMassEntityQuery& Query, FMassExecutionContext& Context)
{
	DbgSeen = 0;
	DbgSwitching = 0;
	DbgHasTarget = 0;
	DbgNoWaypoint = 0;
	DbgTooEarly = 0;
	DbgSignalled = 0;

	Query.ForEachEntityChunk(Context, [this](FMassExecutionContext& ChunkContext)
	{
		const int32 NumEntities = ChunkContext.GetNumEntities();
		auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
		const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
		const auto PatrolList = ChunkContext.GetFragmentView<FMassPatrolFragment>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			FMassAIStateFragment& StateFrag = StateList[i];
			const FMassAITargetFragment& TargetFrag = TargetList[i];
			const FMassPatrolFragment& PatrolFrag = PatrolList[i];

			++DbgSeen;

			// Mitten im Zustandswechsel nicht dazwischenfunken.
			if (StateFrag.SwitchingState)
			{
				++DbgSwitching;
				continue;
			}

			// Wer ein Ziel hat, steht nicht sinnlos herum - darum kuemmert sich
			// der Kampfpfad, und ein Patrouillenbefehl wuerde ihn stoeren.
			if (TargetFrag.bHasValidTarget)
			{
				++DbgHasTarget;
				continue;
			}

			// Ohne Wegpunkt gibt es kein Patrouillenziel; PISwitcher wuerde nur
			// den Zustandstimer zuruecksetzen und die Einheit bliebe stehen.
			if (PatrolFrag.TargetWaypointLocation.IsNearlyZero())
			{
				++DbgNoWaypoint;
				continue;
			}

			// Nur wirksam, wenn MaxIdleSeconds ausdruecklich hochgesetzt wurde -
			// siehe die Begruendung an der Deklaration.
			if (MaxIdleSeconds > 0.f && StateFrag.StateTimer < MaxIdleSeconds)
			{
				++DbgTooEarly;
				continue;
			}

			++DbgSignalled;

			if (bRepairPlaceholderSignal)
			{
				StateFrag.PlaceholderSignal = UnitSignals::PatrolRandom;
			}

			// Derselbe Weg, den die Einheit von allein gegangen waere: der
			// Handler sucht ein neues Zufallsziel und schaltet auf PatrolRandom.
			SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::PISwitcher, ChunkContext.GetEntity(i));
			StateFrag.StateTimer = 0.f;
		}
	});
}

void UPreventIdlingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{

	// Siehe mass_scopes: macht diesen Prozessor als Spalte Exclusive/UPreventIdlingProcessor im CSV sichtbar.
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(UPreventIdlingProcessor);

	TimeSinceLastRun += Context.GetDeltaTimeSeconds();
	if (TimeSinceLastRun < ExecutionInterval)
	{
		return;
	}
	TimeSinceLastRun -= ExecutionInterval;

	if (!SignalSubsystem)
	{
		return;
	}

	RunQuery(PatrolIdleQuery, Context);
	const FString PatrolIdleReport = DbgReport();

	RunQuery(IdleQuery, Context);
	const FString IdleReport = DbgReport();

	if (bDebugLog)
	{
		DbgLogAccu += ExecutionInterval;
		if (DbgLogAccu >= 3.f)
		{
			DbgLogAccu = 0.f;
			UE_LOG(LogTemp, Warning, TEXT("PREVIDLE PatrolIdle{%s} Idle{%s}"),
				*PatrolIdleReport, *IdleReport);
		}
	}
}

FString UPreventIdlingProcessor::DbgReport() const
{
	return FString::Printf(
		TEXT("gesehen=%d switching=%d ziel=%d ohneWP=%d zufrueh=%d signal=%d"),
		DbgSeen, DbgSwitching, DbgHasTarget, DbgNoWaypoint, DbgTooEarly, DbgSignalled);
}
