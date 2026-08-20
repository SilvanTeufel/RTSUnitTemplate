// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/Avoidance/DynamicObstacleRegProcessor.h"

#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassCommonTypes.h"
#include "MassNavigationFragments.h"
#include "Avoidance/MassAvoidanceFragments.h"
#include "Mass/UnitMassTag.h"
#include "NavigationSystem.h"


namespace
{
    static constexpr float InitialCellSize = 100.f;
    static constexpr float LifeTime = 3.0f;
    static constexpr float LargeRadiusThreshold = InitialCellSize/2.f;
    static constexpr float SubObstacleRadius = InitialCellSize;
    static constexpr float ClusteringDistanceThreshold = 150.f;
    
    const FColor BoxColor_Single = FColor::Red;
    const FColor SubBoxColor_Circle = FColor::Orange;
    const FColor SubBoxColor_Merged = FColor::Yellow;
}


UDynamicObstacleRegProcessor::UDynamicObstacleRegProcessor()
{
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	bRequiresGameThreadExecution = true;
}

void UDynamicObstacleRegProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	auto SetupQuery = [&](FMassEntityQuery& Query)
	{
		Query.Initialize(EntityManager);
		Query.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
		Query.AddRequirement<FMassAvoidanceColliderFragment>(EMassFragmentAccess::ReadOnly);
		Query.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite);
		Query.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadOnly);
		Query.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::None);
		Query.AddTagRequirement<FMassStateDisableObstacleTag>(EMassFragmentPresence::None);
		Query.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
		Query.AddTagRequirement<FMassStateRepairTag>(EMassFragmentPresence::None);
		Query.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::None);
		Query.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
		Query.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
		Query.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::Any);;
		Query.RegisterWithProcessor(*this);
	};

	SetupQuery(BuildObstacleQuery);
	BuildObstacleQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::All);

	SetupQuery(RepairObstacleQuery);
	RepairObstacleQuery.AddTagRequirement<FMassStateRepairTag>(EMassFragmentPresence::All);

	SetupQuery(PauseObstacleQuery);
	PauseObstacleQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::All);

	SetupQuery(IdleObstacleQuery);
	IdleObstacleQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::All);
}
// --- Main Execution ---

void UDynamicObstacleRegProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return;
    }
    TimeSinceLastRun -= ExecutionInterval;

    // --- NEW: Destroy volumes spawned in the previous frame ---
    for (const TWeakObjectPtr<AActor>& VolumePtr : SpawnedNavVolumes)
    {
        if (VolumePtr.IsValid())
        {
            VolumePtr->Destroy();
        }
    }
    SpawnedNavVolumes.Empty(); // Clear the array for the new frame

    UMassNavigationSubsystem* NavSys = Context.GetWorld()->GetSubsystem<UMassNavigationSubsystem>();
    if (!ensure(NavSys != nullptr))
    {
        return;
    }

    // Clear and re-initialize the grid for this frame.
    NavSys->GetObstacleGridMutable().Initialize(InitialCellSize);

    // Pass 1: Collect static obstacles and immediately process dynamic ones.
    auto ProcessQuery = [&](FMassEntityQuery& Query)
    {
        CollectAndProcessObstacles(Context, Query, *NavSys);
    };

    ProcessQuery(BuildObstacleQuery);
    ProcessQuery(RepairObstacleQuery);
    ProcessQuery(PauseObstacleQuery);
    ProcessQuery(IdleObstacleQuery);
}

void UDynamicObstacleRegProcessor::CollectAndProcessObstacles(FMassExecutionContext& Context, FMassEntityQuery& Query, UMassNavigationSubsystem& NavSys)
{
    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(Context.GetWorld());
    if (!NavSystem) return;

    // Zaehler fuer die Belegzeile weiter unten. Lokal, also pro Durchlauf - die Zahl sagt,
    // wie viele Objekte in DIESEM Takt nicht als Ausweichhindernis eingetragen werden konnten.
    int32 UebersprungeneHindernisse = 0;
    int32 EingetrageneHindernisse = 0;

    Query.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        const auto& Colliders = ChunkContext.GetFragmentView<FMassAvoidanceColliderFragment>();
        const TArrayView<FMassAgentCharacteristicsFragment> CharList = ChunkContext.GetMutableFragmentView<FMassAgentCharacteristicsFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            if (CharList[i].bIsFlying) continue;

            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            const FVector Location = CharList[i].PositionedTransform.GetLocation();

            const float Radius = Colliders[i].GetCircleCollider().Radius;

            // Suchbox mit der tatsaechlichen Groesse skalieren statt fester 100 Einheiten.
            //
            // Der Test soll aussortieren, was gar nicht im begehbaren Bereich liegt (Leichen unter der
            // Welt, Platzhalter). Mit fester Box von 100 fielen aber GEBAEUDE durch: sie stanzen ein
            // Loch ins Navigationsnetz, ihre Mitte liegt also mitten im Loch, und bei einem grossen
            // Gebaeude ist der naechste begehbare Punkt weiter als 100 entfernt. Ein Gebaeude, das die
            // Pruefung nicht besteht, wird NIE als Ausweichhindernis eingetragen - Einheiten weichen
            // ihm dann nicht aus, und Arbeiter bleiben zwischen Gebaeude und anderen Einheiten haengen.
            //
            // Der Radius des eigenen Kolliders ist genau das richtige Mass: er sagt, wie weit das
            // Objekt selbst reicht, also wie weit der naechste begehbare Punkt entfernt sein darf.
            const FVector SuchBox(FMath::Max(100.f, Radius + 200.f),
                                  FMath::Max(100.f, Radius + 200.f), 300.f);
            FNavLocation NavLoc;
            if (!NavSystem->ProjectPointToNavigation(Location, NavLoc, SuchBox))
            {
                // Belegzeile fuer die Boxvergroesserung: wer hier durchfaellt, wird NICHT als
                // Ausweichhindernis eingetragen und kann von anderen Einheiten nicht umgangen werden.
                // Vor der Aenderung war die Box fest 100 - bei grossen Gebaeuden zu klein.
                ++UebersprungeneHindernisse;
                continue;
            }
            ++EingetrageneHindernisse;
            
            // Stationary units should always be obstacles
            AddSingleObstacleToGrid(NavSys, Entity, Location, Radius);
        }
    });

    // Nur melden, wenn tatsaechlich etwas durchgefallen ist - und hoechstens jede Sekunde.
    if (UebersprungeneHindernisse > 0)
    {
        static thread_local double LetzteMeldung = 0.0;
        const double Jetzt = Context.GetWorld() ? Context.GetWorld()->GetTimeSeconds() : 0.0;
        if (Jetzt - LetzteMeldung > 1.0 || Jetzt < LetzteMeldung)
        {
            LetzteMeldung = Jetzt;
            UE_LOG(LogTemp, Warning,
                TEXT("[Hindernis] %d Objekte nicht als Ausweichhindernis eingetragen (Projektion gescheitert), %d eingetragen"),
                UebersprungeneHindernisse, EingetrageneHindernisse);
        }
    }
}




void UDynamicObstacleRegProcessor::AddSingleObstacleToGrid(UMassNavigationSubsystem& NavSys, const FMassEntityHandle Entity, const FVector& Location, const float Radius)
{
    // ... implementation is unchanged ...
    UWorld* World = NavSys.GetWorld();
    if (Radius > LargeRadiusThreshold)
    {
        const float SubObstacleDiameter = SubObstacleRadius * 2.0f;
        const int32 NumSubObstacles = FMath::CeilToInt((2.0f * PI * Radius) / SubObstacleDiameter);

        for (int32 j = 0; j < NumSubObstacles; ++j)
        {
            const float Angle = (static_cast<float>(j) / NumSubObstacles) * 2.0f * PI;
            const FVector Offset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
            const FBox SubBounds(Location + Offset - FVector(SubObstacleRadius), Location + Offset + FVector(SubObstacleRadius));

            if (Debug) DrawDebugBox(World, SubBounds.GetCenter(), SubBounds.GetExtent(), SubBoxColor_Circle, false, LifeTime, 0, 2.0f);

            NavSys.GetObstacleGridMutable().Add(FMassNavigationObstacleItem(Entity, EMassNavigationObstacleFlags::HasColliderData), SubBounds);
        }
    }
    else
    {
        const FBox Bounds(Location - FVector(Radius), Location + FVector(Radius));
        if (Debug) DrawDebugBox(World, Bounds.GetCenter(), Bounds.GetExtent(), BoxColor_Single, false, LifeTime, 0, 2.0f);
        NavSys.GetObstacleGridMutable().Add(FMassNavigationObstacleItem(Entity, EMassNavigationObstacleFlags::HasColliderData), Bounds);
    }
}
