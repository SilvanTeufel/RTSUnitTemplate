// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Mass/Replication/MassUnitReplicatorBase.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "Characters/Unit/UnitBase.h"
#include "MassCommonFragments.h"
#include "Mass/Traits/UnitReplicationFragments.h"
#include "MassReplicationFragments.h"
#include "Engine/World.h"
#include "Tests/AutomationCommon.h"
#include "MassRepresentationFragments.h"
#include "Actors/Projectile.h"
#include "MassActorSubsystem.h"
#include "MassArchetypeTypes.h"
#include "Mass/Replication/UnitClientBubbleInfo.h"

#include "MassEntitySubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReplicatorNullProjectileTest, "RTSUnitTemplate.Mass.ReplicatorNullProjectile", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * This test verifies that the replication bits update doesn't crash 
 * when a unit has no ProjectileBaseClass set but has a LastProjectileClass in its AI state.
 */
bool FReplicatorNullProjectileTest::RunTest(const FString& Parameters)
{
    // 1. Setup World and Entity Manager
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!World) return false;

    UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
    if (!EntitySubsystem)
    {
        World->DestroyWorld(false);
        return false;
    }
    FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
    
    // 2. Define fragments and create archetype
    TArray<const UScriptStruct*> FragmentTypes;
    FragmentTypes.Add(FMassAIStateFragment::StaticStruct());
    FragmentTypes.Add(FMassActorFragment::StaticStruct());
    FragmentTypes.Add(FMassNetworkIDFragment::StaticStruct());
    FragmentTypes.Add(FTransformFragment::StaticStruct());
    FragmentTypes.Add(FMassAgentCharacteristicsFragment::StaticStruct());
    FragmentTypes.Add(FMassCombatStatsFragment::StaticStruct());

    FMassArchetypeHandle Archetype = EntityManager.CreateArchetype(FragmentTypes);
    FMassEntityHandle Entity = EntityManager.CreateEntity(Archetype);

    // 3. Setup Fragments
    FMassAIStateFragment& AIS = EntityManager.GetFragmentDataChecked<FMassAIStateFragment>(Entity);
    AIS.LastProjectileClass = AProjectile::StaticClass(); // Non-null to trigger the path
    AIS.IsInitialized = true;
    
    FMassNetworkIDFragment& NetIDFrag = EntityManager.GetFragmentDataChecked<FMassNetworkIDFragment>(Entity);
    NetIDFrag.NetID = FMassNetworkID(1);

    // 4. Spawn Unit Actor and set ProjectileBaseClass to null
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AUnitBase* Unit = World->SpawnActor<AUnitBase>(AUnitBase::StaticClass(), SpawnParams);
    if (!Unit)
    {
        World->DestroyWorld(false);
        return false;
    }
    Unit->ProjectileBaseClass = nullptr; // CRASH TRIGGER
    
    FMassActorFragment& ActorFrag = EntityManager.GetFragmentDataChecked<FMassActorFragment>(Entity);
    ActorFrag.SetNoHandleMapUpdate(Entity, Unit, false);

    // 5. Spawn Bubble Info
    AUnitClientBubbleInfo* BubbleInfo = World->SpawnActor<AUnitClientBubbleInfo>(AUnitClientBubbleInfo::StaticClass(), SpawnParams);
    if (!BubbleInfo)
    {
        Unit->Destroy();
        World->DestroyWorld(false);
        return false;
    }

    // 6. Call UpdateReplicationBits and verify no crash
    FUnitReplicationItem RepItem;
    // Now BubbleInfo is non-null, so it will enter the block and hit the ProjectileBaseClass check.
    bool bChanged = UMassUnitReplicatorBase::UpdateReplicationBits(RepItem, EntityManager, Entity, BubbleInfo);
    
    AddInfo(TEXT("Successfully called UpdateReplicationBits without crashing"));
    TestTrue(TEXT("UpdateReplicationBits should have returned true (bits changed)"), bChanged);

    // Clean up
    BubbleInfo->Destroy();
    Unit->Destroy();
    World->DestroyWorld(false);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
