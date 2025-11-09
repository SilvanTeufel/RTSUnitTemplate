

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
#include "Mass/UnitMassTag.h"
#include "MassEntitySubsystem.h"
#include "EngineUtils.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/MassActorBindingComponent.h"

// Forward-declare slice control API implemented in MassUnitReplicatorBase.cpp
namespace ReplicationSliceControl
{
	void SetSlice(int32 StartIndex, int32 Count);
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
static TAutoConsoleVariable<int32> CVarRTS_ServerKick_MaxPerChunk(
	TEXT("net.RTS.ServerReplicationKick.MaxPerChunk"),
	16,
	TEXT("Max entities allowed per chunk this tick; chunks exceeding are deferred. Default 16."),
	ECVF_Default);
static TAutoConsoleVariable<int32> CVarRTS_ServerKick_MaxPerTick(
		TEXT("net.RTS.ServerReplicationKick.MaxPerTick"),
		16,
		TEXT("Max total entities processed by this processor per tick; extra chunks are skipped. Default 16."),
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

// CVAR: Enforce full slices (Count == MaxPerChunk) when possible. If remaining budget is smaller,
// defer the chunk to the next tick to avoid short, jittery updates between ticks. Chunks with fewer
// than MaxPerChunk entities are processed with their actual size.
static TAutoConsoleVariable<int32> CVarRTS_ServerKick_EnforceFullSlices(
	TEXT("net.RTS.ServerReplicationKick.EnforceFullSlices"),
	1,
	TEXT("When 1, only process a chunk if at least MaxPerChunk budget remains (unless the chunk has < MaxPerChunk entities). This keeps slices consistently filled and avoids partial slices."),
	ECVF_Default);

// CVAR: Control the legacy server-side re-registration fallback. Default OFF now that UnitSignalingProcessor assists.
static TAutoConsoleVariable<int32> CVarRTS_ServerKick_ReRegisterMissing(
	TEXT("net.RTS.ServerReplicationKick.ReRegisterMissing"),
	0,
	TEXT("When 1, perform server-side recovery to re-register Units missing in the replication query by adding NetID and registry entries. Default 0 (disabled) because UnitSignalingProcessor assists."),
	ECVF_Default);

	// File-scope signature structure and storage so we can clear on world teardown
		namespace {
			struct FSig
			{
				FVector Loc; uint16 P=0,Y=0,R=0; FVector Scale; uint32 TagBits = 0u;
				bool operator==(const FSig& O) const
				{
					return Loc.Equals(O.Loc, 0.1f) && P==O.P && Y==O.Y && R==O.R && Scale.Equals(O.Scale, 0.01f) && TagBits == O.TagBits;
				}
			};
			static TMap<uint32, FSig> GLastSigByID; // server-only, cleared on world cleanup
			static TMap<const UWorld*, int32> GProcessedCountByWorld;
			static TMap<const UWorld*, double> GLastExecTimeByWorld;
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
						GProcessedCountByWorld.Remove(InWorld);
						GLastExecTimeByWorld.Remove(InWorld);
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
				if (Bits & UnitTagBits::Attack)             Names.Add(TEXT("Attack"));
				if (Bits & UnitTagBits::Chase)              Names.Add(TEXT("Chase"));
				if (Bits & UnitTagBits::Build)              Names.Add(TEXT("Build"));
				if (Bits & UnitTagBits::ResourceExtraction) Names.Add(TEXT("ResourceExtraction"));
				if (Bits & UnitTagBits::GoToResource)       Names.Add(TEXT("GoToResource"));
				if (Bits & UnitTagBits::GoToBuild)          Names.Add(TEXT("GoToBuild"));
				if (Bits & UnitTagBits::GoToBase)           Names.Add(TEXT("GoToBase"));
				if (Bits & UnitTagBits::PatrolIdle)         Names.Add(TEXT("PatrolIdle"));
				if (Bits & UnitTagBits::PatrolRandom)       Names.Add(TEXT("PatrolRandom"));
				if (Bits & UnitTagBits::Patrol)             Names.Add(TEXT("Patrol"));
				if (Bits & UnitTagBits::Run)                Names.Add(TEXT("Run"));
				if (Bits & UnitTagBits::Pause)              Names.Add(TEXT("Pause"));
				if (Bits & UnitTagBits::Evasion)            Names.Add(TEXT("Evasion"));
				if (Bits & UnitTagBits::Idle)               Names.Add(TEXT("Idle"));
				if (Bits & UnitTagBits::StopMovement)       Names.Add(TEXT("StopMovement"));
				if (Bits & UnitTagBits::DisableObstacle)    Names.Add(TEXT("DisableObstacle"));
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
	ExecutionFlags = (int32)EProcessorExecutionFlags::Server;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
}

void UServerReplicationKickProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassNetworkIDFragment>(EMassFragmentAccess::ReadOnly);
	// Do NOT require FMassReplicationSharedFragment here. We want to include entities that are missing it,
	// so the replicator fallback below can still process them and populate the bubble.
	EntityQuery.RegisterWithProcessor(*this);
}

void UServerReplicationKickProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
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
								if (EHandle.IsSet() && EM->IsEntityValid(EHandle))
								{
									if (FMassNetworkIDFragment* NetFrag = EM->GetFragmentDataPtr<FMassNetworkIDFragment>(EHandle))
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
										if (FMassNetworkIDFragment* NewFragPtr = EM->GetFragmentDataPtr<FMassNetworkIDFragment>(EHandle))
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

// Fair chunk scheduling: rotate starting chunk each tick to avoid starving the last chunks
int32 TotalChunksThisTick = 0;
EntityQuery.ForEachEntityChunk(Context, [&TotalChunksThisTick](FMassExecutionContext&){ ++TotalChunksThisTick; });
static TMap<const UWorld*, int32> GChunkStartIndexByWorld;
int32& ChunkStartIndex = GChunkStartIndexByWorld.FindOrAdd(World);
if (TotalChunksThisTick > 0)
{
	if (ChunkStartIndex >= TotalChunksThisTick || ChunkStartIndex < 0)
	{
		ChunkStartIndex = ChunkStartIndex % FMath::Max(1, TotalChunksThisTick);
	}
}

int32 ChunksUsedThisTick = 0;

// Shared per-chunk processing lambda (handles slice selection and replication)
auto ProcessChunk = [World, LODSub, RepSub, MaxPerChunk, MaxPerTick, &ProcessedThisTick, &EntityManager, &ChunksUsedThisTick](FMassExecutionContext& ChunkContext)
{
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

	// Build lightweight change signatures
	auto QuantizeAngle = [](float AngleDeg)->uint16
	{
		const float Norm = FMath::Fmod(AngleDeg + 360.0f, 360.0f);
		return static_cast<uint16>(FMath::RoundToInt((Norm / 360.0f) * 65535.0f));
	};

	const int32 Num = ChunkContext.GetNumEntities();
	const TConstArrayView<FMassNetworkIDFragment> NetIDs = ChunkContext.GetFragmentView<FMassNetworkIDFragment>();
	const TConstArrayView<FTransformFragment> Transforms = ChunkContext.GetFragmentView<FTransformFragment>();

	// If our budgets are low, process a slice of this chunk this tick and continue next tick.
	if (ProcessedThisTick >= MaxPerTick)
	{
		return; // budget used up this tick; try again next tick
	}
	const int32 RemainingBudget = MaxPerTick - ProcessedThisTick;

	// Decide slice size with optional enforcement of full slices
	const bool bEnforceFullSlices = (CVarRTS_ServerKick_EnforceFullSlices.GetValueOnGameThread() != 0) && (MaxPerTick >= MaxPerChunk);
	int32 SliceSize = 0;
	const int32 DesiredSliceSize = FMath::Min(MaxPerChunk, Num);
	if (bEnforceFullSlices && Num >= MaxPerChunk)
	{
		// Require enough global budget to produce a full slice for this chunk
		if (RemainingBudget < MaxPerChunk)
		{
			if (CVarRTS_ServerKick_LogLevel.GetValueOnGameThread() >= 2)
			{
				UE_LOG(LogTemp, Log, TEXT("ServerKick: Deferring chunk (Num=%d) this tick to keep full slice size=%d (RemainingBudget=%d < MaxPerChunk=%d)"),
					Num, MaxPerChunk, RemainingBudget, MaxPerChunk);
			}
			return; // try next chunk or next tick
		}
		SliceSize = MaxPerChunk;
	}
	else
	{
		// Either chunk is smaller than MaxPerChunk or enforcement disabled; use remaining budget
		SliceSize = FMath::Clamp(FMath::Min3(RemainingBudget, MaxPerChunk, Num), 0, Num);
	}

	// Build a more stable per-chunk key using a hash of NetIDs values and the replicator pointer
	uint64 ChunkKey = static_cast<uint64>(reinterpret_cast<uintptr_t>(Replicator));
	auto AccHash = [&ChunkKey](uint32 V)
	{
		// Mix function (similar to boost::hash_combine)
		ChunkKey ^= static_cast<uint64>(V) + 0x9e3779b97f4a7c15ull + (ChunkKey << 6) + (ChunkKey >> 2);
	};
	if (Num > 0)
	{
		const int32 I0 = 0;
		const int32 I1 = Num / 3;
		const int32 I2 = (2 * Num) / 3;
		const int32 I3 = Num - 1;
		AccHash(NetIDs[I0].NetID.GetValue());
		AccHash(NetIDs[I1].NetID.GetValue());
		AccHash(NetIDs[I2].NetID.GetValue());
		AccHash(NetIDs[I3].NetID.GetValue());
	}
	int32& StartOffset = GStartOffsetByChunk.FindOrAdd(ChunkKey);
	if (StartOffset >= Num)
	{
		StartOffset = 0;
	}
	const int32 SliceStart = StartOffset;
	// Advance start offset for next tick; wrap within this chunk size
	StartOffset = (StartOffset + SliceSize) % FMath::Max(1, Num);

	if (SliceSize <= 0)
	{
		return;
	}

	if (CVarRTS_ServerKick_LogLevel.GetValueOnGameThread() >= 2)
	{
		UE_LOG(LogTemp, Log, TEXT("ServerKick: Processing slice Start=%d Count=%d (ChunkEntities=%d Remain=%d MaxPerTick=%d MaxPerChunk=%d)"),
			SliceStart, SliceSize, Num, RemainingBudget, MaxPerTick, MaxPerChunk);
	}

	// Instruct replicator to only process the slice of this chunk
	ReplicationSliceControl::SetSlice(SliceStart, SliceSize);

	// Optionally log a small sample (verbose only)
	if (CVarRTS_ServerKick_LogLevel.GetValueOnGameThread() >= 2)
	{
		const int32 MaxLog = FMath::Min(20, SliceSize);
		FString IdList;
		for (int32 i = 0; i < MaxLog; ++i)
		{
			const int32 Idx = (SliceStart + i) % Num;
			if (i > 0) { IdList += TEXT(", "); }
			IdList += FString::Printf(TEXT("%u"), NetIDs[Idx].NetID.GetValue());
		}
		UE_LOG(LogTemp, Log, TEXT("ServerReplicationKick: %d entities. Slice NetIDs[%d] from %d: %s%s"),
			Num, MaxLog, SliceStart, *IdList, (SliceSize > MaxLog ? TEXT(" ...") : TEXT("")));
	}

	// Detailed tag replication log per entity (verbose only)
	if (CVarRTS_ServerKick_LogLevel.GetValueOnGameThread() >= 2)
	{
		const int32 MaxLog = FMath::Min(20, SliceSize);
		for (int32 i = 0; i < MaxLog; ++i)
		{
			const int32 Idx = (SliceStart + i) % Num;
			const uint32 ID = NetIDs[Idx].NetID.GetValue();
			const FMassEntityHandle EH = ChunkContext.GetEntity(Idx);
			const uint32 NewBits = BuildReplicatedTagBits(EntityManager, EH);
			const FSig* Prev = GLastSigByID.Find(ID);
			const uint32 OldBits = Prev ? Prev->TagBits : 0u;
			const bool bChangedBits = (NewBits != OldBits);
			const FString Names = StringifyUnitTagBits(NewBits);
			UE_LOG(LogTemp, Log, TEXT("ServerReplicationKick: NetID=%u TagBits=0x%08x %s Tags=[%s]"), ID, NewBits, bChangedBits ? TEXT("[CHANGED]") : TEXT(""), *Names);
		}
		// Also log AI target fragment fields for visibility
		for (int32 i = 0; i < MaxLog; ++i)
		{
			const int32 Idx = (SliceStart + i) % Num;
			const uint32 ID = NetIDs[Idx].NetID.GetValue();
			const FMassEntityHandle EH = ChunkContext.GetEntity(Idx);
			const FMassAITargetFragment* AIT = EntityManager.GetFragmentDataPtr<FMassAITargetFragment>(EH);
			bool bHas = false; bool bFocused = false; FVector LKL = FVector::ZeroVector; FVector AbilityLoc = FVector::ZeroVector; bool bTargetSet = false; uint32 TargetNetID = 0u;
			if (AIT)
			{
				bHas = AIT->bHasValidTarget; bFocused = AIT->IsFocusedOnTarget; LKL = AIT->LastKnownLocation; AbilityLoc = AIT->AbilityTargetLocation; bTargetSet = AIT->TargetEntity.IsSet();
				if (bTargetSet && EntityManager.IsEntityValid(AIT->TargetEntity))
				{
					if (const FMassNetworkIDFragment* TgtNet = EntityManager.GetFragmentDataPtr<FMassNetworkIDFragment>(AIT->TargetEntity))
					{
						TargetNetID = TgtNet->NetID.GetValue();
					}
				}
			}
			UE_LOG(LogTemp, Log, TEXT("ServerReplicationKick: AITarget NetID=%u HasValid=%d Focused=%d LKL=%s AbilityLoc=%s TargetSet=%d TargetNetID=%u"),
				ID, bHas?1:0, bFocused?1:0, *LKL.ToString(), *AbilityLoc.ToString(), bTargetSet?1:0, TargetNetID);
		}
		// Also log values of replicated fragments (CombatStats, AgentCharacteristics, AIState)
		for (int32 i = 0; i < MaxLog; ++i)
		{
			const int32 Idx = (SliceStart + i) % Num;
			const uint32 ID = NetIDs[Idx].NetID.GetValue();
			const FMassEntityHandle EH = ChunkContext.GetEntity(Idx);
			float H = 0.f, MH = 0.f, Run = 0.f, FlyH = 0.f, StateT = 0.f; int32 Team = 0; bool Fly = false, Invis = false, CanAtk = true, CanMove = true, Hold = false;
			if (const FMassCombatStatsFragment* CS = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(EH))
			{
				H = CS->Health; MH = CS->MaxHealth; Run = CS->RunSpeed; Team = CS->TeamId;
			}
			if (const FMassAgentCharacteristicsFragment* AC = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(EH))
			{
				Fly = AC->bIsFlying; Invis = AC->bIsInvisible; FlyH = AC->FlyHeight;
			}
			if (const FMassAIStateFragment* AIS = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(EH))
			{
				StateT = AIS->StateTimer; CanAtk = AIS->CanAttack; CanMove = AIS->CanMove; Hold = AIS->HoldPosition;
			}
			UE_LOG(LogTemp, Log, TEXT("ServerRep Frags: Health=%.1f/%.1f Run=%.1f Team=%d Flying=%d Invis=%d FlyH=%.1f StateT=%.2f CanAtk=%d CanMove=%d Hold=%d"),
				H, MH, Run, Team, Fly?1:0, Invis?1:0, FlyH, StateT, CanAtk?1:0, CanMove?1:0, Hold?1:0);
		}
	}

	// Invoke the same function the MassReplicationProcessor would call on the server.
	Replicator->ProcessClientReplication(ChunkContext, RepCtx);

	// Account budget usage for this slice
	ProcessedThisTick += SliceSize;
	++ChunksUsedThisTick;

	// Clear slice control so other paths process full ranges by default
	ReplicationSliceControl::ClearSlice();

	// Update stored signatures after replication to reflect the latest sent state for the slice
	for (int32 i = SliceStart; i < SliceStart + SliceSize && i < Num; ++i)
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
		const FMassEntityHandle EH = ChunkContext.GetEntity(i);
		S.TagBits = BuildReplicatedTagBits(EntityManager, EH);
		GLastSigByID.FindOrAdd(ID) = S;
	}
};

// First pass: process chunks starting from rotating start index
int32 ChunkOrdinal = 0;
EntityQuery.ForEachEntityChunk(Context, [&, ChunkStartIndex](FMassExecutionContext& ChunkContext)
{
	if (ProcessedThisTick >= MaxPerTick) { return; }
	if (ChunkOrdinal++ < ChunkStartIndex) { return; }
	ProcessChunk(ChunkContext);
});

// Second pass: wrap-around to the beginning if budget remains
if (ProcessedThisTick < MaxPerTick && TotalChunksThisTick > 0 && ChunkStartIndex > 0)
{
	int32 ChunkOrdinal2 = 0;
	EntityQuery.ForEachEntityChunk(Context, [&, ChunkStartIndex](FMassExecutionContext& ChunkContext)
	{
		if (ProcessedThisTick >= MaxPerTick) { return; }
		if (ChunkOrdinal2++ >= ChunkStartIndex) { return; }
		ProcessChunk(ChunkContext);
	});
}

// Rotate start index for next tick so later chunks get priority next time
if (TotalChunksThisTick > 0)
{
	ChunkStartIndex = (ChunkStartIndex + FMath::Max(1, ChunksUsedThisTick)) % TotalChunksThisTick;
}
}
}
