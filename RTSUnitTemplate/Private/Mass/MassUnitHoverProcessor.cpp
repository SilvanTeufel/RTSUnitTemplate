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
	float ClosestDistanceSq = FLT_MAX;
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

			FVector BaseLocation = EntityTransform.GetLocation();
			// Fuer die HOEHE den GROESSEREN der beiden Werte nehmen, nicht den der gerade
			// gewaehlten Form. Eine Einheit mit Box-Kollision hat trotzdem eine Kapsel, und ist die
			// hoeher als die Box, blieb der obere Teil der Einheit bisher unklickbar. Umgekehrt
			// genauso. Die Breite bleibt formabhaengig (GetRadiusInDirection), weil ein
			// aufgeweiteter Radius benachbarte Einheiten gegenseitig verdecken wuerde.
			const float HalfHeight = FMath::Max(
				CharFrag.CapsuleHeight,
				CharFrag.bUseBoxComponent ? CharFrag.BoxExtent.Z : 0.f);
			const float Height = HalfHeight * 2.0f;

			// Die Trefferkapsel aus der EIGENEN Pose aufbauen, nicht aus LastGroundLocation.
			//
			// Bisher stand der Fuss der Kapsel auf dem zuletzt getracten Boden. Stimmt dieser Wert
			// nicht mehr - Rampe, Plattform, Dach, oder ein fehlgeschlagener Bodentrace -, sitzt die
			// Trefferkapsel senkrecht versetzt zu der Einheit, die man auf dem Schirm sieht.
			//
			// Und genau dieser Versatz haengt am Kamerawinkel: schaut die Kamera steil von oben,
			// laeuft der Strahl fast senkrecht und ein Hoehenfehler verschiebt den Abstand zum
			// Strahl kaum. Wird die Kamera herausgezoomt und flacher gestellt, wandert derselbe
			// Hoehenfehler direkt in die Seite - die Einheit laesst sich dann nicht mehr
			// ueberfahren, obwohl der Mauszeiger auf ihr steht. Das erklaert das "stimmt nicht
			// immer".
			//
			// Der Entitaets-Transform ist dieselbe Quelle, aus der auch die sichtbare Instanz
			// gezeichnet wird. Damit deckt sich die Trefferkapsel per Bauart mit dem Bild.
			// Der Versatz ist derselbe, den ActorTransformSyncProcessor beim Setzen der Pose
			// abzieht (dort HeightOffset) - bei stimmendem Bodenwert kommt also exakt dieselbe
			// Kapsel heraus wie vorher, nur ohne die Abhaengigkeit vom letzten Trace.
			BaseLocation.Z -= CharFrag.bIsFlying ? HalfHeight : CharFrag.CapsuleHeight;

			FVector OutP1, OutP2;
			FMath::SegmentDistToSegmentSafe(RayOrigin, RayEnd, BaseLocation, BaseLocation + FVector(0,0,Height), OutP1, OutP2);
			float DistSq = FVector::DistSquared(OutP1, OutP2);

			FVector DirToMouse = OutP1 - OutP2;
			DirToMouse.Z = 0.f;
			FVector Dir2D = DirToMouse.GetSafeNormal2D();
			if (Dir2D.IsNearlyZero())
			{
				// Steht der Mauszeiger genau auf der Achse der Einheit, hat der Verbindungsvektor
				// keine waagerechte Komponente mehr. GetRadiusInDirection liefert dann fuer
				// Box-Einheiten ausdruecklich 0 und der Treffer faellt an der Stelle durch, an
				// der er am sichersten sein muesste. Ersatzweise waagerecht von der Kamera weg
				// peilen - diese Richtung ist immer definiert.
				Dir2D = (BaseLocation - RayOrigin).GetSafeNormal2D();
				if (Dir2D.IsNearlyZero())
				{
					Dir2D = FVector(1.f, 0.f, 0.f);
				}
			}
			float Radius = CharFrag.GetRadiusInDirection(Dir2D, EntityTransform.GetRotation().Rotator());

			if (DistSq <= FMath::Square(Radius))
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
				float DistToCamSq = FVector::DistSquared(RayOrigin, EntityTransform.GetLocation());
				if (DistToCamSq < ClosestDistanceSq)
				{
					ClosestDistanceSq = DistToCamSq;
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
