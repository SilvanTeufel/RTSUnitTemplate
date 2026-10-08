// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Mass/Replication/ServerReplicationKickProcessor.h"

#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "MassCommandBuffer.h"
#include "MassCommands.h"
#include "MassReplicationFragments.h"
#include "MassReplicationSubsystem.h"
#include "MassLODSubsystem.h"
#include "Mass/Replication/MassUnitReplicatorBase.h"
#include "Mass/Replication/ReplicationBootstrap.h"
#include "Mass/Replication/UnitRegistryReplicator.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Mass/Replication/ReplicationSettings.h"
#include "Mass/UnitMassTag.h"
#include "MassEntitySubsystem.h"
#include "MassActorSubsystem.h"
#include "EngineUtils.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/MassActorBindingComponent.h"
#include "GameStates/ResourceGameState.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "Characters/Unit/ConstructionUnit.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "MassNavigationFragments.h"
#include "Mass/MassUnitVisualFragments.h"

// Forward-declare slice control API implemented in MassUnitReplicatorBase.cpp
namespace ReplicationSliceControl
{
	void SetSlice(int32 StartIndex, int32 Count);
	void SetIndices(const TArray<int32>* Indices);
	void ClearSlice();
}

// Configurable grace period CVAR: number of seconds after world start to bypass change-based skip
static TAutoConsoleVariable<float> CVarRTSUnitStartupRepGraceSeconds(
	TEXT("r.RTSUnit.StartupRepGraceSeconds"),
	10.0f,
	TEXT("Seconds to force server replication regardless of transform change after world start."),
	ECVF_Default);

// CVARs to control processor work and logging
static TAutoConsoleVariable<int32> CVarRTS_ServerKick_Enable(
	TEXT("net.RTS.ServerReplicationKick.Enable"),
	1,
	TEXT("Enable/disable ServerReplicationKickProcessor."),
	ECVF_Default);
static TAutoConsoleVariable<int32> CVarRTS_ServerKick_MaxPerTick(
		TEXT("net.RTS.ServerReplicationKick.MaxPerTick"),
		256,
		TEXT("Max CHANGED entities replicated per run (10 Hz). Unchanged entities are only checked and cost no budget. Default 256."),
		ECVF_Default);
static TAutoConsoleVariable<int32> CVarRTS_ServerKick_LogLevel(
	TEXT("net.RTS.ServerReplicationKick.LogLevel"),
	0,
	TEXT("Logging level: 0=Off, 1=Warn, 2=Verbose."),
	ECVF_Default);

// CVAR: when 1, do NOT skip clean chunks. This forces the replicator to run even when
// transform and signature appear unchanged, allowing tag-only updates to propagate.
static TAutoConsoleVariable<int32> CVarRTS_ServerKick_ProcessCleanChunks(
	TEXT("net.RTS.ServerReplicationKick.ProcessCleanChunks"),
	0,
	TEXT("Process chunks even when signature unchanged (0=skip clean chunks, 1=process anyway). Default 1 to ensure tag-only changes replicate."),
	ECVF_Default);

// Sicherheitsnetz: auch Einheiten ohne erkannte Aenderung gehen spaetestens nach RefreshSeconds einmal durch den
// Replikator. Der vergleicht selbst mit dem zuletzt gesendeten Stand und sendet nur echte Unterschiede - kostet
// also nur CPU, keine Bandbreite, und faengt alles ab, was die Signatur unten nicht kennt.
static TAutoConsoleVariable<float> CVarRTS_ServerKick_RefreshSeconds(
	TEXT("net.RTS.ServerReplicationKick.RefreshSeconds"),
	2.0f,
	TEXT("Unchanged entities are re-checked by the replicator at least this often (seconds). 0 disables. Default 2."),
	ECVF_Default);
static TAutoConsoleVariable<int32> CVarRTS_ServerKick_RefreshMaxPerTick(
	TEXT("net.RTS.ServerReplicationKick.RefreshMaxPerTick"),
	64,
	TEXT("Max unchanged entities re-checked per run (counts toward MaxPerTick). Default 64."),
	ECVF_Default);

// CVAR: Control the legacy server-side re-registration fallback. Now enabled by default for robust startup registration.
static TAutoConsoleVariable<int32> CVarRTS_ServerKick_ReRegisterMissing(
	TEXT("net.RTS.ServerReplicationKick.ReRegisterMissing"),
	1,
	TEXT("When 1, perform server-side recovery to re-register Units missing in the replication query by adding NetID and registry entries. Default 1 (enabled) for robust startup."),
	ECVF_Default);

	// File-scope signature structure and storage so we can clear on world teardown
		namespace {
			struct FSig
			{
				FVector Loc; uint16 P=0,Y=0,R=0; FVector Scale; uint32 TagBits = 0u;
				float Health = 0.f; float Shield = 0.f;
				uint8 FireCounter = 0; uint32 TargetNetID = 0;
				// Bewegungsauftrag: neuer Befehl oder neues Tempo (Formationsregler) muss raus, auch bevor
				// sich die Position merklich aendert.
				FVector MoveCenter = FVector::ZeroVector; float MoveSpeed = 0.f; uint16 MoveActionID = 0;
				// Was der Replikator sonst noch sendet und sich auch im Stand aendern kann: Unsichtbarkeit,
				// HoldPosition, CanAttack/CanMove, Zielerfassung, Animation, Effekte, Faehigkeitsziel.
				uint32 StateBits = 0u; FVector AbilityLoc = FVector::ZeroVector; uint32 RunAnimData = 0u;
				// Zeitpunkt des letzten Durchgangs durch den Replikator (fuer das Sicherheitsnetz)
				double SentTime = 0.0;
				
				// Optional: track uncritical data hash to detect changes without full replication? 
				// No, FSig is just for the "Kick" decision.

				bool IsNearlyEqual(const FSig& O, float LocThresh, float AngleThresh, float ScaleThresh) const
				{
					if (TagBits != O.TagBits) return false;
					if (FireCounter != O.FireCounter) return false;
					if (TargetNetID != O.TargetNetID) return false;
					if (!FMath::IsNearlyEqual(Health, O.Health, 0.1f)) return false;
					if (!FMath::IsNearlyEqual(Shield, O.Shield, 0.1f)) return false;
					if (MoveActionID != O.MoveActionID) return false;
					if (!FMath::IsNearlyEqual(MoveSpeed, O.MoveSpeed, 1.f)) return false;
					if (!MoveCenter.Equals(O.MoveCenter, LocThresh)) return false;
					if (StateBits != O.StateBits) return false;
					if (RunAnimData != O.RunAnimData) return false;
					if (!AbilityLoc.Equals(O.AbilityLoc, LocThresh)) return false;

					// If the unit is dead, we don't care about transform changes for the purpose of kicking replication
					const bool bIsDead = (TagBits & UnitTagBits::Dead) != 0;
					if (!bIsDead)
					{
						if (!Loc.Equals(O.Loc, LocThresh)) return false;
						if (!Scale.Equals(O.Scale, ScaleThresh)) return false;
						
						// Angle check (quantized uint16)
						auto Diff = [](uint16 a, uint16 b) { return (uint16)FMath::Abs((int32)a - (int32)b); };
						uint16 ThresholdQ = (uint16)FMath::RoundToInt((AngleThresh / 360.0f) * 65535.0f);
						if (Diff(P, O.P) > ThresholdQ) return false;
						if (Diff(Y, O.Y) > ThresholdQ) return false;
						if (Diff(R, O.R) > ThresholdQ) return false;
					}

					return true;
				}

				bool operator==(const FSig& O) const
				{
					return Loc.Equals(O.Loc, 0.1f) && P==O.P && Y==O.Y && R==O.R && Scale.Equals(O.Scale, 0.01f) && TagBits == O.TagBits && FireCounter == O.FireCounter && TargetNetID == O.TargetNetID;
				}
			};
			static TMap<uint32, FSig> GLastSigByID; // server-only, cleared on world cleanup
			// Per-chunk slice start offset: key built from replicator and chunk identity
			static TMap<uint64, int32> GStartOffsetByChunk;
			static bool GCleanupRegistered = false;
			static FDelegateHandle GCleanupHandle;
			static void EnsureServerKickCleanupRegistered()
			{
				if (!GCleanupRegistered)
				{
					GCleanupHandle = FWorldDelegates::OnWorldCleanup.AddStatic([](UWorld* InWorld, bool, bool){
 					GLastSigByID.Reset();
						// Reset all per-chunk cursors on world cleanup (safe since chunks are world-bound)
						GStartOffsetByChunk.Reset();
					});
					GCleanupRegistered = true;
				}
			}

			// Pretty-print UnitTagBits into a readable list for diagnostics
			static FString StringifyUnitTagBits(uint32 Bits)
			{
				TArray<const TCHAR*> Names;
				if (Bits & UnitTagBits::Dead)               Names.Add(TEXT("Dead"));
				if (Bits & UnitTagBits::Rooted)             Names.Add(TEXT("Rooted"));
				if (Bits & UnitTagBits::Casting)            Names.Add(TEXT("Casting"));
				if (Bits & UnitTagBits::Charging)           Names.Add(TEXT("Charging"));
				if (Bits & UnitTagBits::IsAttacked)         Names.Add(TEXT("IsAttacked"));
				if (Bits & UnitTagBits::Idle)               Names.Add(TEXT("Idle"));
				if (Bits & UnitTagBits::Build)              Names.Add(TEXT("Build"));
				if (Bits & UnitTagBits::ResourceExtraction) Names.Add(TEXT("ResourceExtraction"));
				if (Bits & UnitTagBits::GoToResource)       Names.Add(TEXT("GoToResource"));
				if (Bits & UnitTagBits::GoToBuild)          Names.Add(TEXT("GoToBuild"));
				if (Bits & UnitTagBits::GoToBase)           Names.Add(TEXT("GoToBase"));
				if (Bits & UnitTagBits::PatrolIdle)         Names.Add(TEXT("PatrolIdle"));
				if (Bits & UnitTagBits::PatrolRandom)       Names.Add(TEXT("PatrolRandom"));
				if (Bits & UnitTagBits::Patrol)             Names.Add(TEXT("Patrol"));
				if (Bits & UnitTagBits::YawFollow)          Names.Add(TEXT("YawFollow"));
				if (Bits & UnitTagBits::Pause)              Names.Add(TEXT("Pause"));
				if (Bits & UnitTagBits::Evasion)            Names.Add(TEXT("Evasion"));
				if (Bits & UnitTagBits::Detect)             Names.Add(TEXT("Detect"));
				if (Bits & UnitTagBits::StopMovement)       Names.Add(TEXT("StopMovement"));
				if (Bits & UnitTagBits::DisableObstacle)    Names.Add(TEXT("DisableObstacle"));
				if (Bits & UnitTagBits::RunAnimation)       Names.Add(TEXT("RunAnimation"));
				if (Names.Num() == 0)
				{
					return TEXT("<none>");
				}
				FString Out;
				for (int32 i=0;i<Names.Num();++i)
				{
					if (i>0) Out += TEXT(", ");
					Out += Names[i];
				}
				return Out;
			}
		}

UServerReplicationKickProcessor::UServerReplicationKickProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
}

void UServerReplicationKickProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassNetworkIDFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FRunAnimationFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassVisualEffectFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FEffectAreaImpactFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	// Do NOT require FMassReplicationSharedFragment here. We want to include entities that are missing it,
	// so the replicator fallback below can still process them and populate the bubble.
	EntityQuery.RegisterWithProcessor(*this);

	StartupFreezeQuery.Initialize(EntityManager);
	StartupFreezeQuery.AddTagRequirement<FMassStateFrozenTag>(EMassFragmentPresence::All);
	StartupFreezeQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	StartupFreezeQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
	StartupFreezeQuery.RegisterWithProcessor(*this);

	InitialKickQuery.Initialize(EntityManager);
	InitialKickQuery.AddTagRequirement<FMassStateNeedsInitialKickTag>(EMassFragmentPresence::All);
	InitialKickQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	InitialKickQuery.RegisterWithProcessor(*this);
}

void UServerReplicationKickProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{

	// Siehe mass_scopes: macht diesen Prozessor als Spalte Exclusive/UServerReplicationKickProcessor im CSV sichtbar.
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(UServerReplicationKickProcessor);

	if (bSkipReplication) return;

	TimeSinceLastRun += Context.GetDeltaTimeSeconds();
	if (TimeSinceLastRun < ExecutionInterval)
	{
		return;
	}
	TimeSinceLastRun = 0.f;

	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	
	TArray<AUnitBase*> UnitsToKick;

	// Handle global startup freeze release
	if (AResourceGameState* GS = World->GetGameState<AResourceGameState>())
	{
		if (World->GetNetMode() != NM_Client && !GS->bStartupFreezeReleased)
		{
			// If MatchStartTime is not set (<= 0), we treat time as reached immediately.
			// Default is -1.f, but also check for 0.f just in case.
			const bool bTimeReached = (GS->MatchStartTime <= 0.01f) || (World->GetTimeSeconds() >= GS->MatchStartTime);

			if (bTimeReached)
			{
				bool bCanRelease = false;
				
				// In Standalone or with no clients, we can release immediately when time is reached.
				// Otherwise, wait for all units to be registered to ensure client sync.
				// Note: GetNumPlayerControllers() might be 0 very early, which also qualifies as 'no clients yet'.
				if (World->GetNetMode() == NM_Standalone || World->GetNetMode() == NM_ListenServer && World->GetNumPlayerControllers() <= 1)
				{
					bCanRelease = true;
				}
				else if (World->GetNetMode() == NM_DedicatedServer || World->GetNumPlayerControllers() > 1)
				{
					if (AUnitRegistryReplicator* Reg = AUnitRegistryReplicator::GetOrSpawn(*World))
					{
						if (Reg->AreAllUnitsRegistered())
						{
							bCanRelease = true;
						}
					}
				}
				else
				{
					// Fallback for any other case (e.g. early ListenServer)
					bCanRelease = true;
				}
				
				if (bCanRelease)
				{
					StartupFreezeQuery.ForEachEntityChunk(Context, [&EntityManager, &UnitsToKick](FMassExecutionContext& FreezeCtx)
					{
						const int32 Num = FreezeCtx.GetNumEntities();
						TArrayView<FMassActorFragment> ActorList = FreezeCtx.GetMutableFragmentView<FMassActorFragment>();
						for (int32 i = 0; i < Num; ++i)
						{
							FMassEntityHandle Entity = FreezeCtx.GetEntity(i);
							EntityManager.Defer().RemoveTag<FMassStateFrozenTag>(Entity);
							EntityManager.Defer().RemoveTag<FMassStateNeedsInitialKickTag>(Entity);
							
							if (AUnitBase* Unit = Cast<AUnitBase>(ActorList[i].GetMutable()))
							{
								UnitsToKick.Add(Unit);
								if (Unit->MassActorBindingComponent && !Unit->MassActorBindingComponent->StopSeparation && !Cast<AConstructionUnit>(Unit))
								{
									EntityManager.Defer().RemoveTag<FMassStateStopSeparationTag>(Entity);
								}
							}
							else
							{
								EntityManager.Defer().RemoveTag<FMassStateStopSeparationTag>(Entity);
							}
						}
					});

					GS->bStartupFreezeReleased = true;
					UE_LOG(LogTemp, Log, TEXT("ServerKick: Startup freeze released for all units (MatchStartTime reached or skipped. Mode=%d, PC=%d, MatchStartTime=%.2f)."), 
						(int32)World->GetNetMode(), World->GetNumPlayerControllers(), GS->MatchStartTime);
				}
			}
		}

		// Process newly spawned units or units waiting for startup release
		if (GS->bStartupFreezeReleased)
		{
			// Handle units that were frozen but are now released (mostly server/standalone)
			StartupFreezeQuery.ForEachEntityChunk(Context, [&EntityManager, &UnitsToKick](FMassExecutionContext& FreezeCtx)
			{
				const int32 Num = FreezeCtx.GetNumEntities();
				TArrayView<FMassActorFragment> ActorList = FreezeCtx.GetMutableFragmentView<FMassActorFragment>();
				for (int32 i = 0; i < Num; ++i)
				{
					FMassEntityHandle Entity = FreezeCtx.GetEntity(i);
					EntityManager.Defer().RemoveTag<FMassStateFrozenTag>(Entity);
					EntityManager.Defer().RemoveTag<FMassStateNeedsInitialKickTag>(Entity);
					
					if (AUnitBase* Unit = Cast<AUnitBase>(ActorList[i].GetMutable()))
					{
						UnitsToKick.Add(Unit);
						if (Unit->MassActorBindingComponent && !Unit->MassActorBindingComponent->StopSeparation && !Cast<AConstructionUnit>(Unit))
						{
							EntityManager.Defer().RemoveTag<FMassStateStopSeparationTag>(Entity);
						}
					}
					else
					{
						EntityManager.Defer().RemoveTag<FMassStateStopSeparationTag>(Entity);
					}
				}
			});

			// Handle newly spawned units (Server and Client)
			InitialKickQuery.ForEachEntityChunk(Context, [&EntityManager, &UnitsToKick](FMassExecutionContext& KickCtx)
			{
				const int32 Num = KickCtx.GetNumEntities();
				TArrayView<FMassActorFragment> ActorList = KickCtx.GetMutableFragmentView<FMassActorFragment>();
				for (int32 i = 0; i < Num; ++i)
				{
					FMassEntityHandle Entity = KickCtx.GetEntity(i);
					//EntityManager.Defer().RemoveTag<FMassStateNeedsInitialKickTag>(Entity);

					if (AUnitBase* Unit = Cast<AUnitBase>(ActorList[i].GetMutable()))
					{
						UnitsToKick.Add(Unit);
					}
				}
			});
		}
	}
	// Perform the synchronization kick for all collected units
	if (UnitsToKick.Num() > 0)
	{
		const int32 MaxBatchSize = 200;
		for (int32 i = 0; i < UnitsToKick.Num(); i += MaxBatchSize)
		{
			int32 CurrentBatchNum = FMath::Min(MaxBatchSize, UnitsToKick.Num() - i);
			TArray<AUnitBase*> BatchUnits;
			BatchUnits.Reserve(CurrentBatchNum);

			for (int32 j = 0; j < CurrentBatchNum; ++j)
			{
				BatchUnits.Add(UnitsToKick[i + j]);
			}

			ACustomControllerBase* AnyController = nullptr;
			for (TActorIterator<ACustomControllerBase> It(World); It; ++It)
			{
				AnyController = *It;
				break;
			}

			if (AnyController)
			{
				// Use Defer().PushCommand to safely call the controller outside of the Mass processing phase
				Context.Defer().PushCommand<FMassDeferredSetCommand>([AnyController, BatchUnits](FMassEntityManager& EM)
				{
					if (IsValid(AnyController))
					{
						AnyController->Batch_KickUnits(BatchUnits);
					}
				});
			}
		}
	}
	// Respect global replication mode: only run in custom Mass mode
	if (CVarRTS_ServerKick_Enable.GetValueOnGameThread() == 0)
	{
		return; // disabled via CVAR
	}
	EnsureServerKickCleanupRegistered();
	// Ensure registry actor exists; bubble class registration is handled in URTSWorldCacheSubsystem::Initialize
	AUnitRegistryReplicator::GetOrSpawn(*World);

	UMassLODSubsystem* LODSub = World->GetSubsystem<UMassLODSubsystem>();
	UMassReplicationSubsystem* RepSub = World->GetSubsystem<UMassReplicationSubsystem>();
	// Safety: ensure bubble class is registered to avoid GetBubbleInfoClassHandle errors on both server and client worlds
	RTSReplicationBootstrap::RegisterForWorld(*World);
	if (!LODSub || !RepSub)
	{
		if (CVarRTS_ServerKick_LogLevel.GetValueOnGameThread() >= 2)
		{
			UE_LOG(LogTemp, Log, TEXT("ServerKick: Waiting for subsystems (LOD=%p Rep=%p)"), LODSub, RepSub);
		}
		return;
	}

	// First-time init log when subsystems become available (throttled by world)
	static TSet<const UWorld*> GLoggedInit;
	if (!GLoggedInit.Contains(World))
	{
		if (CVarRTS_ServerKick_LogLevel.GetValueOnGameThread() >= 1)
		{
			//UE_LOG(LogTemp, Log, TEXT("ServerKick: Init in world %s. LODSub=%p RepSub=%p"), *World->GetName(), LODSub, RepSub);
		}
		GLoggedInit.Add(World);
	}

	// Per-world startup grace window setup
	static TMap<const UWorld*, double> GGraceEndByWorld;
	static TSet<const UWorld*> GLoggedGraceEnd;
	const double Now = World->GetTimeSeconds();
	double* GraceEndPtr = GGraceEndByWorld.Find(World);
	if (!GraceEndPtr)
	{
		const float GraceSeconds = CVarRTSUnitStartupRepGraceSeconds.GetValueOnGameThread();
		GGraceEndByWorld.Add(World, Now + GraceSeconds);
		GraceEndPtr = GGraceEndByWorld.Find(World);
		if (CVarRTS_ServerKick_LogLevel.GetValueOnGameThread() >= 1)
		{
			UE_LOG(LogTemp, Log, TEXT("ServerKick: Startup replication grace enabled for %.2fs (world=%s)"), GraceSeconds, *World->GetName());
		}
	}
	const bool bInGrace = (GraceEndPtr && Now < *GraceEndPtr);
	if (!bInGrace && !GLoggedGraceEnd.Contains(World))
	{
		if (CVarRTS_ServerKick_LogLevel.GetValueOnGameThread() >= 1)
		{
			//UE_LOG(LogTemp, Log, TEXT("ServerKick: Startup replication grace ended (world=%s)"), *World->GetName());
		}
		GLoggedGraceEnd.Add(World);
	}

	// Detect missing units vs replication query; optionally re-register on server (guarded by CVAR)
	{
		int32 QueryCount = 0;
		int32 ChunkCount = 0;
		EntityQuery.ForEachEntityChunk(Context, [&QueryCount, &ChunkCount](FMassExecutionContext& ChunkContext)
		{
			QueryCount += ChunkContext.GetNumEntities();
			++ChunkCount;
		});
		// Verbose diagnostic: total eligible entities across all chunks for this tick
		if (CVarRTS_ServerKick_LogLevel.GetValueOnGameThread() >= 2)
		{
			UE_LOG(LogTemp, Log, TEXT("ServerKick: Query eligible entities this tick: Total=%d across %d chunks"), QueryCount, ChunkCount);
		}

		// Only attempt legacy re-registration when explicitly enabled
		if (CVarRTS_ServerKick_ReRegisterMissing.GetValueOnGameThread() != 0)
		{
			int32 LiveCount = 0;
			TSet<int32> LiveIndices;
			TSet<FName> LiveOwners;
			for (TActorIterator<AUnitBase> It(World); It; ++It)
			{
				AUnitBase* Unit = *It;
				if (!IsValid(Unit)) { continue; }
				++LiveCount;
				LiveOwners.Add(Unit->GetFName());
				if (Unit->UnitIndex != INDEX_NONE)
				{
					LiveIndices.Add(Unit->UnitIndex);
				}
			}

			AUnitRegistryReplicator* Reg = AUnitRegistryReplicator::GetOrSpawn(*World);
			if (Reg)
			{
				TSet<int32> RegIndices;
				TSet<FName> RegOwners;
				RegIndices.Reserve(Reg->Registry.Items.Num());
				RegOwners.Reserve(Reg->Registry.Items.Num());
				for (const FUnitRegistryItem& Itm : Reg->Registry.Items)
				{
					if (Itm.UnitIndex != INDEX_NONE) { RegIndices.Add(Itm.UnitIndex); }
					if (Itm.OwnerName != NAME_None) { RegOwners.Add(Itm.OwnerName); }
				}

				if (LiveCount > QueryCount)
				{
					if (CVarRTS_ServerKick_LogLevel.GetValueOnGameThread() >= 1)
					{
						UE_LOG(LogTemp, Warning, TEXT("ServerKick: Live Units (%d) exceed ReplicationQuery count (%d). Attempting re-register of missing units."), LiveCount, QueryCount);
					}
					UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
					FMassEntityManager* EM = EntitySubsystem ? &EntitySubsystem->GetMutableEntityManager() : nullptr;
					int32 Inserted = 0;
					for (TActorIterator<AUnitBase> It2(World); It2; ++It2)
					{
						AUnitBase* Unit = *It2;
						if (!IsValid(Unit)) { continue; }
						// Consider unit missing if either UnitIndex is not registered OR OwnerName is not registered.
						const bool bMissing = (!RegIndices.Contains(Unit->UnitIndex)) || (!RegOwners.Contains(Unit->GetFName()));
						if (!bMissing)
						{
							continue;
						}
						
						FMassNetworkID NetIDValue;
						bool bHaveNetID = false;
						if (EM)
						{
							if (UMassActorBindingComponent* Bind = Unit->FindComponentByClass<UMassActorBindingComponent>())
							{
								const FMassEntityHandle EHandle = Bind->GetMassEntityHandle();
								if (EHandle.IsSet() && EM->IsEntityActive(EHandle))
								{
									if (FMassNetworkIDFragment* NetFrag = TryGetFragmentDataPtrMutable<FMassNetworkIDFragment>(*EM, EHandle))
									{
										if (NetFrag->NetID.GetValue() != 0)
										{
											NetIDValue = NetFrag->NetID;
											bHaveNetID = true;
										}
										else
										{
											NetIDValue = FMassNetworkID(Reg->GetNextNetID());
											NetFrag->NetID = NetIDValue;
											bHaveNetID = true;
										}
									}
									else
									{
										// Add missing NetID fragment on-the-fly so the entity becomes eligible for replication queries
										NetIDValue = FMassNetworkID(Reg->GetNextNetID());
										EM->AddFragmentToEntity(EHandle, FMassNetworkIDFragment::StaticStruct());
										// Re-fetch using TryGetFragmentDataPtrMutable (EM->AddFragmentToEntity is immediate here)
										if (FMassNetworkIDFragment* NewFragPtr = TryGetFragmentDataPtrMutable<FMassNetworkIDFragment>(*EM, EHandle))
										{
											NewFragPtr->NetID = NetIDValue;
											bHaveNetID = true;
										}
										if (CVarRTS_ServerKick_LogLevel.GetValueOnGameThread() >= 1)
										{
											UE_LOG(LogTemp, Warning, TEXT("ServerKick: Added FMassNetworkIDFragment for %s with NetID=%u"), *Unit->GetName(), NetIDValue.GetValue());
										}
									}
								}
							}
						}
						if (!bHaveNetID)
						{
							NetIDValue = FMassNetworkID(Reg->GetNextNetID());
						}
						const int32 NewIdx = Reg->Registry.Items.AddDefaulted();
						Reg->Registry.Items[NewIdx].OwnerName = Unit->GetFName();
						Reg->Registry.Items[NewIdx].UnitIndex = Unit->UnitIndex;
						Reg->Registry.Items[NewIdx].NetID = NetIDValue;
						Reg->Registry.MarkItemDirty(Reg->Registry.Items[NewIdx]);
						Inserted++;
					}
					if (Inserted > 0)
					{
						Reg->Registry.MarkArrayDirty();
						Reg->ForceNetUpdate();
						if (CVarRTS_ServerKick_LogLevel.GetValueOnGameThread() >= 1)
						{
							UE_LOG(LogTemp, Warning, TEXT("ServerKick: Inserted %d missing units into UnitRegistry (world=%s)"), Inserted, *World->GetName());
						}
					}
				}
			}
		}
		else if (CVarRTS_ServerKick_LogLevel.GetValueOnGameThread() >= 2)
		{
			UE_LOG(LogTemp, Log, TEXT("ServerKick: Re-register missing is disabled (net.RTS.ServerReplicationKick.ReRegisterMissing=0)"));
		}
	}

	// Budget je Durchlauf (10 Hz): zaehlt nur Einheiten, die sich seit dem letzten Senden geaendert haben.
	// Gemessen 07.10.2026: vorher verbrauchten stehende Einheiten das Budget mit (ganze 64er-Scheiben,
	// geaendert oder nicht) - fahrende kamen beim Client nur alle 0,3-0,7 s an, einzelne erst nach 5 s.
	const int32 MaxPerTick = FMath::Max(1, CVarRTS_ServerKick_MaxPerTick.GetValueOnGameThread());
	int32 ProcessedThisTick = 0;

	// Faire Reihenfolge: ging das Budget mitten in einem Chunk aus, beginnt der naechste Durchlauf dort.
	int32 TotalChunksThisTick = 0;
	EntityQuery.ForEachEntityChunk(Context, [&TotalChunksThisTick](FMassExecutionContext&){ ++TotalChunksThisTick; });
	static TMap<const UWorld*, int32> GChunkStartIndexByWorld;
	int32& ChunkStartIndex = GChunkStartIndexByWorld.FindOrAdd(World);
	if (ChunkStartIndex < 0 || ChunkStartIndex >= FMath::Max(1, TotalChunksThisTick))
	{
		ChunkStartIndex = 0;
	}
	int32 ChunkWhereBudgetRanOut = INDEX_NONE;

	// Schwellen wie im MassUnitReplicatorBase
	float LocThresh = 10.0f;
	float AngleThresh = 5.0f;
	float ScaleThresh = 0.02f;
	if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("net.RTS.ServerRep.LocThresholdCm"))) LocThresh = Var->GetFloat();
	if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("net.RTS.ServerRep.AngleThresholdDeg"))) AngleThresh = Var->GetFloat();
	if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("net.RTS.ServerRep.ScaleThreshold"))) ScaleThresh = Var->GetFloat();

	// Ohne Aenderungspruefung alles senden (Startphase oder per CVar erzwungen); das Budget gilt trotzdem.
	const bool bSendAll = (CVarRTS_ServerKick_ProcessCleanChunks.GetValueOnGameThread() != 0) || bInGrace;
	const int32 KickLogLevel = CVarRTS_ServerKick_LogLevel.GetValueOnGameThread();
	const float RefreshSeconds = CVarRTS_ServerKick_RefreshSeconds.GetValueOnGameThread();
	const int32 RefreshMaxPerTick = CVarRTS_ServerKick_RefreshMaxPerTick.GetValueOnGameThread();
	int32 RefreshedThisTick = 0;

	auto QuantizeAngle = [](float AngleDeg)->uint16
	{
		const float Norm = FMath::Fmod(AngleDeg + 360.0f, 360.0f);
		return static_cast<uint16>(FMath::RoundToInt((Norm / 360.0f) * 65535.0f));
	};

	auto ProcessChunk = [&](FMassExecutionContext& ChunkContext, int32 ChunkOrdinal)
	{
		const int32 Num = ChunkContext.GetNumEntities();
		if (Num <= 0 || ProcessedThisTick >= MaxPerTick)
		{
			return;
		}

		// Use a fallback replicator instance since some entities may lack FMassReplicationSharedFragment
		static TMap<const UWorld*, TWeakObjectPtr<UMassUnitReplicatorBase>> GReplicatorPerWorld;
		UMassUnitReplicatorBase* Replicator = nullptr;
		if (TWeakObjectPtr<UMassUnitReplicatorBase>* Found = GReplicatorPerWorld.Find(World))
		{
			Replicator = Found->Get();
		}
		if (!Replicator)
		{
			Replicator = NewObject<UMassUnitReplicatorBase>((UObject*)GetTransientPackage(), UMassUnitReplicatorBase::StaticClass());
			if (Replicator)
			{
				Replicator->AddToRoot(); // keep alive for world lifetime to avoid GC
			}
			GReplicatorPerWorld.Add(World, Replicator);
		}
		FMassReplicationContext RepCtx(*World, *LODSub, *RepSub);

		const TConstArrayView<FMassNetworkIDFragment> NetIDs = ChunkContext.GetFragmentView<FMassNetworkIDFragment>();
		const TConstArrayView<FTransformFragment> Transforms = ChunkContext.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassAIStateFragment> AIStates = ChunkContext.GetFragmentView<FMassAIStateFragment>();
		const TConstArrayView<FMassAITargetFragment> AITargets = ChunkContext.GetFragmentView<FMassAITargetFragment>();
		const TConstArrayView<FMassCombatStatsFragment> CombatStats = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
		const TConstArrayView<FMassMoveTargetFragment> MoveTargets = ChunkContext.GetFragmentView<FMassMoveTargetFragment>();
		const TConstArrayView<FMassAgentCharacteristicsFragment> Characteristics = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
		const TConstArrayView<FRunAnimationFragment> RunAnims = ChunkContext.GetFragmentView<FRunAnimationFragment>();
		const TConstArrayView<FMassVisualEffectFragment> VisualEffects = ChunkContext.GetFragmentView<FMassVisualEffectFragment>();
		const TConstArrayView<FEffectAreaImpactFragment> Impacts = ChunkContext.GetFragmentView<FEffectAreaImpactFragment>();

		// Tags gelten je Archetyp, also fuer den ganzen Chunk gleich: einmal statt je Einheit.
		const uint32 ChunkTagBits = BuildReplicatedTagBits(EntityManager, ChunkContext.GetEntity(0));

		// Stabiler Schluessel je Chunk aus einigen NetIDs, fuer den Lesezeiger ueber Durchlaeufe hinweg.
		uint64 ChunkKey = 0x51ED27u;
		auto AccHash = [&ChunkKey](uint32 V)
		{
			ChunkKey ^= static_cast<uint64>(V) + 0x9e3779b97f4a7c15ull + (ChunkKey << 6) + (ChunkKey >> 2);
		};
		AccHash(NetIDs[0].NetID.GetValue());
		AccHash(NetIDs[Num / 3].NetID.GetValue());
		AccHash(NetIDs[(2 * Num) / 3].NetID.GetValue());
		AccHash(NetIDs[Num - 1].NetID.GetValue());
		int32& Cursor = GStartOffsetByChunk.FindOrAdd(ChunkKey);
		if (Cursor < 0 || Cursor >= Num)
		{
			Cursor = 0;
		}

		TArray<int32> DirtyIndices;
		TArray<FSig, TInlineAllocator<64>> DirtySigs;
		int32 Scanned = 0;
		for (; Scanned < Num && ProcessedThisTick + DirtyIndices.Num() < MaxPerTick; ++Scanned)
		{
			const int32 i = (Cursor + Scanned) % Num;
			const FTransform& Xf = Transforms[i].GetTransform();
			const FRotator Rot = Xf.Rotator();

			FSig S;
			S.Loc = Xf.GetLocation();
			S.P = QuantizeAngle(Rot.Pitch);
			S.Y = QuantizeAngle(Rot.Yaw);
			S.R = QuantizeAngle(Rot.Roll);
			S.Scale = Xf.GetScale3D();
			S.TagBits = ChunkTagBits;
			if (AIStates.Num() > 0)
			{
				S.FireCounter = AIStates[i].ProjectileFireCounter;
				S.TargetNetID = AIStates[i].LastTargetNetID;
			}
			if (AITargets.Num() > 0)
			{
				if (const FMassNetworkIDFragment* TargetNetIDFrag = TryGetFragmentDataPtr<FMassNetworkIDFragment>(EntityManager, AITargets[i].TargetEntity))
				{
					S.TargetNetID = TargetNetIDFrag->NetID.GetValue();
				}
			}
			if (CombatStats.Num() > 0)
			{
				S.Health = CombatStats[i].Health;
				S.Shield = CombatStats[i].Shield;
			}
			if (MoveTargets.Num() > 0)
			{
				S.MoveCenter = MoveTargets[i].Center;
				S.MoveSpeed = MoveTargets[i].DesiredSpeed.Get();
				S.MoveActionID = MoveTargets[i].GetCurrentActionID();
			}

			// Dieselben Quellen wie UpdateReplicationBits/PackedBits im Replikator, nur als Vergleichswert.
			uint32 Bits = 0u;
			int32 Bit = 0;
			auto Push = [&Bits, &Bit](bool bValue) { if (bValue) { Bits |= (1u << Bit); } ++Bit; };
			if (CombatStats.Num() > 0)
			{
				const FMassCombatStatsFragment& CS = CombatStats[i];
				Push(CS.IsInitialized); Push(CS.bUseProjectile); Push(CS.bCanMoveWhileAttacking); Push(CS.bRotatesToMovementIfMoveWhileAttacking);
			}
			Bit = 4;
			if (Characteristics.Num() > 0)
			{
				const FMassAgentCharacteristicsFragment& AC = Characteristics[i];
				Push(AC.bIsFlying); Push(AC.bIsInvisible); Push(AC.bCanOnlyAttackFlying); Push(AC.bCanOnlyAttackGround);
				Push(AC.bCanBeInvisible); Push(AC.bCanDetectInvisible); Push(AC.RotatesToMovement); Push(AC.RotatesToEnemy);
			}
			Bit = 12;
			if (AIStates.Num() > 0)
			{
				const FMassAIStateFragment& AIS = AIStates[i];
				Push(AIS.CanAttack); Push(AIS.CanMove); Push(AIS.HoldPosition); Push(AIS.HasAttacked);
				Push(AIS.SwitchingState); Push(AIS.IsInitialized); Push(AIS.LastbFollowTarget);
				Push(AIS.LastProjectileClass != nullptr);
			}
			Bit = 20;
			if (AITargets.Num() > 0)
			{
				const FMassAITargetFragment& AIT = AITargets[i];
				Push(AIT.bHasValidTarget); Push(AIT.IsFocusedOnTarget);
				S.AbilityLoc = FVector(AIT.AbilityTargetLocation);
			}
			Bit = 22;
			if (VisualEffects.Num() > 0)
			{
				const FMassVisualEffectFragment& VE = VisualEffects[i];
				Push(VE.bPulsateEnabled); Push(VE.bRotationEnabled); Push(VE.bOscillationEnabled);
			}
			Bit = 25;
			if (Impacts.Num() > 0)
			{
				const FEffectAreaImpactFragment& EA = Impacts[i];
				Push(EA.bImpactVFXTriggered); Push(EA.bIsScalingAfterImpact); Push(EA.bImpactScaleTriggered); Push(EA.bPendingDestruction);
			}
			if (RunAnims.Num() > 0)
			{
				// Dauer quantisiert wie im Replikator, Animationszustand darueber
				S.RunAnimData = static_cast<uint32>(FMath::Clamp(RunAnims[i].Duration * 100.f, 0.f, 65535.f))
					| (static_cast<uint32>(RunAnims[i].AnimationState.GetValue()) << 16);
			}
			S.StateBits = Bits;

			const FSig* Prev = GLastSigByID.Find(NetIDs[i].NetID.GetValue());
			bool bDirty = bSendAll || !Prev || !S.IsNearlyEqual(*Prev, LocThresh, AngleThresh, ScaleThresh);
			// Sicherheitsnetz: lange nicht gepruefte Einheit einmal durch den Replikator schicken.
			if (!bDirty && RefreshSeconds > 0.f && RefreshedThisTick < RefreshMaxPerTick && (Now - Prev->SentTime) >= RefreshSeconds)
			{
				bDirty = true;
				++RefreshedThisTick;
			}
			if (bDirty)
			{
				S.SentTime = Now;
				DirtyIndices.Add(i);
				DirtySigs.Add(S);
			}
		}

		// Naechster Durchlauf beginnt hinter der zuletzt geprueften Einheit: was diesmal nicht mehr ins
		// Budget passte, kommt dann zuerst dran.
		Cursor = (Cursor + Scanned) % Num;
		if (Scanned < Num)
		{
			ChunkWhereBudgetRanOut = ChunkOrdinal;
		}

		if (DirtyIndices.Num() == 0)
		{
			return;
		}
		ProcessedThisTick += DirtyIndices.Num();

		if (KickLogLevel >= 2)
		{
			const int32 MaxLog = FMath::Min(20, DirtyIndices.Num());
			FString IdList;
			for (int32 k = 0; k < MaxLog; ++k)
			{
				if (k > 0) { IdList += TEXT(", "); }
				IdList += FString::Printf(TEXT("%u"), NetIDs[DirtyIndices[k]].NetID.GetValue());
			}
			UE_LOG(LogTemp, Log, TEXT("ServerReplicationKick: chunk %d entities, scanned %d, changed %d (budget used %d/%d). NetIDs: %s%s"),
				Num, Scanned, DirtyIndices.Num(), ProcessedThisTick, MaxPerTick, *IdList, (DirtyIndices.Num() > MaxLog ? TEXT(" ...") : TEXT("")));
			UE_LOG(LogTemp, Log, TEXT("ServerReplicationKick: chunk TagBits=0x%08x Tags=[%s]"), ChunkTagBits, *StringifyUnitTagBits(ChunkTagBits));
		}

		// Nur die geaenderten Einheiten an den Replikator geben.
		ReplicationSliceControl::SetIndices(&DirtyIndices);
		Replicator->ProcessClientReplication(ChunkContext, RepCtx);
		ReplicationSliceControl::ClearSlice();

		// Gesendeten Stand merken
		for (int32 k = 0; k < DirtyIndices.Num(); ++k)
		{
			GLastSigByID.FindOrAdd(NetIDs[DirtyIndices[k]].NetID.GetValue()) = DirtySigs[k];
		}
	};

	// Ab ChunkStartIndex bis zum Ende, dann von vorn bis ChunkStartIndex.
	int32 ChunkOrdinal = 0;
	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const int32 Ordinal = ChunkOrdinal++;
		if (Ordinal >= ChunkStartIndex) { ProcessChunk(ChunkContext, Ordinal); }
	});
	if (ChunkStartIndex > 0 && ProcessedThisTick < MaxPerTick)
	{
		int32 ChunkOrdinal2 = 0;
		EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
		{
			const int32 Ordinal = ChunkOrdinal2++;
			if (Ordinal < ChunkStartIndex) { ProcessChunk(ChunkContext, Ordinal); }
		});
	}

	if (ChunkWhereBudgetRanOut != INDEX_NONE)
	{
		ChunkStartIndex = ChunkWhereBudgetRanOut;
	}
}
