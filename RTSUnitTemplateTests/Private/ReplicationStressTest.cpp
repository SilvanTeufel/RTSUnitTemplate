#include "ReplicationStressTest.h"
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
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "Mass/Replication/UnitClientBubbleInfo.h"

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
	
	// Erh�he das Framework-Zeitlimit von AFunctionalTest
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
	// Erm�glicht den Start im normalen PIE ohne Framework-Controller
	if (bAutoStartInPIE && !IsRunning())
	{
		PrepareTest();
		StartTest();
	}
}

void AReplicationStressTest::PrepareTest()
{
	Super::PrepareTest();

	// 1. Zeitlimit gro�z�gig setzen
	TimeLimit = TestTimeout + 20.0f;

	if (HasAuthority())
	{
		// Wir nutzen hier Display statt Warning, um nicht selbst Warnungen zu triggern
		UE_LOG(LogTemp, Display, TEXT("STRESS TEST: Server startet Spawning von %d Einheiten..."), StaticLoadUnitCount);
		SpawnUnits(StaticLoadUnitCount);
	}
}

void AReplicationStressTest::StartTest()
{
	Super::StartTest();
	TimeSinceStart = 0.0f;
	LastMonitorTime = 0.0f;
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

	// --- NEU: Netzwerk- & Performance-Monitoring ---
	if (UNetDriver* NetDriver = GetWorld()->GetNetDriver())
	{
		// 1. Bandbreite messen (kumuliert über alle Verbindungen)
		uint32 TotalOutRate = NetDriver->OutBytesPerSecond;
		float OutRateKB = TotalOutRate / 1024.0f;

		// 2. Frame-Time messen (Ersatz für die EOS-Warnungen)
		float CurrentFPS = 1.0f / (DeltaSeconds > 0.f ? DeltaSeconds : 0.001f);

		// Alle 1 Sekunde einen Status-Bericht ausgeben
		if (TimeSinceStart - LastMonitorTime >= 1.0f)
		{
			UE_LOG(LogTemp, Display, TEXT("STATS [%.1fs]: Net-Out: %.2f KB/s, Server-FPS: %.1f"), 
				TimeSinceStart, OutRateKB, CurrentFPS);
			
			// Optional: Manueller Fail bei extremer Last (> 5 MB/s)
			if (OutRateKB > 5000.0f) 
			{
				// FinishTest(EFunctionalTestResult::Failed, TEXT("Netzwerk-Last zu hoch (> 5MB/s)!"));
			}
			
			LastMonitorTime = TimeSinceStart;
		}

		// 3. Performance-Warnung selbst loggen (wird im Report angezeigt)
		if (DeltaSeconds > 0.1f) // > 100ms Frame (entspricht der EOS Warnung)
		{
			UE_LOG(LogTemp, Display, TEXT("PERF-WARNUNG: Schwerer Lag-Spike erkannt: %.1fms"), DeltaSeconds * 1000.0f);
		}
	}

	// --- NEU: Bubble-Payload Monitoring ---
	for (TActorIterator<AUnitClientBubbleInfo> It(GetWorld()); It; ++It)
	{
		if (AUnitClientBubbleInfo* Bubble = *It)
		{
			int32 AgentCount = Bubble->Agents.Items.Num();
			
			// Dynamische Schätzung: Nutze sizeof für mehr Präzision + 10% Overhead-Puffer
			float ItemSize = (float)sizeof(FUnitReplicationItem);
			float EstimatedSizeKB = (AgentCount * ItemSize * 1.1f) / 1024.0f;
			
			static int32 LastLoggedCount = -1;
			if (AgentCount != LastLoggedCount && AgentCount > 0)
			{
				UE_LOG(LogTemp, Display, TEXT("BUBBLE STATS: Einheiten in Bubble: %d (~%.2f KB Payload)"), 
					AgentCount, EstimatedSizeKB);
				
				if (EstimatedSizeKB > 60.0f)
				{
					FString ErrorMsg = FString::Printf(TEXT("KRITISCH: Bubble Payload zu groß! %.2f KB (Limit 64KB). Reduziere Einheiten oder Payload-Fields!"), EstimatedSizeKB);
					FinishTest(EFunctionalTestResult::Failed, ErrorMsg);
					CurrentState = ETestState::Finished;
					return;
				}
				LastLoggedCount = AgentCount;
			}
		}
	}

	// 1. Suche nach dem Replicator (f�r alle zug�nglich)
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

		// 2. Zustandsmaschine (f�r alle zug�nglich)
		switch (CurrentState)
		{
		case ETestState::WaitingForStaticLoad:
			if (Registered >= StaticLoadUnitCount)
			{
				UE_LOG(LogTemp, Display, TEXT("STRESS TEST: Initial-Load erkannt (%d/%d)"), Registered, StaticLoadUnitCount);
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

	// 3. Server Logik f�r Spawning
	if (HasAuthority())
	{
		if (!bBurstSpawned && TimeSinceStart >= BurstDelay)
		{
			UE_LOG(LogTemp, Display, TEXT("STRESS TEST: Server startet Burst Spawning..."));
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

	// Nutze das gew�hlte Blueprint oder Fallback auf Basisklasse
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
			// Nutze einen hohen Bereich f�r Test-Einheiten
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
