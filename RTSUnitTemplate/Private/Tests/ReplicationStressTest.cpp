#include "Tests/ReplicationStressTest.h"
#include "Mass/Replication/UnitRegistryReplicator.h"
#include "Characters/Unit/UnitBase.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "MassEntitySubsystem.h"
#include "Mass/MassActorBindingComponent.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "FunctionalTest.h"
#include "Net/UnrealNetwork.h"

AReplicationStressTest::AReplicationStressTest()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	StaticLoadUnitCount = 500;
	BurstLoadUnitCount = 200;
	BurstDelay = 5.0f;
	TestTimeout = 60.0f;
	bAutoStartInPIE = false;
	
	// Erhöhe das Framework-Zeitlimit von AFunctionalTest
	TimeLimit = 100.0f;
}

void AReplicationStressTest::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	bReplicates = true;
}

void AReplicationStressTest::BeginPlay()
{
	Super::BeginPlay();
	// Ermöglicht den Start im normalen PIE ohne Framework-Controller
	if (bAutoStartInPIE && !IsRunning())
	{
		PrepareTest();
		StartTest();
	}
}

void AReplicationStressTest::PrepareTest()
{
	Super::PrepareTest();

	// Stelle sicher, dass das Framework-Zeitlimit ausreicht
	TimeLimit = TestTimeout + 10.0f;

	if (HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("STRESS TEST: Server startet Spawning von %d Einheiten..."), StaticLoadUnitCount);
		SpawnUnits(StaticLoadUnitCount);
	}
}

void AReplicationStressTest::StartTest()
{
	Super::StartTest();
	TimeSinceStart = 0.0f;
}

void AReplicationStressTest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AReplicationStressTest, bBurstSpawned);
	DOREPLIFETIME(AReplicationStressTest, StaticLoadUnitCount);
	DOREPLIFETIME(AReplicationStressTest, BurstLoadUnitCount);
	DOREPLIFETIME(AReplicationStressTest, BurstDelay);
	DOREPLIFETIME(AReplicationStressTest, TestTimeout);
}

void AReplicationStressTest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsRunning() || CurrentState == ETestState::Finished) return;

	TimeSinceStart += DeltaSeconds;

	// 1. Suche nach dem Replicator (für alle zugänglich)
	AUnitRegistryReplicator* Reg = nullptr;
	for (TActorIterator<AUnitRegistryReplicator> It(GetWorld()); It; ++It)
	{
		Reg = *It;
		break;
	}

	if (Reg)
	{
		int32 Registered, Total;
		Reg->GetRegistrationCounts(Registered, Total);

		// 2. Zustandsmaschine (für alle zugänglich)
		switch (CurrentState)
		{
		case ETestState::WaitingForStaticLoad:
			if (Registered >= StaticLoadUnitCount)
			{
				UE_LOG(LogTemp, Warning, TEXT("STRESS TEST: Initial-Load erkannt (%d/%d)"), Registered, StaticLoadUnitCount);
				CurrentState = ETestState::WaitingForBurstLoad;
			}
			break;

		case ETestState::WaitingForBurstLoad:
			if (bBurstSpawned && (Registered >= (StaticLoadUnitCount + BurstLoadUnitCount)))
			{
				CurrentState = ETestState::ValidatingSelection;
			}
			break;

		case ETestState::ValidatingSelection:
			{
				int32 ValidHandles = 0;
				for (TActorIterator<AUnitBase> It(GetWorld()); It; ++It)
				{
					if (UMassActorBindingComponent* Binding = It->FindComponentByClass<UMassActorBindingComponent>())
					{
						if (Binding->GetEntityHandle().IsValid()) ValidHandles++;
					}
				}

				if (ValidHandles > 0 || (StaticLoadUnitCount + BurstLoadUnitCount == 0))
				{
					FinishTest(EFunctionalTestResult::Succeeded, TEXT("Stress-Test erfolgreich abgeschlossen!"));
					CurrentState = ETestState::Finished;
					return; // Beende Tick hier
				}
			}
			break;
		default: ;
		}
	}

	// 3. Server Logik für Spawning
	if (HasAuthority())
	{
		if (!bBurstSpawned && TimeSinceStart >= BurstDelay)
		{
			UE_LOG(LogTemp, Warning, TEXT("STRESS TEST: Server startet Burst Spawning..."));
			SpawnUnits(BurstLoadUnitCount);
			bBurstSpawned = true;
		}
	}

	// 4. Timeout Check am Ende
	if (TimeSinceStart > TestTimeout)
	{
		RunDetailedClientCheck();
		int32 RegCount = 0, TotCount = 0;
		if (Reg) Reg->GetRegistrationCounts(RegCount, TotCount);
		
		FString ErrorMsg = FString::Printf(TEXT("Test Timeout! Zustand: %d, Reg: %d/%d"), 
			(int32)CurrentState, RegCount, StaticLoadUnitCount + (bBurstSpawned ? BurstLoadUnitCount : 0));
		
		FinishTest(EFunctionalTestResult::Failed, ErrorMsg);
		CurrentState = ETestState::Finished;
	}
}

void AReplicationStressTest::SpawnUnits(int32 Count)
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Nutze das gewählte Blueprint oder Fallback auf Basisklasse
	TSubclassOf<AUnitBase> UnitClass = UnitClassToSpawn ? UnitClassToSpawn : TSubclassOf<AUnitBase>(AUnitBase::StaticClass());
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	FVector StartLoc = GetActorLocation() + FVector(500.0f, 0.0f, 0.0f);
	int32 Side = FMath::CeilToInt(FMath::Sqrt((float)Count));

	for (int32 i = 0; i < Count; ++i)
	{
		FVector Loc = StartLoc + FVector((i / Side) * 200.0f, (i % Side) * 200.0f, 0.0f);
		if (AUnitBase* NewUnit = World->SpawnActor<AUnitBase>(UnitClass, Loc, FRotator::ZeroRotator, SpawnParams))
		{
			// WICHTIG: Index manuell setzen, da wir am GameMode vorbeigehen
			// Nutze einen hohen Bereich für Test-Einheiten
			static int32 TestUnitCounter = 10000; 
			NewUnit->SetUnitIndex(TestUnitCounter++);
		}
	}
}

void AReplicationStressTest::RunDetailedClientCheck()
{
	int32 UnitsWithIndex = 0;
	int32 UnitsWithMassBinding = 0;
	int32 TotalUnits = 0;
	
	for (TActorIterator<AUnitBase> It(GetWorld()); It; ++It)
	{
		TotalUnits++;
		if (It->UnitIndex != INDEX_NONE) UnitsWithIndex++;
		if (UMassActorBindingComponent* B = It->FindComponentByClass<UMassActorBindingComponent>())
		{
			if (B->GetEntityHandle().IsValid()) UnitsWithMassBinding++;
		}
	}
	
	UE_LOG(LogTemp, Error, TEXT("DIAGNOSE: Units im Level (Total): %d, Mit UnitIndex: %d, Mit Valid Mass-Handle: %d"), 
		TotalUnits, UnitsWithIndex, UnitsWithMassBinding);
}
