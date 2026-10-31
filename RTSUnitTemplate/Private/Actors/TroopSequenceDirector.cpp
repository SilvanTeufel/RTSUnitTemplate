// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/TroopSequenceDirector.h"

#include "Characters/Camera/CameraBase.h"
#include "Characters/Camera/ExtendedCameraBase.h"
#include "Characters/Unit/UnitBase.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "GameModes/RTSGameModeBase.h"
#include "GAS/AttributeSetBase.h"
#include "Hud/PathProviderHUD.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"

ATroopSequenceDirector::ATroopSequenceDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = false;
	SetHidden(true);
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

void ATroopSequenceDirector::BeginPlay()
{
	Super::BeginPlay();
	StartTimer = StartDelay;
}

void ATroopSequenceDirector::StartSequence()
{
	CurrentTroop.Reset();
	AllSpawned.Reset();
	UnitPowerFactors.Reset();
	CurrentStep = INDEX_NONE;
	bRepeatStep = false;
	Reinforcements = 0;
	bRunning = Steps.Num() > 0;
	bWaitingForNext = false;
	StartTimer = -1.f;
	if (bRunning)
	{
		SpawnStep(0);
	}
}

void ATroopSequenceDirector::StopSequence()
{
	bRunning = false;
	bWaitingForNext = false;
	StartTimer = -1.f;
}

void ATroopSequenceDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The controller sets its own team again during its late init (after the director's first try),
	// so watch for that every tick instead of only at the steps.
	if (bSelectAllTeams && bFogApplied)
	{
		if (const ACustomControllerBase* LocalPC = GetWorld() ? Cast<ACustomControllerBase>(GetWorld()->GetFirstPlayerController()) : nullptr)
		{
			bFogApplied = LocalPC->SelectableTeamId == 0;
		}
	}
	if ((bDisableFogOfWar || bSelectAllTeams) && !bFogApplied)
	{
		ApplyFogOfWar();
	}
	if (bFollowNewTroop)
	{
		UpdateCamera(DeltaSeconds);
	}

	// Spawning and judging the fight is the server's business.
	if (!HasAuthority())
	{
		return;
	}

	if (StartTimer >= 0.f)
	{
		StartTimer -= DeltaSeconds;
		if (StartTimer < 0.f)
		{
			StartSequence();
		}
		return;
	}
	if (!bRunning)
	{
		return;
	}

	if (bWaitingForNext)
	{
		NextTimer -= DeltaSeconds;
		if (NextTimer > 0.f)
		{
			return;
		}
		bWaitingForNext = false;
		if (bRepeatStep)
		{
			bRepeatStep = false;
			SpawnStep(CurrentStep);
			return;
		}
		Reinforcements = 0;
		int32 Next = CurrentStep + 1;
		if (!Steps.IsValidIndex(Next))
		{
			if (!bLoop)
			{
				bRunning = false;
				OnSequenceFinished.Broadcast();
				return;
			}
			Next = 0;
		}
		SpawnStep(Next);
		return;
	}

	CheckTimer -= DeltaSeconds;
	if (CheckTimer > 0.f)
	{
		return;
	}
	CheckTimer = CheckInterval;

	if (IsFightOver())
	{
		int32 Survivors = 0;
		int32 WinnerTeam = INDEX_NONE;
		for (const TWeakObjectPtr<AUnitBase>& Weak : AllSpawned)
		{
			if (IsAlive(Weak.Get()))
			{
				++Survivors;
				WinnerTeam = Weak->TeamId;
			}
		}
		const bool bNewTroopWon = Steps.IsValidIndex(CurrentStep) && WinnerTeam == Steps[CurrentStep].TeamId;
		UE_LOG(LogTemp, Log, TEXT("[TroopSequence] Fight after step %d over: team %d left with %d units%s"),
			CurrentStep, WinnerTeam, Survivors, bNewTroopWon || CurrentStep == 0 ? TEXT("") : TEXT(" - the NEW troop lost"));
		// The newest unit type should win its fight: send it once more instead of going on.
		bRepeatStep = !bNewTroopWon && CurrentStep > 0 && Reinforcements < MaxReinforcements;
		if (bRepeatStep)
		{
			++Reinforcements;
		}
		OnFightOver.Broadcast(CurrentStep);
		bWaitingForNext = true;
		NextTimer = CurrentStep == 0 ? FMath::Max(NextStepDelay, FirstTroopLead) : NextStepDelay;
		return;
	}

	// Progress = somebody lost health. Regeneration does not count as progress.
	const float TotalHealth = GetTotalHealth();
	if (LastTotalHealth < 0.f || TotalHealth < LastTotalHealth * 0.995f)
	{
		StalemateTime = 0.f;
	}
	else
	{
		StalemateTime += CheckInterval;
	}
	LastTotalHealth = TotalHealth;
	if (StalemateTimeout > 0.f && StalemateTime >= StalemateTimeout)
	{
		ResolveStalemate();
		StalemateTime = 0.f;
	}
}

float ATroopSequenceDirector::GetTotalHealth() const
{
	float Total = 0.f;
	for (const TWeakObjectPtr<AUnitBase>& Weak : AllSpawned)
	{
		const AUnitBase* Unit = Weak.Get();
		if (IsAlive(Unit) && Unit->Attributes)
		{
			Total += Unit->Attributes->GetHealth() + Unit->Attributes->GetShield();
		}
	}
	return Total;
}

void ATroopSequenceDirector::ResolveStalemate()
{
	TMap<int32, float> TeamPower;
	for (const TWeakObjectPtr<AUnitBase>& Weak : AllSpawned)
	{
		if (IsAlive(Weak.Get()))
		{
			TeamPower.FindOrAdd(Weak->TeamId) += GetUnitPower(Weak.Get());
		}
	}
	int32 Loser = INDEX_NONE;
	float LoserPower = TNumericLimits<float>::Max();
	for (const TPair<int32, float>& Pair : TeamPower)
	{
		if (Pair.Value < LoserPower)
		{
			LoserPower = Pair.Value;
			Loser = Pair.Key;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[TroopSequence] Stalemate in step %d (no damage for %.0f s): team %d falls"), CurrentStep, StalemateTimeout, Loser);
	for (const TWeakObjectPtr<AUnitBase>& Weak : AllSpawned)
	{
		if (IsAlive(Weak.Get()) && Weak->TeamId == Loser)
		{
			Weak->SetShield(0.f);
			Weak->SetHealth(0.f);
		}
	}
}

void ATroopSequenceDirector::SpawnStep(int32 Index)
{
	CurrentStep = Index;
	CurrentTroop.Reset();

	ARTSGameModeBase* GameMode = Cast<ARTSGameModeBase>(UGameplayStatics::GetGameMode(this));
	const FTroopSequenceStep& Step = Steps[Index];
	const FTroopTeamSetup* Setup = FindTeamSetup(Step.TeamId);
	if (!GameMode || !Step.UnitClass || !Setup || !Setup->SpawnPoint)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TroopSequence] Step %d skipped: %s"), Index,
			!GameMode ? TEXT("no ARTSGameModeBase") : !Step.UnitClass ? TEXT("no UnitClass") : TEXT("no TeamSetup/SpawnPoint for its TeamId"));
		// Treat it as a finished fight, so the sequence goes on.
		bWaitingForNext = true;
		NextTimer = 0.f;
		return;
	}

	// Drop the dead, so the list only holds who can still fight.
	AllSpawned.RemoveAll([](const TWeakObjectPtr<AUnitBase>& U) { return !IsAlive(U.Get()); });
	for (auto It = UnitPowerFactors.CreateIterator(); It; ++It)
	{
		if (!IsAlive(It.Key().ResolveObjectPtr()))
		{
			It.RemoveCurrent();
		}
	}

	FUnitSpawnParameter Param;
	Param.UnitBaseClass = Step.UnitClass;
	Param.UnitCount = 1;
	Param.UnitOffset = FVector(0.f, 0.f, SpawnHeight);
	Param.UnitMinRange = FVector(-SpawnRadius, -SpawnRadius, 0.f);
	Param.UnitMaxRange = FVector(SpawnRadius, SpawnRadius, 0.f);
	Param.State = SpawnState;
	Param.StatePlaceholder = SpawnState;
	Param.WaypointTag = Setup->WaypointTag;
	Param.TeamId = Step.TeamId;
	Param.Attributes = Step.Attributes;
	Param.CanBeSelected = bUnitsSelectable;

	// Power of the enemies still standing, with their current health.
	float EnemyPower = 0.f;
	for (const TWeakObjectPtr<AUnitBase>& Weak : AllSpawned)
	{
		if (Weak.IsValid() && Weak->TeamId != Step.TeamId)
		{
			EnemyPower += GetUnitPower(Weak.Get());
		}
	}

	const FVector Location = Setup->SpawnPoint->GetActorLocation();
	const int32 Limit = bAdaptiveCount ? FMath::Max(Step.Count, MaxCount) : Step.Count;
	float TroopPower = 0.f;
	int32 Tries = 0;
	while (Tries++ < Limit)
	{
		const bool bEnough = CurrentTroop.Num() >= Step.Count && (!bAdaptiveCount || TroopPower >= AdaptiveRatio * EnemyPower);
		if (bEnough)
		{
			break;
		}
		if (AUnitBase* Unit = GameMode->SpawnSingleUnit(Param, Location, nullptr, Step.TeamId, nullptr))
		{
			CurrentTroop.Add(Unit);
			AllSpawned.Add(Unit);
			UnitPowerFactors.Add(Unit, Step.PowerFactor);
			TroopPower += GetUnitPower(Unit);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[TroopSequence] Step %d: %d x %s for team %d (power %.0f vs enemies %.0f)"),
		Index, CurrentTroop.Num(), *Step.UnitClass->GetName(), Step.TeamId, TroopPower, EnemyPower);
	CheckTimer = CheckInterval;
	StalemateTime = 0.f;
	LastTotalHealth = -1.f;
	if (bDisableFogOfWar || bSelectAllTeams)
	{
		// The controller may set the fog / its team up again after its own late init; say it once more.
		bFogApplied = false;
	}
	if (bSelectNewTroop)
	{
		SelectCurrentTroop();
	}
	OnStepSpawned.Broadcast(Index);
}

bool ATroopSequenceDirector::IsFightOver() const
{
	// Over when at most one team of the spawned units is still standing. With only one troop on the
	// field (the first step) that is true at once, so the next troop comes right after it.
	int32 FirstTeam = INDEX_NONE;
	for (const TWeakObjectPtr<AUnitBase>& Weak : AllSpawned)
	{
		const AUnitBase* Unit = Weak.Get();
		if (!IsAlive(Unit))
		{
			continue;
		}
		if (FirstTeam == INDEX_NONE)
		{
			FirstTeam = Unit->TeamId;
		}
		else if (Unit->TeamId != FirstTeam)
		{
			return false;
		}
	}
	return true;
}

float ATroopSequenceDirector::GetUnitPower(const AUnitBase* Unit) const
{
	if (!IsAlive(Unit) || !Unit->Attributes)
	{
		return 0.f;
	}
	const float* Factor = UnitPowerFactors.Find(Unit);
	const float Raw = FMath::Sqrt(FMath::Max(1.f, Unit->Attributes->GetHealth() + Unit->Attributes->GetShield())
		* FMath::Max(1.f, Unit->Attributes->GetAttackDamage()));
	return Raw * (Factor ? *Factor : 1.f);
}

bool ATroopSequenceDirector::IsAlive(const AUnitBase* Unit)
{
	if (!IsValid(Unit) || Unit->GetUnitState() == UnitData::Dead)
	{
		return false;
	}
	return !Unit->Attributes || Unit->Attributes->GetHealth() > 0.f;
}

bool ATroopSequenceDirector::GetAliveCentre(const TArray<TWeakObjectPtr<AUnitBase>>& Units, FVector& OutCentre)
{
	FVector Sum = FVector::ZeroVector;
	int32 Num = 0;
	for (const TWeakObjectPtr<AUnitBase>& Weak : Units)
	{
		const AUnitBase* Unit = Weak.Get();
		if (IsAlive(Unit))
		{
			Sum += Unit->GetActorLocation();
			++Num;
		}
	}
	if (Num == 0)
	{
		return false;
	}
	OutCentre = Sum / Num;
	return true;
}

ACameraBase* ATroopSequenceDirector::GetLocalCamera() const
{
	const APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	return Controller && Controller->IsLocalController() ? Cast<ACameraBase>(Controller->GetPawn()) : nullptr;
}

void ATroopSequenceDirector::ApplyFogOfWar()
{
	ACustomControllerBase* Controller = GetWorld() ? Cast<ACustomControllerBase>(GetWorld()->GetFirstPlayerController()) : nullptr;
	if (Controller && Controller->IsLocalController())
	{
		if (bDisableFogOfWar)
		{
			Controller->SetFogOfWarCheatDisabled(true);
		}
		if (bSelectAllTeams && Controller->SelectableTeamId != 0)
		{
			// Local only (standalone showcase map); 0 = the selection takes every team.
			Controller->SelectableTeamId = 0;
			// The minimap widget was bound to the player's first team when the HUD came up; bind it
			// to the minimap actor of the new team, or it keeps showing its empty (white) image.
			if (AExtendedCameraBase* Camera = Cast<AExtendedCameraBase>(Controller->GetPawn()))
			{
				if (Camera->Minimap)
				{
					Camera->Minimap->InitializeForTeam(0);
				}
			}
		}
		bFogApplied = true;
	}
}

void ATroopSequenceDirector::SelectCurrentTroop()
{
	ACustomControllerBase* Controller = GetWorld() ? Cast<ACustomControllerBase>(GetWorld()->GetFirstPlayerController()) : nullptr;
	if (!Controller || !Controller->IsLocalController() || !Controller->HUDBase)
	{
		return;
	}
	AHUDBase* HUD = Controller->HUDBase;
	HUD->DeselectAllUnits();
	for (const TWeakObjectPtr<AUnitBase>& Weak : CurrentTroop)
	{
		AUnitBase* Unit = Weak.Get();
		if (IsAlive(Unit))
		{
			HUD->SelectedUnits.AddUnique(Unit);
			HUD->SelectedUnitsSet.Add(Unit);
			Unit->SetSelected();
		}
	}
	Controller->SelectedUnits = HUD->SelectedUnits;
}

void ATroopSequenceDirector::UpdateCamera(float DeltaSeconds)
{
	ACameraBase* Camera = GetLocalCamera();
	if (!Camera)
	{
		return;
	}

	// On a listen server / standalone the troop lists are filled here; a pure client has none and
	// leaves the camera alone.
	FVector Target;
	if (!GetAliveCentre(CurrentTroop, Target) && !GetAliveCentre(AllSpawned, Target))
	{
		return;
	}

	const FVector Current = Camera->GetActorLocation();
	const FVector Goal(Target.X, Target.Y, Current.Z);
	Camera->SetActorLocation(FMath::VInterpTo(Current, Goal, DeltaSeconds, FollowSpeed));

	if (FollowArmLength > 0.f && Camera->SpringArm)
	{
		Camera->SpringArm->TargetArmLength = FMath::FInterpTo(Camera->SpringArm->TargetArmLength, FollowArmLength, DeltaSeconds, FollowSpeed);
	}
}

const FTroopTeamSetup* ATroopSequenceDirector::FindTeamSetup(int32 TeamId) const
{
	return TeamSetups.FindByPredicate([TeamId](const FTroopTeamSetup& Setup) { return Setup.TeamId == TeamId; });
}
