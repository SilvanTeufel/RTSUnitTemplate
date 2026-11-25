// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Controller/PlayerController/RTSPerfCapacityTest.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/MassUnitBase.h"
#include "GameModes/RTSGameModeBase.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Hud/HUDBase.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderTimer.h"
#include "DynamicRHI.h"

namespace RTSPerfCapacity
{
	/** The running test. Rooted while it runs, so no UPROPERTY owner is needed. */
	static URTSPerfCapacityTest* GActive = nullptr;

	/** A unit counts as "moving" when its path in one window is at least this long (uu). */
	static constexpr double MinPathPerWindow = 500.0;

	/** Below this share of moving units a window fails the movement check. */
	static constexpr float MinMovingPct = 50.f;

	static double Average(const TArray<float>& Values)
	{
		double Sum = 0.0;
		for (float V : Values) { Sum += V; }
		return Values.Num() > 0 ? Sum / Values.Num() : 0.0;
	}

	static double Percentile(TArray<float> Values, float Pct)
	{
		if (Values.Num() == 0) { return 0.0; }
		Values.Sort();
		const int32 Index = FMath::Clamp(FMath::CeilToInt(Pct / 100.f * Values.Num()) - 1, 0, Values.Num() - 1);
		return Values[Index];
	}
}

ACustomControllerBase* URTSPerfCapacityTest::GetController() const
{
	return ControllerPtr.Get();
}

bool URTSPerfCapacityTest::IsRunning()
{
	return RTSPerfCapacity::GActive != nullptr;
}

void URTSPerfCapacityTest::Start(ACustomControllerBase* Controller, const FSettings& InSettings)
{
	if (!Controller)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PerfCapacity] No ACustomControllerBase - test not started."));
		return;
	}
	Stop(TEXT("restarted"));

	URTSPerfCapacityTest* Test = NewObject<URTSPerfCapacityTest>(GetTransientPackage());
	Test->AddToRoot();
	RTSPerfCapacity::GActive = Test;

	Test->Settings = InSettings;
	Test->Settings.Step = FMath::Max(1, Test->Settings.Step);
	Test->ControllerPtr = Controller;
	Test->Phase = EPhase::Warmup;
	Test->PhaseStart = FPlatformTime::Seconds();
	Test->ApplyUncappedFrameRate();
	Test->TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(Test, &URTSPerfCapacityTest::Tick));

	UE_LOG(LogTemp, Warning,
		TEXT("[PerfCapacity] Start | kind %s | target %.0f fps | start %d, step %d, max %d | warmup %.0f s, settle %.0f s, measure %.0f s | require %d | own units of that kind now %d"),
		KindName(Test->Settings.Kind), Test->Settings.TargetFps, Test->Settings.StartUnits, Test->Settings.Step, Test->Settings.MaxUnits,
		Test->Settings.WarmupSeconds, Test->Settings.SettleSeconds, Test->Settings.MeasureSeconds,
		Test->Settings.RequiredUnits, Test->CountOwnUnits());
}

void URTSPerfCapacityTest::Stop(const TCHAR* Reason)
{
	if (URTSPerfCapacityTest* Test = RTSPerfCapacity::GActive)
	{
		FTSTicker::GetCoreTicker().RemoveTicker(Test->TickerHandle);
		Test->Finish(FString::Printf(TEXT("stopped (%s)"), Reason));
	}
}

void URTSPerfCapacityTest::ApplyUncappedFrameRate()
{
	// With the usual 60 fps cap every passing step would read "60.0" and the headroom stays
	// invisible. The cap goes off for the test and comes back in RestoreFrameRate.
	IConsoleManager& Console = IConsoleManager::Get();
	if (IConsoleVariable* MaxFps = Console.FindConsoleVariable(TEXT("t.MaxFPS")))
	{
		SavedMaxFps = MaxFps->GetFloat();
		MaxFps->Set(0.f, ECVF_SetByConsole);
	}
	if (IConsoleVariable* VSync = Console.FindConsoleVariable(TEXT("r.VSync")))
	{
		SavedVSync = VSync->GetInt();
		VSync->Set(0, ECVF_SetByConsole);
	}
	if (GEngine)
	{
		bSavedSmoothFrameRate = GEngine->bSmoothFrameRate;
		GEngine->bSmoothFrameRate = false;
	}
	// No attribute tree with pause in the middle of a step.
	if (IConsoleVariable* PauseOnPoint = Console.FindConsoleVariable(TEXT("RTS.Talents.PauseOnPoint")))
	{
		SavedPauseOnPoint = PauseOnPoint->GetInt();
		PauseOnPoint->Set(0, ECVF_SetByConsole);
	}
	bFrameRateChanged = true;
}

void URTSPerfCapacityTest::RestoreFrameRate()
{
	if (!bFrameRateChanged)
	{
		return;
	}
	IConsoleManager& Console = IConsoleManager::Get();
	if (IConsoleVariable* MaxFps = Console.FindConsoleVariable(TEXT("t.MaxFPS")))
	{
		MaxFps->Set(SavedMaxFps, ECVF_SetByConsole);
	}
	if (IConsoleVariable* VSync = Console.FindConsoleVariable(TEXT("r.VSync")))
	{
		VSync->Set(SavedVSync, ECVF_SetByConsole);
	}
	if (GEngine)
	{
		GEngine->bSmoothFrameRate = bSavedSmoothFrameRate;
	}
	if (IConsoleVariable* PauseOnPoint = Console.FindConsoleVariable(TEXT("RTS.Talents.PauseOnPoint")))
	{
		PauseOnPoint->Set(SavedPauseOnPoint, ECVF_SetByConsole);
	}
	bFrameRateChanged = false;
}

int32 URTSPerfCapacityTest::CountOwnUnits() const
{
	const ACustomControllerBase* PC = GetController();
	if (!PC || !PC->GetWorld())
	{
		return 0;
	}
	int32 Count = 0;
	for (TActorIterator<AUnitBase> It(PC->GetWorld()); It; ++It)
	{
		const AUnitBase* U = *It;
		if (IsValid(U) && U->TeamId == PC->SelectableTeamId && U->CanMove && U->GetUnitState() != UnitData::Dead
			&& MatchesKind(U))
		{
			++Count;
		}
	}
	return Count;
}

const TCHAR* URTSPerfCapacityTest::KindName(EUnitKind Kind)
{
	switch (Kind)
	{
	case EUnitKind::Ism:      return TEXT("ism");
	case EUnitKind::Skeletal: return TEXT("skeletal");
	default:                  return TEXT("mixed");
	}
}

bool URTSPerfCapacityTest::IsSkeletalUnit(const AUnitBase* Unit)
{
	// A Mass unit without skeletal movement is drawn as an ISM instance; everything else (skeletal
	// Mass units and plain character units) moves a skeletal mesh actor.
	const AMassUnitBase* MassUnit = Cast<AMassUnitBase>(Unit);
	return !MassUnit || MassUnit->bUseSkeletalMovement;
}

bool URTSPerfCapacityTest::MatchesKind(const AUnitBase* Unit) const
{
	switch (Settings.Kind)
	{
	case EUnitKind::Ism:      return !IsSkeletalUnit(Unit);
	case EUnitKind::Skeletal: return IsSkeletalUnit(Unit);
	default:                  return true;
	}
}

int32 URTSPerfCapacityTest::RemoveOtherKind()
{
	ACustomControllerBase* PC = GetController();
	if (!PC || Settings.Kind == EUnitKind::Mixed)
	{
		return 0;
	}
	static const FGameplayTag CameraUnitTag =
		FGameplayTag::RequestGameplayTag(FName(TEXT("Character.CameraUnit")), /*ErrorIfNotFound=*/false);

	int32 Removed = 0;
	for (TActorIterator<AUnitBase> It(PC->GetWorld()); It; ++It)
	{
		AUnitBase* U = *It;
		if (!IsValid(U) || U->TeamId != PC->SelectableTeamId || !U->CanMove || U->GetUnitState() == UnitData::Dead
			|| MatchesKind(U))
		{
			continue;
		}
		// The camera unit is the player's hero - without it the map may count as lost.
		if (CameraUnitTag.IsValid() && U->UnitTags.HasTag(CameraUnitTag))
		{
			continue;
		}
		U->KillSilently();
		++Removed;
	}
	return Removed;
}

int32 URTSPerfCapacityTest::SpawnUnits(int32 Count)
{
	ACustomControllerBase* PC = GetController();
	ARTSGameModeBase* GameMode = PC ? Cast<ARTSGameModeBase>(UGameplayStatics::GetGameMode(PC)) : nullptr;
	if (!GameMode || Count <= 0)
	{
		return 0;
	}

	// Which row: the spawn tables also hold buildings and enemy species. Preferred is a row whose
	// class already marches for this player (same look and cost as in a real match), then the most
	// common own class of that kind (LevelSix places its ISM units by hand - no table row has
	// them), and only then the first movable row of the right kind.
	TMap<UClass*, int32> OwnClasses;
	for (TActorIterator<AUnitBase> It(PC->GetWorld()); It; ++It)
	{
		if (IsValid(*It) && It->TeamId == PC->SelectableTeamId && It->CanMove && It->GetUnitState() != UnitData::Dead
			&& MatchesKind(*It))
		{
			OwnClasses.FindOrAdd(It->GetClass())++;
		}
	}
	const FUnitSpawnParameter* Chosen = nullptr;
	const FUnitSpawnParameter* Fallback = nullptr;
	for (UDataTable* Table : GameMode->UnitSpawnParameters)
	{
		if (!Table) { continue; }
		for (const FName& RowName : Table->GetRowNames())
		{
			const FUnitSpawnParameter* Row = Table->FindRow<FUnitSpawnParameter>(RowName, TEXT(""));
			const AUnitBase* Cdo = (Row && Row->UnitBaseClass) ? Row->UnitBaseClass->GetDefaultObject<AUnitBase>() : nullptr;
			if (!Cdo || !Cdo->CanMove || !MatchesKind(Cdo))
			{
				continue;
			}
			if (!Chosen && OwnClasses.Contains(Row->UnitBaseClass.Get()))
			{
				Chosen = Row;
			}
			if (!Fallback)
			{
				Fallback = Row;
			}
		}
	}
	FUnitSpawnParameter OwnClassRow;
	if (!Chosen && OwnClasses.Num() > 0)
	{
		OwnClasses.ValueSort([](int32 A, int32 B) { return A > B; });
		OwnClassRow.UnitBaseClass = OwnClasses.CreateConstIterator()->Key;
		OwnClassRow.State = UnitData::Idle;
		OwnClassRow.StatePlaceholder = UnitData::Idle;
		Chosen = &OwnClassRow;
	}
	if (!Chosen)
	{
		Chosen = Fallback;
	}
	if (!Chosen)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PerfCapacity] No movable %s row in the spawn tables."), KindName(Settings.Kind));
		return 0;
	}

	int32 Spawned = 0;
	const int32 Offset = CountOwnUnits();
	for (int32 i = 0; i < Count; ++i)
	{
		// Golden angle on a disc that grows with the total, so the density stays about the same.
		const int32 Index = Offset + i;
		const float Angle = 2.39996323f * Index;
		const float Radius = Settings.Spread * FMath::Sqrt(float(Index + 1) / 600.f);
		const FVector Location = SpawnCenter + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);
		if (GameMode->SpawnSingleUnit(*Chosen, Location, nullptr, PC->SelectableTeamId, nullptr))
		{
			++Spawned;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[PerfCapacity] Spawned %d of %d x %s (row id %d)."),
		Spawned, Count, *GetNameSafe(Chosen->UnitBaseClass.Get()), Chosen->Id);
	return Spawned;
}

void URTSPerfCapacityTest::SelectAll()
{
	if (ACustomControllerBase* PC = GetController())
	{
		PC->RTSSelectAllOwn();
	}
}

void URTSPerfCapacityTest::DeselectAll()
{
	// AHUDBase::DeselectAllUnits only empties the selection when CharacterIsUnSelectable is set, so
	// the selection is taken apart here directly - the same steps, unconditionally.
	ACustomControllerBase* PC = GetController();
	if (!PC)
	{
		return;
	}
	if (AHUDBase* HUD = PC->HUDBase)
	{
		for (AUnitBase* U : HUD->SelectedUnits)
		{
			if (IsValid(U))
			{
				U->SetDeselected();
			}
		}
		HUD->SelectedUnits.Empty();
		HUD->SelectedUnitsSet.Empty();
	}
	PC->SelectedUnits.Empty();
}

void URTSPerfCapacityTest::IssueOrder()
{
	ACustomControllerBase* PC = GetController();
	if (!PC)
	{
		return;
	}

	// Orders always go through the selection, exactly like a player's right click. Only what
	// happens AFTER the order differs: the unselected windows drop the selection again.
	SelectAll();
	FVector Center = FVector::ZeroVector;
	int32 Counter = 0;
	for (AUnitBase* U : PC->SelectedUnits)
	{
		if (IsValid(U) && U->CanMove) { Center += U->GetMassActorLocation(); ++Counter; }
	}
	if (Counter > 0)
	{
		Center /= Counter;
		// Back and forth, so the group keeps marching instead of arriving and standing.
		const float Sign = (OrderCount % 2 == 0) ? 1.f : -1.f;
		const FVector Target = Center + FVector(Settings.Distance * Sign, Settings.Distance * Sign * 0.6f, 0.f);
		++OrderCount;
		PC->RTSPerfTestFixCamera();
		PC->RightClickPressedMassMinimap(Target);
	}
	if (!bKeepSelected)
	{
		DeselectAll();
	}
	LastOrderTime = FPlatformTime::Seconds();
}

void URTSPerfCapacityTest::SampleMovement(bool bAccumulate)
{
	ACustomControllerBase* PC = GetController();
	if (!PC || !PC->GetWorld())
	{
		return;
	}
	if (!bAccumulate)
	{
		Tracks.Reset();
	}
	for (TActorIterator<AUnitBase> It(PC->GetWorld()); It; ++It)
	{
		AUnitBase* U = *It;
		if (!IsValid(U) || U->TeamId != PC->SelectableTeamId || !U->CanMove || U->GetUnitState() == UnitData::Dead
			|| !MatchesKind(U))
		{
			continue;
		}
		const FVector Mass = U->GetMassActorLocation();
		const FVector Actor = U->GetActorLocation();
		const bool bSkeletal = IsSkeletalUnit(U);

		FUnitTrack* Track = Tracks.Find(U);
		if (!Track || !bAccumulate)
		{
			FUnitTrack& New = Tracks.Add(U);
			New.LastMass = Mass;
			New.LastActor = Actor;
			New.bSkeletal = bSkeletal;
			continue;
		}
		Track->MassPath += FVector::Dist2D(Track->LastMass, Mass);
		Track->ActorPath += FVector::Dist2D(Track->LastActor, Actor);
		Track->LastMass = Mass;
		Track->LastActor = Actor;
		// Only actors that are on screen are guaranteed to get their transform written every frame.
		Track->bSkeletal = bSkeletal && U->IsOnViewport;
	}
	LastMovementSample = FPlatformTime::Seconds();
}

void URTSPerfCapacityTest::EnterSettle()
{
	bKeepSelected = (NextWindow == EWindow::BaselineSelected);
	if (bKeepSelected)
	{
		SelectAll();
	}
	else
	{
		DeselectAll();
	}
	Phase = EPhase::Settle;
	PhaseStart = FPlatformTime::Seconds();
	// A fresh order right away, so newly spawned units march for the whole window too.
	LastOrderTime = -1000.0;
}

void URTSPerfCapacityTest::BeginWindow(EWindow Window)
{
	CurrentWindow = Window;
	FrameMs.Reset();
	SumGameMs = SumRenderMs = SumGpuMs = 0.0;
	SampleMovement(false);
	Phase = EPhase::Measure;
	PhaseStart = FPlatformTime::Seconds();
}

void URTSPerfCapacityTest::EndWindow()
{
	SampleMovement(true);

	FWindowResult R;
	switch (CurrentWindow)
	{
	case EWindow::BaselineUnselected: R.Label = TEXT("baseline unselected"); break;
	case EWindow::BaselineSelected:   R.Label = TEXT("baseline SELECTED"); break;
	case EWindow::BaselineDrift:      R.Label = TEXT("baseline unselected (drift)"); break;
	default:                          R.Label = TEXT("step"); break;
	}
	R.Units = CountOwnUnits();
	R.bSelected = bKeepSelected;
	R.Frames = FrameMs.Num();
	R.FrameMs = RTSPerfCapacity::Average(FrameMs);
	R.FrameMsP95 = RTSPerfCapacity::Percentile(FrameMs, 95.f);
	R.Fps = R.FrameMs > 0.0 ? 1000.0 / R.FrameMs : 0.0;
	if (R.Frames > 0)
	{
		R.GameThreadMs = SumGameMs / R.Frames;
		R.RenderThreadMs = SumRenderMs / R.Frames;
		R.GpuMs = SumGpuMs / R.Frames;
	}

	int32 Tracked = 0, Moved = 0, SkeletalTracked = 0, SkeletalMoved = 0;
	for (const TPair<TWeakObjectPtr<AUnitBase>, FUnitTrack>& Pair : Tracks)
	{
		if (!Pair.Key.IsValid())
		{
			continue;
		}
		++Tracked;
		Moved += Pair.Value.MassPath >= RTSPerfCapacity::MinPathPerWindow ? 1 : 0;
		if (Pair.Value.bSkeletal)
		{
			++SkeletalTracked;
			SkeletalMoved += Pair.Value.ActorPath >= RTSPerfCapacity::MinPathPerWindow ? 1 : 0;
		}
	}
	R.MovedMassPct = Tracked > 0 ? 100.f * Moved / Tracked : 0.f;
	R.MovedSkeletalPct = SkeletalTracked > 0 ? 100.f * SkeletalMoved / SkeletalTracked : -1.f;
	R.bMovementOk = R.MovedMassPct >= RTSPerfCapacity::MinMovingPct
		&& (R.MovedSkeletalPct < 0.f || R.MovedSkeletalPct >= RTSPerfCapacity::MinMovingPct);

	UE_LOG(LogTemp, Warning,
		TEXT("[PerfCapacity] %-28s units %4d | %6.1f fps | frame %6.2f ms (p95 %6.2f) | GT %6.2f RT %6.2f GPU %6.2f ms | moving: mass %5.1f %%, skeletal actors %s | %d frames"),
		*R.Label, R.Units, R.Fps, R.FrameMs, R.FrameMsP95, R.GameThreadMs, R.RenderThreadMs, R.GpuMs,
		R.MovedMassPct,
		R.MovedSkeletalPct < 0.f ? TEXT("  n/a") : *FString::Printf(TEXT("%5.1f %%"), R.MovedSkeletalPct),
		R.Frames);
	Results.Add(R);

	if (!R.bMovementOk)
	{
		bMovementFailed = true;
		Finish(TEXT("units are not moving"));
		return;
	}

	auto SpawnNextStep = [this]() -> bool
	{
		const int32 Before = CountOwnUnits();
		if (Before + Settings.Step > Settings.MaxUnits)
		{
			Finish(FString::Printf(TEXT("max units reached (%d)"), Settings.MaxUnits));
			return false;
		}
		SpawnUnits(Settings.Step);
		if (CountOwnUnits() <= Before)
		{
			Finish(TEXT("spawn failed"));
			return false;
		}
		NextWindow = EWindow::Step;
		EnterSettle();
		return true;
	};

	switch (CurrentWindow)
	{
	case EWindow::BaselineUnselected:
		NextWindow = EWindow::BaselineSelected;
		EnterSettle();
		break;
	case EWindow::BaselineSelected:
		NextWindow = EWindow::BaselineDrift;
		EnterSettle();
		break;
	case EWindow::BaselineDrift:
	{
		// The baseline count passes on the mean of both unselected windows.
		const double Mean = 0.5 * (Results[Results.Num() - 3].Fps + R.Fps);
		if (Mean < Settings.TargetFps)
		{
			Finish(TEXT("already below target at the baseline"));
			return;
		}
		BestUnits = R.Units;
		BestFps = Mean;
		SpawnNextStep();
		break;
	}
	default:
		if (R.Fps < Settings.TargetFps)
		{
			Finish(TEXT("below target"));
			return;
		}
		BestUnits = R.Units;
		BestFps = R.Fps;
		SpawnNextStep();
		break;
	}
}

bool URTSPerfCapacityTest::Tick(float DeltaTime)
{
	if (Phase == EPhase::Done)
	{
		return false;
	}
	ACustomControllerBase* PC = GetController();
	if (!PC || !PC->GetWorld())
	{
		Finish(TEXT("controller or world gone"));
		return false;
	}

	const double Now = FPlatformTime::Seconds();

	// A paused game (menu, layout choice, talent point) is no measurement: the phase starts over.
	if (UGameplayStatics::IsGamePaused(PC))
	{
		PhaseStart = Now;
		FrameMs.Reset();
		SumGameMs = SumRenderMs = SumGpuMs = 0.0;
		return true;
	}

	if (Now - LastOrderTime >= Settings.OrderInterval)
	{
		IssueOrder();
	}

	const double Elapsed = Now - PhaseStart;
	switch (Phase)
	{
	case EPhase::Warmup:
		if (Elapsed >= Settings.WarmupSeconds)
		{
			// Spawn centre and the default start count from the units the map gave the player.
			FVector Sum = FVector::ZeroVector;
			int32 Counter = 0;
			for (TActorIterator<AUnitBase> It(PC->GetWorld()); It; ++It)
			{
				if (IsValid(*It) && It->TeamId == PC->SelectableTeamId && It->CanMove && It->GetUnitState() != UnitData::Dead)
				{
					Sum += It->GetMassActorLocation();
					++Counter;
				}
			}
			SpawnCenter = Counter > 0 ? Sum / Counter : FVector::ZeroVector;
			UnitsBeforeClearing = Counter;
			const int32 Removed = RemoveOtherKind();
			UE_LOG(LogTemp, Warning, TEXT("[PerfCapacity] Kind %s: %d own movable units on the map, %d of the other kind removed."),
				KindName(Settings.Kind), Counter, Removed);
			Phase = EPhase::Clearing;
			PhaseStart = Now;
		}
		break;

	case EPhase::Clearing:
		// The removed units need a moment to die and leave Mass before the count is right.
		if (Elapsed >= 3.0)
		{
			const int32 Wanted = Settings.StartUnits > 0 ? Settings.StartUnits : UnitsBeforeClearing;
			const int32 Missing = FMath::Min(Wanted, Settings.MaxUnits) - CountOwnUnits();
			if (Missing > 0)
			{
				SpawnUnits(Missing);
			}
			NextWindow = EWindow::BaselineUnselected;
			EnterSettle();
		}
		break;

	case EPhase::Settle:
		if (Elapsed >= Settings.SettleSeconds)
		{
			BeginWindow(NextWindow);
		}
		break;

	case EPhase::Measure:
		FrameMs.Add(DeltaTime * 1000.f);
		SumGameMs += FPlatformTime::ToMilliseconds(GGameThreadTime);
		SumRenderMs += FPlatformTime::ToMilliseconds(GRenderThreadTime);
		SumGpuMs += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0));
		if (Now - LastMovementSample >= 1.0)
		{
			SampleMovement(true);
		}
		if (Elapsed >= Settings.MeasureSeconds)
		{
			EndWindow();
		}
		break;

	default:
		break;
	}
	return Phase != EPhase::Done;
}

void URTSPerfCapacityTest::Finish(const FString& Reason)
{
	if (Phase == EPhase::Done && RTSPerfCapacity::GActive != this)
	{
		return;
	}
	Phase = EPhase::Done;
	RestoreFrameRate();

	// Selection cost and drift from the baseline triple (unselected, selected, unselected).
	FString Baseline = TEXT("baseline incomplete");
	if (Results.Num() >= 3)
	{
		const FWindowResult& A = Results[0];
		const FWindowResult& S = Results[1];
		const FWindowResult& B = Results[2];
		const double UnselectedMs = 0.5 * (A.FrameMs + B.FrameMs);
		Baseline = FString::Printf(
			TEXT("selection rings at %d units: %.1f -> %.1f fps (+%.2f ms frame, +%.2f ms GT) | drift %.2f ms (%.1f fps)"),
			S.Units, 0.5 * (A.Fps + B.Fps), S.Fps, S.FrameMs - UnselectedMs,
			S.GameThreadMs - 0.5 * (A.GameThreadMs + B.GameThreadMs),
			FMath::Abs(A.FrameMs - B.FrameMs), FMath::Abs(A.Fps - B.Fps));
	}

	// Estimate where the frame time crosses the target, linear between the last passing and the
	// first failing step. Labelled as an estimate - only BestUnits is measured.
	FString Estimate = TEXT("-");
	const FWindowResult* LastPass = nullptr;
	const FWindowResult* FirstFail = nullptr;
	for (const FWindowResult& R : Results)
	{
		if (R.bSelected) { continue; }
		if (R.Fps >= Settings.TargetFps) { LastPass = &R; }
		else if (!FirstFail) { FirstFail = &R; }
	}
	if (LastPass && FirstFail && FirstFail->Units > LastPass->Units && FirstFail->FrameMs > LastPass->FrameMs)
	{
		const double TargetMs = 1000.0 / Settings.TargetFps;
		const double Slope = (FirstFail->FrameMs - LastPass->FrameMs) / (FirstFail->Units - LastPass->Units);
		const int32 Units = LastPass->Units + FMath::FloorToInt((TargetMs - LastPass->FrameMs) / Slope);
		Estimate = FString::Printf(TEXT("~%d (%.3f ms per unit)"), Units, Slope);
	}

	const bool bGateOk = Settings.RequiredUnits <= 0 || BestUnits >= Settings.RequiredUnits;
	const bool bAborted = Reason.StartsWith(TEXT("stopped")) || Reason.StartsWith(TEXT("controller"))
		|| Reason.StartsWith(TEXT("already below"))
		|| Reason.StartsWith(TEXT("spawn")) || Results.Num() < 3;
	const bool bPass = !bMovementFailed && bGateOk && !bAborted;
	const FString Verdict = bPass ? TEXT("PASS") : TEXT("FAIL");

	UE_LOG(LogTemp, Warning, TEXT("[PerfCapacity] End: %s"), *Reason);
	UE_LOG(LogTemp, Warning, TEXT("[PerfCapacity] %s"), *Baseline);
	UE_LOG(LogTemp, Warning,
		TEXT("[PerfCapacity] RESULT %s | kind %s | %d units at >= %.0f fps (measured %.1f fps, unselected) | crossing estimate %s | required %d | movement %s"),
		*Verdict, KindName(Settings.Kind), BestUnits, Settings.TargetFps, BestFps, *Estimate, Settings.RequiredUnits,
		bMovementFailed ? TEXT("FAILED") : TEXT("ok"));

	WriteCsv(Verdict);

	if (RTSPerfCapacity::GActive == this)
	{
		RTSPerfCapacity::GActive = nullptr;
		RemoveFromRoot();
	}
	if (Settings.bQuitWhenDone)
	{
		FPlatformMisc::RequestExitWithStatus(false, bPass ? 0 : 3);
	}
}

void URTSPerfCapacityTest::WriteCsv(const FString& Verdict) const
{
	FString Csv = TEXT("Window,Units,Selected,Frames,Fps,FrameMs,FrameMsP95,GameThreadMs,RenderThreadMs,GpuMs,MovingMassPct,MovingSkeletalPct,MovementOk\n");
	for (const FWindowResult& R : Results)
	{
		Csv += FString::Printf(TEXT("%s,%d,%d,%d,%.2f,%.3f,%.3f,%.3f,%.3f,%.3f,%.1f,%.1f,%d\n"),
			*R.Label, R.Units, R.bSelected ? 1 : 0, R.Frames, R.Fps, R.FrameMs, R.FrameMsP95,
			R.GameThreadMs, R.RenderThreadMs, R.GpuMs, R.MovedMassPct, R.MovedSkeletalPct, R.bMovementOk ? 1 : 0);
	}
	Csv += FString::Printf(TEXT("RESULT,%d,,,%.2f,,,,,,,,%s\n"), BestUnits, BestFps, *Verdict);

	const FString Path = FPaths::ProfilingDir() / FString::Printf(TEXT("RTSPerfCapacity_%s_%s.csv"),
		KindName(Settings.Kind), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	if (FFileHelper::SaveStringToFile(Csv, *Path))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PerfCapacity] Written: %s"), *FPaths::ConvertRelativePathToFull(Path));
	}
}

static FAutoConsoleCommandWithWorldAndArgs GRTSPerfCapacityCommand(
	TEXT("RTS.PerfTest.Capacity"),
	TEXT("How many own units keep TargetFps while marching, with the selection rings OFF. ")
	TEXT("Args (Key=Value): Kind=ism|skeletal|mixed Start=0 Step=50 Max=2000 Target=60 Measure=12 Settle=4 Warmup=10 ")
	TEXT("Spread=4000 Distance=9000 Require=0 Quit=0. Quit=1 exits with 0 (pass) or 3 (fail)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ACustomControllerBase* PC = World ? Cast<ACustomControllerBase>(World->GetFirstPlayerController()) : nullptr;
		const FString Line = FString::Join(Args, TEXT(" "));
		URTSPerfCapacityTest::FSettings S;
		FString Kind;
		if (FParse::Value(*Line, TEXT("Kind="), Kind))
		{
			S.Kind = Kind.Equals(TEXT("skeletal"), ESearchCase::IgnoreCase) ? URTSPerfCapacityTest::EUnitKind::Skeletal
				: Kind.Equals(TEXT("mixed"), ESearchCase::IgnoreCase) ? URTSPerfCapacityTest::EUnitKind::Mixed
				: URTSPerfCapacityTest::EUnitKind::Ism;
		}
		FParse::Value(*Line, TEXT("Start="), S.StartUnits);
		FParse::Value(*Line, TEXT("Step="), S.Step);
		FParse::Value(*Line, TEXT("Max="), S.MaxUnits);
		FParse::Value(*Line, TEXT("Target="), S.TargetFps);
		FParse::Value(*Line, TEXT("Measure="), S.MeasureSeconds);
		FParse::Value(*Line, TEXT("Settle="), S.SettleSeconds);
		FParse::Value(*Line, TEXT("Warmup="), S.WarmupSeconds);
		FParse::Value(*Line, TEXT("Spread="), S.Spread);
		FParse::Value(*Line, TEXT("Distance="), S.Distance);
		FParse::Value(*Line, TEXT("Require="), S.RequiredUnits);
		int32 Quit = 0;
		FParse::Value(*Line, TEXT("Quit="), Quit);
		S.bQuitWhenDone = Quit != 0;
		URTSPerfCapacityTest::Start(PC, S);
	}));

static FAutoConsoleCommand GRTSPerfCapacityStopCommand(
	TEXT("RTS.PerfTest.Capacity.Stop"),
	TEXT("Stops a running RTS.PerfTest.Capacity and prints what was measured so far."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		URTSPerfCapacityTest::Stop(TEXT("console"));
	}));
