// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Mass/MassUnitHoverProcessor.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "Characters/Unit/MassUnitBase.h"
#include "Characters/Unit/UnitBase.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassActorSubsystem.h"
#include "MassSignalSubsystem.h"
#include "Mass/MassUnitVisualFragments.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "Async/Async.h"
#include "MassEntitySubsystem.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "HAL/IConsoleManager.h"
#include "DrawDebugHelpers.h"
#include "Core/RTSUnitGeometry.h"

static TAutoConsoleVariable<float> CVarHoverInterval(
	TEXT("rts.hover.interval"),
	-1.f,
	TEXT("Abstand zwischen zwei Hover-Pruefungen in Sekunden. -1 = Wert aus dem Prozessor ")
	TEXT("(HoverUpdateInterval, Vorgabe 0,05). Kleiner = schneller, aber teurer."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarHoverDiag(
	TEXT("rts.hover.diag"),
	0,
	TEXT("1 = meldet alle 5 s, wie weit die Hover-Trefferkapsel vom gemerkten Bodenwert abweicht."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarHoverDebugDraw(
	TEXT("rts.hover.debugdraw"),
	0,
	TEXT("1 = zeichnet fuer die ueberfahrene Einheit die drei Hover-Formen: Kapsel gruen, ")
	TEXT("BoxCollision-Box blau, Mesh-Box gelb; die getroffene Form dick. 2 = zusaetzlich den ")
	TEXT("Vorfilter-Zylinder (grau) jedes Kandidaten."),
	ECVF_Default);

// DIE DREI HOVER-FORMEN (01.10.2026).
//
// Nutzervorgabe: Hover = Strahl trifft die echte Kapsel ODER die echte BoxCollision-Box ODER die
// Mesh-Box - jede nur aus ihren EIGENEN Massen, der vorderste Treffer gewinnt.
//
// Die Geometrie selbst steht seit 01.10.2026 (zweiter Durchgang) in RTSUnitGeometry: derselbe Satz
// Formen dient auch dem Einrasten und der Pfadpruefung der EnergyWall. Gebaeude sind dort an
// LastGroundLocation verankert statt am FTransformFragment - dessen Z lag bei einem Teil der
// Gebaeude eine Kapselhalbhoehe zu tief, und genau so tief lag der Hover am WallTower.
namespace HoverShapes = RTSUnitGeometry;

UMassUnitHoverProcessor::UMassUnitHoverProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bRequiresGameThreadExecution = true; // Still required to access PlayerController and potentially Actor mesh bounds
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
}

void UMassUnitHoverProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassUnitVisualFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassHoverFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All); // All units and buildings

	// TOTE Entitaeten fallen raus.
	//
	// Bis zum 21.09.2026 lief der Hover ueber ALLE Einheiten, auch die gefallenen. Das kostete
	// nicht nur Rechenzeit in genau den Momenten, in denen am meisten los ist - eine Leiche
	// wurde dabei auch zum gueltigen Klickziel und konnte eine lebende Einheit verdecken.
	EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
	EntityQuery.RegisterWithProcessor(*this);
}

void UMassUnitHoverProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);
	
	if (UWorld* World = Owner.GetWorld())
	{
		SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
	}

	if (SignalSubsystem)
	{
		CustomOverlapStartDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::CustomOverlapStart)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UMassUnitHoverProcessor, HandleCustomOverlapStart));

		CustomOverlapEndDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::CustomOverlapEnd)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UMassUnitHoverProcessor, HandleCustomOverlapEnd));
	}
}

void UMassUnitHoverProcessor::BeginDestroy()
{
	if (SignalSubsystem)
	{
		if (CustomOverlapStartDelegateHandle.IsValid())
		{
			auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::CustomOverlapStart);
			Delegate.Remove(CustomOverlapStartDelegateHandle);
			CustomOverlapStartDelegateHandle.Reset();
		}

		if (CustomOverlapEndDelegateHandle.IsValid())
		{
			auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::CustomOverlapEnd);
			Delegate.Remove(CustomOverlapEndDelegateHandle);
			CustomOverlapEndDelegateHandle.Reset();
		}
	}

	Super::BeginDestroy();
}

void UMassUnitHoverProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{

	// Siehe mass_scopes: macht diesen Prozessor als Spalte Exclusive/UMassUnitHoverProcessor im CSV sichtbar.
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(UMassUnitHoverProcessor);

	UWorld* World = EntityManager.GetWorld();
	if (!World || !SignalSubsystem) return;

	// Siehe HoverUpdateInterval. Negativ in der Konsolenvariable heisst: Wert aus dem Prozessor.
	const float IntervalOverride = CVarHoverInterval.GetValueOnGameThread();
	const float UpdateInterval = IntervalOverride >= 0.f ? IntervalOverride : HoverUpdateInterval;

	AccumulatedTime += Context.GetDeltaTimeSeconds();
	if (AccumulatedTime < UpdateInterval)
	{
		return;
	}
	AccumulatedTime = 0.f;

	// DIAGNOSE (bleibt stehen bis abbestellt), schaltbar ueber rts.hover.diag 1.
	//
	// Misst, wie weit der gemerkte Bodenwert von der tatsaechlichen Pose abweicht. Genau diese
	// Abweichung verschob frueher die Trefferkapsel gegen das Bild - unsichtbar bei steiler
	// Kamera, spuerbar bei flacher. Laeuft VOR der Maus-Abfrage, damit sie auch kopflos misst.
	if (CVarHoverDiag.GetValueOnGameThread() != 0)
	{
		HoverDiagTime += 0.1f;
		if (HoverDiagTime >= 5.f)
		{
			HoverDiagTime = 0.f;

			int32 Gesamt = 0;
			int32 Abweichend = 0;
			float MaxAbweichung = 0.f;

			EntityQuery.ForEachEntityChunk(Context, ([&](FMassExecutionContext& ChunkContext)
			{
				const auto DiagTransforms = ChunkContext.GetFragmentView<FTransformFragment>();
				const auto DiagCharFrags = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();

				for (int32 i = 0; i < ChunkContext.GetNumEntities(); ++i)
				{
					const FMassAgentCharacteristicsFragment& C = DiagCharFrags[i];
					const float PoseBasis = DiagTransforms[i].GetTransform().GetLocation().Z
						- (C.bIsFlying ? (C.bUseBoxComponent ? C.BoxExtent.Z : C.CapsuleHeight) : C.CapsuleHeight);
					const float AlteBasis = C.bIsFlying
						? C.LastGroundLocation + C.FlyHeight - (C.bUseBoxComponent ? C.BoxExtent.Z : C.CapsuleHeight)
						: C.LastGroundLocation;

					const float Delta = FMath::Abs(PoseBasis - AlteBasis);
					++Gesamt;
					if (Delta > 50.f) ++Abweichend;
					MaxAbweichung = FMath::Max(MaxAbweichung, Delta);
				}
			}));

			UE_LOG(LogTemp, Warning,
				TEXT("[Hover-Diag] %d von %d Einheiten mit Kapselversatz > 50 uu, groesster Versatz %.0f uu | Takt %.3f s | letzte Pruefung %d Entitaeten in %.3f ms"),
				Abweichend, Gesamt, MaxAbweichung, UpdateInterval, LastCheckedEntities, LastCheckMilliseconds);
		}
	}

	ACustomControllerBase* LocalPC = Cast<ACustomControllerBase>(World->GetFirstPlayerController());
	if (!LocalPC || !LocalPC->IsLocalController()) return;

	FVector RayOrigin, RayDirection;
	if (!LocalPC->DeprojectMousePositionToWorld(RayOrigin, RayDirection)) return;
	FVector RayEnd = RayOrigin + RayDirection * 100000.f;

	// Messung der eigentlichen Pruefung - ohne sie ist "zu rechenintensiv?" nicht zu beantworten.
	const double CheckStartSeconds = FPlatformTime::Seconds();
	int32 CheckedEntities = 0;

	FMassEntityHandle BestEntity;
	// Eintrittsdistanz des besten Treffers entlang des Strahls (01.10.2026). Bisher gewann die
	// Entitaet mit dem kameranaechsten MITTELPUNKT - ein grosses Gebaeude schluckte so Einheiten,
	// die vor ihm standen. Jetzt gewinnt, was der Strahl ZUERST trifft.
	double BestEntryDistance = TNumericLimits<double>::Max();
	const double RayLength = 100000.0;
	const int32 DebugDrawMode = CVarHoverDebugDraw.GetValueOnGameThread();
	const float DebugDrawLifeTime = UpdateInterval + 0.02f;
	HoverShapes::FShapeSet BestShapes;
	HoverShapes::EHitShape BestHitShape = HoverShapes::EHitShape::None;
	int32 BestInstanceIndex = INDEX_NONE;
	TWeakObjectPtr<UInstancedStaticMeshComponent> BestISM = nullptr;
	TWeakObjectPtr<USkeletalMeshComponent> BestMesh = nullptr;
	// Der Aktor zur besten Entitaet - damit der Klick ihn direkt bekommt, statt ihn ueber
	// Linienspuren noch einmal zu suchen (siehe ACustomControllerBase::HoveredUnit).
	TWeakObjectPtr<AUnitBase> BestUnit = nullptr;

	EntityQuery.ForEachEntityChunk(Context, ([&](FMassExecutionContext& ChunkContext)
	{
		const auto Transforms = ChunkContext.GetFragmentView<FTransformFragment>();
		const auto VisualFrags = ChunkContext.GetFragmentView<FMassUnitVisualFragment>();
		const auto ActorFrags = ChunkContext.GetFragmentView<FMassActorFragment>();
		const auto CharFrags = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();

		CheckedEntities += ChunkContext.GetNumEntities();

		for (int32 i = 0; i < ChunkContext.GetNumEntities(); ++i)
		{
			const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
			bool bHit = false;
			int32 CurrentInstanceIndex = INDEX_NONE;
			TWeakObjectPtr<UInstancedStaticMeshComponent> CurrentISM = nullptr;
			TWeakObjectPtr<USkeletalMeshComponent> CurrentMesh = nullptr;
			TWeakObjectPtr<AUnitBase> CurrentUnit = nullptr;

			const FTransform& EntityTransform = Transforms[i].GetTransform();
			const FMassAgentCharacteristicsFragment& CharFrag = CharFrags[i];

			// TREFFERPRUEFUNG (01.10.2026): Vorfilter, dann drei getrennte Formen.
			//
			// Der Vorfilter ist ein senkrechter Zylinder, der alle drei Formen umschliesst - ein
			// einziger Strecken-Abstand je Entitaet wie bisher. Nur wer ihn trifft, bekommt die
			// genauen Tests. Ein Vorfilter-Treffer allein entscheidet nichts.
			//
			// Danach Hover = Kapsel ODER BoxCollision-Box ODER Mesh-Box, jede nur aus ihren eigenen
			// Massen (siehe HoverShapes oben). Die fruehere Mischform aus Box-Radius und Mesh-Hoehe
			// samt Kappeneinzug (24.09.2026) ist damit abgeloest.
			HoverShapes::FShapeSet Shapes;
			HoverShapes::Build(CharFrag, EntityTransform, Shapes);
			if (!Shapes.bValid)
			{
				continue;
			}

			FVector OutP1, OutP2;
			FMath::SegmentDistToSegmentSafe(RayOrigin, RayEnd, Shapes.BroadStart, Shapes.BroadEnd, OutP1, OutP2);
			if (FVector::DistSquared(OutP1, OutP2) > FMath::Square(Shapes.BroadRadius))
			{
				continue;
			}

			if (DebugDrawMode >= 2)
			{
				HoverShapes::DrawBroad(World, Shapes, DebugDrawLifeTime);
			}

			HoverShapes::EHitShape HitShape = HoverShapes::EHitShape::None;
			const double EntryDistance = HoverShapes::RayTrace(Shapes, RayOrigin, RayDirection, RayLength, HitShape);

			if (EntryDistance >= 0.0)
			{
				bHit = true;

				const FMassUnitVisualFragment& VisualFrag = VisualFrags[i];
				if (VisualFrag.VisualInstances.Num() > 0)
				{
					CurrentInstanceIndex = VisualFrag.VisualInstances[0].InstanceIndex;
					CurrentISM = VisualFrag.VisualInstances[0].TargetISM.Get();
				}

				if (const AActor* Actor = ActorFrags[i].Get())
				{
					if (CurrentInstanceIndex == INDEX_NONE)
					{
						if (const AMassUnitBase* Unit = Cast<AMassUnitBase>(Actor))
						{
							CurrentInstanceIndex = Unit->InstanceIndex;
							CurrentISM = Unit->ISMComponent;
						}
					}
					
					if (const ACharacter* Char = Cast<ACharacter>(Actor))
					{
						CurrentMesh = Char->GetMesh();
					}

					CurrentUnit = const_cast<AUnitBase*>(Cast<AUnitBase>(Actor));
				}
			}

			if (bHit)
			{
				if (EntryDistance < BestEntryDistance)
				{
					BestEntryDistance = EntryDistance;
					BestShapes = Shapes;
					BestHitShape = HitShape;
					BestEntity = Entity;
					BestInstanceIndex = CurrentInstanceIndex;
					BestISM = CurrentISM;
					BestMesh = CurrentMesh;
					BestUnit = CurrentUnit;
				}
			}
		}
	}));

	LastCheckedEntities = CheckedEntities;
	LastCheckMilliseconds = (float)((FPlatformTime::Seconds() - CheckStartSeconds) * 1000.0);

	// Jeden Durchlauf melden, nicht nur bei Wechsel: faehrt die Maus ins Leere, muss der Eintrag
	// geloescht werden, sonst bliebe die zuletzt ueberfahrene Einheit fuer immer bevorzugtes
	// Klickziel und man koennte nichts anderes mehr anklicken.
	LocalPC->SetHoveredUnit(BestUnit.Get());

	// Pruefzeichnung ueber rts.hover.debugdraw (01.10.2026): die drei Formen der ueberfahrenen
	// Einheit, die getroffene dick. Damit laesst sich im Spiel nachsehen, welche Form greift.
	if (DebugDrawMode >= 1 && BestEntity.IsValid())
	{
		HoverShapes::DrawShapes(World, BestShapes, BestHitShape, DebugDrawLifeTime);
	}

	// Detect if we changed entity OR instance index/mesh for the same entity
	bool bInstanceChanged = false;
	if (BestEntity.IsValid() && BestEntity == LastHoveredEntity)
	{
		if (FMassHoverFragment* HoverFrag = EntityManager.GetFragmentDataPtr<FMassHoverFragment>(BestEntity))
		{
			if (HoverFrag->HoveredInstanceIndex != BestInstanceIndex || HoverFrag->HoveredMesh != BestMesh || HoverFrag->HoveredISM != BestISM)
			{
				bInstanceChanged = true;
			}
		}
	}

	// Update hover states and signal changes
	if (BestEntity != LastHoveredEntity || bInstanceChanged)
	{
		// End hover for old entity (or old instance)
		if (EntityManager.IsEntityValid(LastHoveredEntity))
		{
			FMassHoverFragment* HoverFrag = EntityManager.GetFragmentDataPtr<FMassHoverFragment>(LastHoveredEntity);
			if (HoverFrag && HoverFrag->bIsHovered)
			{
				SignalSubsystem->SignalEntity(UnitSignals::CustomOverlapEnd, LastHoveredEntity);
				HoverFrag->bIsHovered = false;
			}
		}

		// Start hover for new entity
		if (EntityManager.IsEntityValid(BestEntity))
		{
			FMassHoverFragment* HoverFrag = EntityManager.GetFragmentDataPtr<FMassHoverFragment>(BestEntity);
			if (HoverFrag && !HoverFrag->bIsHovered)
			{
				HoverFrag->HoveredInstanceIndex = BestInstanceIndex;
				HoverFrag->HoveredISM = BestISM;
				HoverFrag->HoveredMesh = BestMesh;
				HoverFrag->bIsHovered = true;
				SignalSubsystem->SignalEntity(UnitSignals::CustomOverlapStart, BestEntity);
			}
		}

		LastHoveredEntity = BestEntity;
		
	}
}

void UMassUnitHoverProcessor::HandleCustomOverlapStart(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
	UWorld* World = GetWorld();
	if (!SignalSubsystem || !World) return;

	TArray<FMassEntityHandle> EntitiesCopy = Entities;

	AsyncTask(ENamedThreads::GameThread, [this, World, EntitiesCopy]()
	{
		UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
		if (!EntitySubsystem) return;
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

		for (FMassEntityHandle Entity : EntitiesCopy)
		{
			if (EntityManager.IsEntityValid(Entity))
			{
				FMassActorFragment* ActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
				FMassHoverFragment* HoverFrag = EntityManager.GetFragmentDataPtr<FMassHoverFragment>(Entity);
				
				if (ActorFrag && HoverFrag)
				{
					if (HoverFrag->LastStartSignalFrame == GFrameCounter) continue;
					HoverFrag->LastStartSignalFrame = GFrameCounter;

					if (AMassUnitBase* Unit = const_cast<AMassUnitBase*>(Cast<AMassUnitBase>(ActorFrag->Get())))
					{
						Unit->CustomOverlapStart(HoverFrag->HoveredInstanceIndex, HoverFrag->HoveredMesh.Get());

						if (this->bSetCustomDataValue && HoverFrag->HoveredInstanceIndex != INDEX_NONE)
						{
							if (UInstancedStaticMeshComponent* TargetISM = HoverFrag->HoveredISM.Get())
							{
								TargetISM->SetCustomDataValue(HoverFrag->HoveredInstanceIndex, 0, 1.0f, true);
							}
							else if (Unit->ISMComponent)
							{
								Unit->ISMComponent->SetCustomDataValue(HoverFrag->HoveredInstanceIndex, 0, 1.0f, true);
							}
						}
					}
				}
			}
		}
	});
}

void UMassUnitHoverProcessor::HandleCustomOverlapEnd(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
	UWorld* World = GetWorld();
	if (!SignalSubsystem || !World) return;

	TArray<FMassEntityHandle> EntitiesCopy = Entities;

	AsyncTask(ENamedThreads::GameThread, [this, World, EntitiesCopy]()
	{
		UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
		if (!EntitySubsystem) return;
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

		for (FMassEntityHandle Entity : EntitiesCopy)
		{
			if (EntityManager.IsEntityValid(Entity))
			{
				FMassActorFragment* ActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
				FMassHoverFragment* HoverFrag = EntityManager.GetFragmentDataPtr<FMassHoverFragment>(Entity);
				
				if (ActorFrag && HoverFrag)
				{
					if (HoverFrag->LastEndSignalFrame == GFrameCounter) continue;
					HoverFrag->LastEndSignalFrame = GFrameCounter;

					if (AMassUnitBase* Unit = const_cast<AMassUnitBase*>(Cast<AMassUnitBase>(ActorFrag->Get())))
					{
						Unit->CustomOverlapEnd(HoverFrag->HoveredInstanceIndex, HoverFrag->HoveredMesh.Get());

						if (this->bSetCustomDataValue && HoverFrag->HoveredInstanceIndex != INDEX_NONE)
						{
							if (UInstancedStaticMeshComponent* TargetISM = HoverFrag->HoveredISM.Get())
							{
								TargetISM->SetCustomDataValue(HoverFrag->HoveredInstanceIndex, 0, 0.0f, true);
							}
							else if (Unit->ISMComponent)
							{
								Unit->ISMComponent->SetCustomDataValue(HoverFrag->HoveredInstanceIndex, 0, 0.0f, true);
							}
						}
					}
				}
			}
		}
	});
}
