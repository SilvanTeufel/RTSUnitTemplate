// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/TutorialActor.h"

#include "Actors/WorkArea.h"
#include "Characters/Unit/BuildingBase.h"
#include "Characters/Unit/UnitBase.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Save/TutorialProgressSave.h"
#include "Widgets/TutorialWidget.h"

ATutorialActor::ATutorialActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = false;
	SetHidden(true);
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

void ATutorialActor::BeginPlay()
{
	Super::BeginPlay();

	// Only where a player looks at the screen.
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (bStartOnBeginPlay && ConsumeAutoStart())
	{
		StartTimer = StartDelay;
		SetActorTickEnabled(true);
	}
}

void ATutorialActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Widget)
	{
		Widget->RemoveFromParent();
		Widget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

FString ATutorialActor::GetProgressKey() const
{
	return ProgressKey.IsEmpty() ? UGameplayStatics::GetCurrentLevelName(this, true) : ProgressKey;
}

int32 ATutorialActor::GetAutoStartCount() const
{
	const UTutorialProgressSave* Save = Cast<UTutorialProgressSave>(
		UGameplayStatics::LoadGameFromSlot(UTutorialProgressSave::SlotName, 0));
	const int32* Count = Save ? Save->AutoStarts.Find(GetProgressKey()) : nullptr;
	return Count ? *Count : 0;
}

bool ATutorialActor::ConsumeAutoStart()
{
	if (MaxAutoStarts <= 0 || Steps.Num() == 0)
	{
		return true;
	}
	UTutorialProgressSave* Save = Cast<UTutorialProgressSave>(
		UGameplayStatics::LoadGameFromSlot(UTutorialProgressSave::SlotName, 0));
	if (!Save)
	{
		Save = Cast<UTutorialProgressSave>(UGameplayStatics::CreateSaveGameObject(UTutorialProgressSave::StaticClass()));
	}
	if (!Save)
	{
		return true;
	}
	int32& Count = Save->AutoStarts.FindOrAdd(GetProgressKey());
	if (Count >= MaxAutoStarts)
	{
		return false;
	}
	++Count;
	UGameplayStatics::SaveGameToSlot(Save, UTutorialProgressSave::SlotName, 0);
	return true;
}

bool ATutorialActor::ReplayTutorial(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return false;
	}
	for (TActorIterator<ATutorialActor> It(World); It; ++It)
	{
		if (It->Steps.Num() > 0)
		{
			It->StopTutorial();
			It->StartTutorial();
			return true;
		}
	}
	return false;
}

bool ATutorialActor::HasTutorial(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return false;
	}
	for (TActorIterator<ATutorialActor> It(World); It; ++It)
	{
		if (It->Steps.Num() > 0)
		{
			return true;
		}
	}
	return false;
}

AControllerBase* ATutorialActor::GetLocalController() const
{
	UWorld* World = GetWorld();
	return World ? Cast<AControllerBase>(World->GetFirstPlayerController()) : nullptr;
}

void ATutorialActor::StartTutorial()
{
	if (GetNetMode() == NM_DedicatedServer || Steps.Num() == 0)
	{
		return;
	}
	AControllerBase* Controller = GetLocalController();
	if (!Controller || !Controller->IsLocalController())
	{
		// The controller (or its team) is not there yet - try again in a moment.
		StartTimer = 0.5f;
		SetActorTickEnabled(true);
		return;
	}
	StartTimer = -1.f;

	if (!Widget && WidgetClass)
	{
		Widget = CreateWidget<UTutorialWidget>(Controller, WidgetClass);
		if (Widget)
		{
			Widget->AddToViewport(WidgetZOrder);
			Widget->OnCloseClicked.AddDynamic(this, &ATutorialActor::HandleWidgetClosed);
		}
	}

	// What stands already (start base, pre-placed buildings) does not count.
	BuildingBaseline.SetNum(Steps.Num());
	for (int32 i = 0; i < Steps.Num(); ++i)
	{
		const ETutorialStepCondition C = Steps[i].Condition;
		BuildingBaseline[i] = C == ETutorialStepCondition::PlaceBuilding || C == ETutorialStepCondition::FinishBuilding
			? CountBuildings(Steps[i], C == ETutorialStepCondition::FinishBuilding) : 0;
	}

	bRunning = true;
	SetActorTickEnabled(true);
	BeginStep(0);
}

void ATutorialActor::StopTutorial()
{
	bRunning = false;
	CurrentStep = INDEX_NONE;
	StartTimer = -1.f;
	if (Widget)
	{
		Widget->HideTutorial();
	}
}

void ATutorialActor::HandleWidgetClosed()
{
	// The player closed it with the X: stop for good (the replay button can start it again).
	StopTutorial();
}

void ATutorialActor::CompleteCurrentStep()
{
	if (bRunning && Steps.IsValidIndex(CurrentStep) && !bStepDone)
	{
		bStepDone = true;
		DoneTime = 0.f;
		OnStepCompleted.Broadcast(CurrentStep);
	}
}

void ATutorialActor::BeginStep(int32 Index)
{
	if (!Steps.IsValidIndex(Index))
	{
		StopTutorial();
		OnTutorialFinished.Broadcast();
		return;
	}
	CurrentStep = Index;
	bStepDone = false;
	StepTime = 0.f;
	DoneTime = 0.f;
	CheckTimer = 0.f;
	if (Widget)
	{
		Widget->ShowStep(MakeStepText(Steps[Index]), Index, Steps.Num());
	}
	OnStepStarted.Broadcast(Index);
}

void ATutorialActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (StartTimer >= 0.f)
	{
		StartTimer -= DeltaSeconds;
		if (StartTimer < 0.f)
		{
			StartTutorial();
		}
		return;
	}
	if (!bRunning || !Steps.IsValidIndex(CurrentStep))
	{
		SetActorTickEnabled(false);
		return;
	}

	const FTutorialStep& Step = Steps[CurrentStep];
	StepTime += DeltaSeconds;

	if (!bStepDone)
	{
		// Keys every frame (a tap lasts one frame), everything else every CheckInterval.
		bool bDone = Step.Condition == ETutorialStepCondition::PressKey && KeyPressed(Step);
		CheckTimer -= DeltaSeconds;
		if (!bDone && Step.Condition != ETutorialStepCondition::PressKey && CheckTimer <= 0.f)
		{
			CheckTimer = CheckInterval;
			bDone = IsStepDone(Step);
		}
		if (bDone)
		{
			CompleteCurrentStep();
		}
	}
	else
	{
		DoneTime += DeltaSeconds;
		if (StepTime >= Step.MinDisplayTime && DoneTime >= StepDelay)
		{
			BeginStep(CurrentStep + 1);
		}
	}
}

FText ATutorialActor::MakeStepText(const FTutorialStep& Step)
{
	FString Text = Step.Text.ToString();
	if (!Text.Contains(TEXT("{")) || Step.UnitClasses.Num() == 0 || !Step.UnitClasses[0])
	{
		return Step.Text;
	}
	const FString Name = GetDefault<AUnitBase>(Step.UnitClasses[0])->Name;
	const bool bVowel = Name.Len() > 0 && FString(TEXT("AEIOUaeiou")).Contains(Name.Left(1));
	Text.ReplaceInline(TEXT("{AName}"), *FString::Printf(TEXT("%s %s"), bVowel ? TEXT("an") : TEXT("a"), *Name), ESearchCase::CaseSensitive);
	Text.ReplaceInline(TEXT("{Name}"), *Name, ESearchCase::CaseSensitive);
	return FText::FromString(Text);
}

bool ATutorialActor::MatchesClasses(const FTutorialStep& Step, const UClass* Class)
{
	if (Step.UnitClasses.Num() == 0)
	{
		return true;
	}
	for (const TSubclassOf<AUnitBase>& Wanted : Step.UnitClasses)
	{
		if (Wanted && Class && Class->IsChildOf(Wanted))
		{
			return true;
		}
	}
	return false;
}

bool ATutorialActor::KeyPressed(const FTutorialStep& Step) const
{
	const APlayerController* Controller = GetLocalController();
	if (!Controller || !Step.Key.Key.IsValid() || !Controller->WasInputKeyJustPressed(Step.Key.Key))
	{
		return false;
	}
	auto Down = [Controller](const FKey& A, const FKey& B) { return Controller->IsInputKeyDown(A) || Controller->IsInputKeyDown(B); };
	return (!Step.Key.bCtrl || Down(EKeys::LeftControl, EKeys::RightControl))
		&& (!Step.Key.bShift || Down(EKeys::LeftShift, EKeys::RightShift))
		&& (!Step.Key.bAlt || Down(EKeys::LeftAlt, EKeys::RightAlt))
		&& (!Step.Key.bCmd || Down(EKeys::LeftCommand, EKeys::RightCommand));
}

bool ATutorialActor::IsStepDone(const FTutorialStep& Step) const
{
	switch (Step.Condition)
	{
	case ETutorialStepCondition::SelectUnits:
	{
		const AControllerBase* Controller = GetLocalController();
		if (!Controller)
		{
			return false;
		}
		int32 Matching = 0;
		for (const AUnitBase* Unit : Controller->SelectedUnits)
		{
			if (IsValid(Unit) && Unit->GetUnitState() != UnitData::Dead && Unit->TeamId == Controller->SelectableTeamId
				&& (!Step.bWorkersOnly || Unit->IsWorker)
				&& (!Step.UnitTag.IsValid() || Unit->UnitTags.HasTagExact(Step.UnitTag))
				&& MatchesClasses(Step, Unit->GetClass()))
			{
				++Matching;
			}
		}
		return Matching >= Step.Count;
	}
	case ETutorialStepCondition::PlaceBuilding:
	case ETutorialStepCondition::FinishBuilding:
	{
		const int32 Baseline = BuildingBaseline.IsValidIndex(CurrentStep) ? BuildingBaseline[CurrentStep] : 0;
		return CountBuildings(Step, Step.Condition == ETutorialStepCondition::FinishBuilding) - Baseline >= Step.Count;
	}
	case ETutorialStepCondition::Wait:
		return StepTime >= Step.WaitSeconds;
	default:
		return false;
	}
}

int32 ATutorialActor::CountBuildings(const FTutorialStep& Step, bool bFinishedOnly) const
{
	const AControllerBase* Controller = GetLocalController();
	UWorld* World = GetWorld();
	if (!Controller || !World || Step.UnitClasses.Num() == 0)
	{
		return 0;
	}
	const int32 TeamId = Controller->SelectableTeamId;
	int32 Num = 0;
	for (TActorIterator<ABuildingBase> It(World); It; ++It)
	{
		if (IsValid(*It) && It->TeamId == TeamId && It->GetUnitState() != UnitData::Dead && MatchesClasses(Step, It->GetClass()))
		{
			++Num;
		}
	}
	if (!bFinishedOnly)
	{
		// Placed construction sites; one whose building already stands is counted by that building.
		for (TActorIterator<AWorkArea> It(World); It; ++It)
		{
			const AWorkArea* Area = *It;
			if (IsValid(Area) && Area->TeamId == TeamId && Area->BuildingClass && !Area->FinishedBuilding
				&& (Area->PlannedBuilding || Area->StartedBuilding) && MatchesClasses(Step, Area->BuildingClass))
			{
				++Num;
			}
		}
	}
	return Num;
}
