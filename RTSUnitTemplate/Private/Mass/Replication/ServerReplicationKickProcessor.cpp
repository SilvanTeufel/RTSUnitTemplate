/*
#if RTSUNITTEMPLATE_NO_LOGS
#undef UE_LOG
#define UE_LOG(CategoryName, Verbosity, Format, ...) ((void)0)
#endif
*/
#include "Mass/Replication/ServerReplicationKickProcessor.h"

#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
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
static TAutoConsoleVariable<int32> CVarRTS_ServerKick_MaxPerChunk(
	TEXT("net.RTS.ServerReplicationKick.MaxPerChunk"),
	400,
	TEXT("Max entities allowed per chunk this tick; chunks exceeding are deferred. Default 400."),
	ECVF_Default);
static TAutoConsoleVariable<int32> CVarRTS_ServerKick_MaxPerTick(
		TEXT("net.RTS.ServerReplicationKick.MaxPerTick"),
		400,
		TEXT("Max total entities processed by this processor per tick; extra chunks are skipped. Default 400 for higher concurrency."),
		ECVF_Default);
static TAutoConsoleVariable<int32> CVarRTS_ServerKick_LogLevel(
	TEXT("net.RTS.ServerReplicationKick.LogLevel"),
	1,
	TEXT("Logging level: 0=Off, 1=Warn, 2=Verbose."),
	ECVF_Default);

// File-scope signature structure and storage so we can clear on world teardown
namespace {
	struct FSig
	{
		FVector Loc; uint16 P=0,Y=0,R=0; FVector Scale;
		bool operator==(const FSig& O) const
		{
			return Loc.Equals(O.Loc, 0.1f) && P==O.P && Y==O.Y && R==O.R && Scale.Equals(O.Scale, 0.01f);
		}
	};
	static TMap<uint32, FSig> GLastSigByID; // server-only, cleared on world cleanup
	static TMap<const UWorld*, int32> GProcessedCountByWorld;
	static TMap<const UWorld*, double> GLastExecTimeByWorld;
	static bool GCleanupRegistered = false;
	static FDelegateHandle GCleanupHandle;
	static void EnsureServerKickCleanupRegistered()
	{
		if (!GCleanupRegistered)
		{
			GCleanupHandle = FWorldDelegates::OnWorldCleanup.AddStatic([](UWorld* InWorld, bool, bool){
				GLastSigByID.Reset();
				GProcessedCountByWorld.Remove(InWorld);
				GLastExecTimeByWorld.Remove(InWorld);
			});
			GCleanupRegistered = true;
		}
	}
}

UServerReplicationKickProcessor::UServerReplicationKickProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::Server;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
}

void UServerReplicationKickProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassNetworkIDFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddSharedRequirement<FMassReplicationSharedFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.RegisterWithProcessor(*this);
}

void UServerReplicationKickProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	// Respect global replication mode: only run in custom Mass mode
	if (RTSReplicationSettings::GetReplicationMode() != RTSReplicationSettings::Mass)
	{
		return;
	}
	if (CVarRTS_ServerKick_Enable.GetValueOnGameThread() == 0)
	{
		return; // disabled via CVAR
	}
	EnsureServerKickCleanupRegistered();
	// Ensure registry actor exists; bubble class registration is handled in URTSWorldCacheSubsystem::Initialize
	AUnitRegistryReplicator::GetOrSpawn(*World);

	UMassLODSubsystem* LODSub = World->GetSubsystem<UMassLODSubsystem>();
	UMassReplicationSubsystem* RepSub = World->GetSubsystem<UMassReplicationSubsystem>();
	if (!LODSub || !RepSub)
	{
		if (CVarRTS_ServerKick_LogLevel.GetValueOnGameThread() >= 2)
		{
			UE_LOG(LogTemp, Verbose, TEXT("ServerKick: Waiting for subsystems (LOD=%p Rep=%p)"), LODSub, RepSub);
		}
		return;
	}

	// First-time init log when subsystems become available (throttled by world)
	static TSet<const UWorld*> GLoggedInit;
	if (!GLoggedInit.Contains(World))
	{
		if (CVarRTS_ServerKick_LogLevel.GetValueOnGameThread() >= 1)
		{
			UE_LOG(LogTemp, Log, TEXT("ServerKick: Init in world %s. LODSub=%p RepSub=%p"), *World->GetName(), LODSub, RepSub);
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
			UE_LOG(LogTemp, Log, TEXT("ServerKick: Startup replication grace ended (world=%s)"), *World->GetName());
		}
		GLoggedGraceEnd.Add(World);
	}

	// Per-tick budgeting across chunks
	int32& ProcessedThisTick = GProcessedCountByWorld.FindOrAdd(World);
	double& LastExecTime = GLastExecTimeByWorld.FindOrAdd(World);
	if (LastExecTime != Now)
	{
		LastExecTime = Now;
		ProcessedThisTick = 0;
	}
	const int32 MaxPerChunk = CVarRTS_ServerKick_MaxPerChunk.GetValueOnGameThread();
	const int32 MaxPerTick = CVarRTS_ServerKick_MaxPerTick.GetValueOnGameThread();

	EntityQuery.ForEachEntityChunk(Context, [World, LODSub, RepSub, bInGrace, MaxPerChunk, MaxPerTick, &ProcessedThisTick](FMassExecutionContext& ChunkContext)
	{
		const FMassReplicationSharedFragment& RepShared = ChunkContext.GetSharedFragment<FMassReplicationSharedFragment>();
		UMassReplicatorBase* Replicator = RepShared.CachedReplicator;
		if (!Replicator)
		{
			return;
		}
		FMassReplicationContext RepCtx(*World, *LODSub, *RepSub);

		// Build lightweight change signatures and skip clean chunks
		auto QuantizeAngle = [](float AngleDeg)->uint16
		{
			const float Norm = FMath::Fmod(AngleDeg + 360.0f, 360.0f);
			return static_cast<uint16>(FMath::RoundToInt((Norm / 360.0f) * 65535.0f));
		};

		const int32 Num = ChunkContext.GetNumEntities();
		const TConstArrayView<FMassNetworkIDFragment> NetIDs = ChunkContext.GetFragmentView<FMassNetworkIDFragment>();
		const TConstArrayView<FTransformFragment> Transforms = ChunkContext.GetFragmentView<FTransformFragment>();

		// Budget caps: skip heavy chunks or when per-tick cap reached
		if (ProcessedThisTick >= MaxPerTick || Num > MaxPerChunk || (ProcessedThisTick + Num) > MaxPerTick)
		{
			return; // defer this chunk to a later tick
		}

		bool bAnyChanged = false;
		// During startup grace period: do not skip replication based on signatures
		if (bInGrace)
		{
			bAnyChanged = true;
		}
		else
		{
			for (int32 i=0; i<Num; ++i)
			{
				const uint32 ID = NetIDs[i].NetID.GetValue();
				const FTransform& Xf = Transforms[i].GetTransform();
				FSig NewSig;
				NewSig.Loc = Xf.GetLocation();
				const FRotator Rot = Xf.Rotator();
				NewSig.P = QuantizeAngle(Rot.Pitch);
				NewSig.Y = QuantizeAngle(Rot.Yaw);
				NewSig.R = QuantizeAngle(Rot.Roll);
				NewSig.Scale = Xf.GetScale3D();
				const FSig* Old = GLastSigByID.Find(ID);
				if (!Old || !(*Old == NewSig))
				{
					bAnyChanged = true;
					break; // one change is enough to require replication for this chunk
				}
			}
		}

		if (!bAnyChanged)
		{
			// Skip invoking the (potentially heavy) replicator if nothing changed for this chunk
			return;
		}

		// Optionally log a small sample (verbose only)
		if (CVarRTS_ServerKick_LogLevel.GetValueOnGameThread() >= 2)
		{
			const int32 MaxLog = FMath::Min(20, Num);
			FString IdList;
			for (int32 i = 0; i < MaxLog; ++i)
			{
				if (i > 0) { IdList += TEXT(", "); }
				IdList += FString::Printf(TEXT("%u"), NetIDs[i].NetID.GetValue());
			}
			UE_LOG(LogTemp, Verbose, TEXT("ServerReplicationKick: %d entities. NetIDs[%d]: %s%s"),
				Num, MaxLog, *IdList, (Num > MaxLog ? TEXT(" ...") : TEXT("")));
		}

		// Invoke the same function the MassReplicationProcessor would call on the server.
		Replicator->ProcessClientReplication(ChunkContext, RepCtx);
		ProcessedThisTick += Num; // account budget usage

		// Update stored signatures after replication to reflect the latest sent state
		for (int32 i=0; i<Num; ++i)
		{
			const uint32 ID = NetIDs[i].NetID.GetValue();
			const FTransform& Xf = Transforms[i].GetTransform();
			FSig S;
			S.Loc = Xf.GetLocation();
			const FRotator Rot = Xf.Rotator();
			S.P = QuantizeAngle(Rot.Pitch);
			S.Y = QuantizeAngle(Rot.Yaw);
			S.R = QuantizeAngle(Rot.Roll);
			S.Scale = Xf.GetScale3D();
			GLastSigByID.FindOrAdd(ID) = S;
		}
	});
}
