// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Characters/Camera/RL/RLRecorderSubsystem.h"

#include "Characters/Camera/RL/InferenceComponent.h"
#include "Characters/Camera/BehaviorTree/RTSRuleBasedDeciderComponent.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Core/RTSUnitTemplateSettings.h"   // AITimeScale from Project Settings
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	/**
	 * Set to 1 to begin recording as soon as the game instance comes up, without anyone typing a command.
	 * Settable from DefaultEngine.ini under [SystemSettings], which is what makes unattended recording
	 * possible in a packaged build - the whole reason this path exists instead of the shared-memory bridge.
	 */
	static int32 GRLRecordAutoStart = 0;
	static FAutoConsoleVariableRef CVarRLRecordAutoStart(
		TEXT("rts.rl.record.autostart"),
		GRLRecordAutoStart,
		TEXT("1 = start RL recording automatically when the game instance initialises."),
		ECVF_Default);

	/**
	 * Class path of the configuration panel opened by "rts.rl.ui". Kept as a setting rather than hardcoded
	 * so the plugin carries no dependency on a particular project's content; set it in DefaultEngine.ini.
	 */
	static FString GRLUiWidgetPath = TEXT("");
	static FAutoConsoleVariableRef CVarRLUiWidgetPath(
		TEXT("rts.rl.ui.widget"),
		GRLUiWidgetPath,
		TEXT("Class path of the RL configuration widget, e.g. /Game/.../BP_RLTrainingWidget_AH.BP_RLTrainingWidget_AH_C"),
		ECVF_Default);

	/**
	 * Global time dilation applied when the game instance starts. Training needs many matches, and at real
	 * time a single useful recording takes half an hour. 1 = normal.
	 * The engine clamps dilation to AWorldSettings::MaxGlobalTimeDilation (20 by default), so the clamp is
	 * raised alongside it - otherwise asking for 10 silently gives you something else.
	 */
	static float GRLTimeScale = 1.f;
	static FAutoConsoleVariableRef CVarRLTimeScale(
		TEXT("rts.ai.timescale"),
		GRLTimeScale,
		TEXT("Global time dilation for AI training runs. 1 = real time."),
		ECVF_Default);

	/**
	 * The effective time scale: project setting by default, console variable when it was explicitly set.
	 * Rationale: the setting is the persistent, discoverable place (Project Settings -> Plugins -> RTS
	 * Unit Template); the CVar stays the quick override for a single test run without touching config.
	 * "Explicitly set" = anything other than 1, which is also the CVar's default.
	 */
	float GetEffectiveTimeScale()
	{
		if (!FMath::IsNearlyEqual(GRLTimeScale, 1.f))
		{
			return GRLTimeScale;
		}

		if (const URTSUnitTemplateSettings* Settings = URTSUnitTemplateSettings::Get())
		{
			return FMath::Max(0.1f, Settings->AITimeScale);
		}

		return 1.f;
	}

	const TCHAR* SourceToToken(ERLSampleSource Source)
	{
		switch (Source)
		{
		case ERLSampleSource::RuleBased: return TEXT("rule");
		case ERLSampleSource::Human:     return TEXT("human");
		case ERLSampleSource::Model:     return TEXT("model");
		default:                         return TEXT("unknown");
		}
	}
}

FString URLRecorderSubsystem::GetRecordingDirectory()
{
	// Saved/ is writable in a packaged build and needs no elevation - that is the whole point of this path
	// over the Global\ shared-memory mapping the Python bridge used.
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("RLData"));
}

void URLRecorderSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RegisterConsoleCommands();

	if (GRLRecordAutoStart != 0)
	{
		StartRecording(TEXT("autostart"));
	}

	// The world does not exist yet at subsystem init, so apply the speed-up once play begins.
	// Checks the EFFECTIVE scale so a value coming from Project Settings registers too, not just the CVar.
	if (!FMath::IsNearlyEqual(GetEffectiveTimeScale(), 1.f))
	{
		FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &URLRecorderSubsystem::ApplyTimeScaleToWorld);
	}
}

void URLRecorderSubsystem::ApplyTimeScaleToWorld(UWorld* World, const UWorld::InitializationValues)
{
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	const float EffectiveScale = GetEffectiveTimeScale();

	if (AWorldSettings* Settings = World->GetWorldSettings())
	{
		// Raise the ceiling first: SetGlobalTimeDilation clamps against it, so without this a request of 10
		// quietly becomes 20's-worth of nothing or the default cap.
		Settings->MaxGlobalTimeDilation = FMath::Max(Settings->MaxGlobalTimeDilation, EffectiveScale);
		// Frames are allowed to represent more simulated time; otherwise the engine caps the step and the
		// world runs slower than the dilation asks for.
		Settings->MinUndilatedFrameTime = 0.0001f;
	}

	UGameplayStatics::SetGlobalTimeDilation(World, EffectiveScale);
	UE_LOG(LogTemp, Warning, TEXT("[RLRecorder] Time scale %.1fx applied to '%s' (source: %s)."),
	       EffectiveScale, *World->GetName(),
	       FMath::IsNearlyEqual(GRLTimeScale, 1.f) ? TEXT("Project Settings") : TEXT("CVar rts.ai.timescale"));
}

namespace
{
	/** Every AI decider in the world, paired with the team it plays for. */
	void CollectAIDeciders(const UWorld* World, TMap<int32, URTSRuleBasedDeciderComponent*>& Out)
	{
		if (!World)
		{
			return;
		}
		for (TActorIterator<APawn> It(const_cast<UWorld*>(World)); It; ++It)
		{
			APawn* Pawn = *It;
			if (!IsValid(Pawn))
			{
				continue;
			}
			if (URTSRuleBasedDeciderComponent* Decider = Pawn->FindComponentByClass<URTSRuleBasedDeciderComponent>())
			{
				const int32 TeamId = Decider->ResolveOwningTeamId();
				if (TeamId > 0)
				{
					Out.Add(TeamId, Decider);
				}
			}
		}
	}

	UInferenceComponent* FindInferenceForTeam(const UWorld* World, int32 TeamId)
	{
		TMap<int32, URTSRuleBasedDeciderComponent*> Deciders;
		CollectAIDeciders(World, Deciders);
		if (URTSRuleBasedDeciderComponent** Found = Deciders.Find(TeamId))
		{
			if (*Found)
			{
				if (AActor* Owner = (*Found)->GetOwner())
				{
					return Owner->FindComponentByClass<UInferenceComponent>();
				}
			}
		}
		return nullptr;
	}
}

TArray<int32> URLRecorderSubsystem::GetAITeamIds() const
{
	TMap<int32, URTSRuleBasedDeciderComponent*> Deciders;
	CollectAIDeciders(GetWorld(), Deciders);

	TArray<int32> Teams;
	Deciders.GetKeys(Teams);
	Teams.Sort();
	return Teams;
}

int32 URLRecorderSubsystem::GetLocalPlayerTeamId() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}
	if (const AControllerBase* PC = Cast<AControllerBase>(World->GetFirstPlayerController()))
	{
		return PC->SelectableTeamId;
	}
	return 0;
}

TArray<int32> URLRecorderSubsystem::GetConfigurableAITeamIds() const
{
	const TArray<int32> AITeams = GetAITeamIds();
	const int32 MyTeam = GetLocalPlayerTeamId();

	// Team 0 is the spectator/observer seat on the test maps - from there every AI may be configured.
	if (MyTeam <= 0)
	{
		return AITeams;
	}

	// Otherwise only the AI that plays on the player's own side.
	TArray<int32> Mine;
	if (AITeams.Contains(MyTeam))
	{
		Mine.Add(MyTeam);
	}
	return Mine;
}

EBrainMode URLRecorderSubsystem::GetTeamBrainMode(int32 TeamId) const
{
	if (const UInferenceComponent* Inference = FindInferenceForTeam(GetWorld(), TeamId))
	{
		return Inference->GetBrainMode();
	}
	return EBrainMode::Behavior_Tree;
}

void URLRecorderSubsystem::SetTeamBrainMode(int32 TeamId, EBrainMode Mode)
{
	if (UInferenceComponent* Inference = FindInferenceForTeam(GetWorld(), TeamId))
	{
		// Write the per-team override rather than the shared default, so switching one side to the network
		// leaves the other on its rules - that asymmetry is the whole point of evaluating a trained model.
		Inference->TeamBrainMode.Add(TeamId, Mode);
	}
}

FString URLRecorderSubsystem::DescribeTeamAI(int32 TeamId) const
{
	const UInferenceComponent* Inference = FindInferenceForTeam(GetWorld(), TeamId);
	if (!Inference)
	{
		return FString::Printf(TEXT("Team %d - no AI"), TeamId);
	}

	const EBrainMode Mode = Inference->GetBrainMode();
	const bool bHasModel = Inference->TeamQNetworkModelData.Contains(TeamId) &&
	                       Inference->TeamQNetworkModelData[TeamId] != nullptr;

	return FString::Printf(TEXT("Team %d - %s - model: %s"), TeamId,
	                       Mode == EBrainMode::RL_Model ? TEXT("RL Model") : TEXT("Behavior Tree"),
	                       bHasModel ? TEXT("loaded") : TEXT("none"));
}

void URLRecorderSubsystem::PopulateAITeamCombo(UComboBoxString* Combo) const
{
	if (!Combo)
	{
		return;
	}

	Combo->ClearOptions();
	for (const int32 TeamId : GetConfigurableAITeamIds())
	{
		Combo->AddOption(FString::Printf(TEXT("Team %d"), TeamId));
	}

	if (Combo->GetOptionCount() > 0)
	{
		Combo->SetSelectedIndex(0);
	}
}

int32 URLRecorderSubsystem::GetSelectedTeamFromCombo(UComboBoxString* Combo) const
{
	if (!Combo)
	{
		return -1;
	}

	// The option text is "Team <id>", so the index into the configurable list is the reliable way back -
	// parsing the label would break the moment someone renames it.
	const int32 Index = Combo->GetSelectedIndex();
	const TArray<int32> Teams = GetConfigurableAITeamIds();
	return Teams.IsValidIndex(Index) ? Teams[Index] : -1;
}

FString URLRecorderSubsystem::ToggleTeamBrainMode(int32 TeamId)
{
	if (TeamId < 0)
	{
		return TEXT("no team selected");
	}

	const EBrainMode Next = (GetTeamBrainMode(TeamId) == EBrainMode::RL_Model)
		? EBrainMode::Behavior_Tree
		: EBrainMode::RL_Model;

	SetTeamBrainMode(TeamId, Next);
	return DescribeTeamAI(TeamId);
}

void URLRecorderSubsystem::Deinitialize()
{
	StopRecording();

	// Unregister by NAME, not by the stored pointer. A PIE session with a client runs two worlds, so this
	// subsystem exists twice and both instances hold pointers to the SAME console objects: the second
	// Deinitialize then unregistered freed memory and took the editor down on every PIE stop. Only the
	// instance that actually registered a name has it in this array, and looking it up fresh means a
	// pointer another instance already released can no longer be dereferenced.
	for (const FString& Name : RegisteredConsoleCommandNames)
	{
		if (IConsoleObject* Existing = IConsoleManager::Get().FindConsoleObject(*Name))
		{
			IConsoleManager::Get().UnregisterConsoleObject(Existing);
		}
	}
	RegisteredConsoleCommandNames.Reset();
	ConsoleCommands.Reset();

	Super::Deinitialize();
}

void URLRecorderSubsystem::RegisterConsoleCommands()
{
	// A PIE session with a client creates this subsystem once per world, and a console name may only be
	// registered once. Claim each name only if it is still free, and remember what we claimed so teardown
	// removes exactly our own entries.
	auto Claim = [this](const TCHAR* Name, IConsoleObject* Registered)
	{
		if (Registered)
		{
			ConsoleCommands.Add(Registered);
			RegisteredConsoleCommandNames.Add(Name);
		}
	};

	// Console commands rather than editor-only tooling: they work in a packaged build, which is the point -
	// a player has to be able to record their own matches without the editor or elevated rights.
	if (!IConsoleManager::Get().FindConsoleObject(TEXT("rts.rl.record.start")))
	{
		Claim(TEXT("rts.rl.record.start"), (IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("rts.rl.record.start"),
		TEXT("Start recording (state, action) pairs for RL training. Optional argument: session label."),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			const FString Label = Args.Num() > 0 ? Args[0] : TEXT("session");
			if (StartRecording(Label))
			{
				UE_LOG(LogTemp, Log, TEXT("[RLRecorder] Recording to %s"), *GetCurrentFilePath());
			}
		}),
		ECVF_Default)));
	}

	if (!IConsoleManager::Get().FindConsoleObject(TEXT("rts.rl.record.stop")))
	{
		Claim(TEXT("rts.rl.record.stop"), (IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("rts.rl.record.stop"),
		TEXT("Stop recording and flush the file."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			const int32 Count = GetSampleCount();
			const FString Path = GetCurrentFilePath();
			StopRecording();
			UE_LOG(LogTemp, Log, TEXT("[RLRecorder] Wrote %d samples to %s"), Count, *Path);
		}),
		ECVF_Default)));
	}

	if (!IConsoleManager::Get().FindConsoleObject(TEXT("rts.rl.ui")))
	{
		Claim(TEXT("rts.rl.ui"), (IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("rts.rl.ui"),
		TEXT("Toggle the RL configuration panel. Needs rts.rl.ui.widget to point at a widget class."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]() { ToggleConfigWidget(); }),
		ECVF_Default)));
	}

	if (!IConsoleManager::Get().FindConsoleObject(TEXT("rts.rl.record.status")))
	{
		Claim(TEXT("rts.rl.record.status"), (IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("rts.rl.record.status"),
		TEXT("Print whether a recording is running, how many samples it holds and where it writes."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			UE_LOG(LogTemp, Log, TEXT("[RLRecorder] recording=%s samples=%d file=%s dir=%s"),
				IsRecording() ? TEXT("yes") : TEXT("no"), GetSampleCount(),
				*GetCurrentFilePath(), *GetRecordingDirectory());
		}),
		ECVF_Default)));
	}
}

bool URLRecorderSubsystem::StartRecording(const FString& SessionLabel)
{
	StopRecording();

	const FString Directory = GetRecordingDirectory();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Directory) && !PlatformFile.CreateDirectoryTree(*Directory))
	{
		UE_LOG(LogTemp, Error, TEXT("[RLRecorder] Could not create '%s'; recording not started."), *Directory);
		return false;
	}

	const FString Label = SessionLabel.IsEmpty() ? TEXT("session") : SessionLabel;
	const FString Stamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	CurrentFilePath = FPaths::Combine(Directory, FString::Printf(TEXT("%s_%s.jsonl"), *Label, *Stamp));

	// Create the file up front so a UI can show the path immediately and a failure surfaces now, not on the
	// first sample half a match later.
	if (!FFileHelper::SaveStringToFile(FString(), *CurrentFilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("[RLRecorder] Could not open '%s' for writing."), *CurrentFilePath);
		CurrentFilePath.Reset();
		return false;
	}

	SampleCount = 0;
	PendingLines.Reset();
	bIsRecording = true;

	UE_LOG(LogTemp, Log, TEXT("[RLRecorder] Recording to '%s'."), *CurrentFilePath);
	return true;
}

void URLRecorderSubsystem::StopRecording()
{
	if (!bIsRecording)
	{
		return;
	}

	FlushBuffer();
	bIsRecording = false;

	UE_LOG(LogTemp, Log, TEXT("[RLRecorder] Stopped after %d samples: '%s'."), SampleCount, *CurrentFilePath);
	CurrentFilePath.Reset();
}

void URLRecorderSubsystem::ToggleConfigWidget()
{
	if (ConfigWidget)
	{
		ConfigWidget->RemoveFromParent();
		ConfigWidget = nullptr;
		return;
	}

	if (GRLUiWidgetPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[RLRecorder] rts.rl.ui.widget is not set - nothing to open."));
		return;
	}

	UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, *GRLUiWidgetPath);
	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RLRecorder] Could not load widget class '%s'."), *GRLUiWidgetPath);
		return;
	}

	APlayerController* Controller = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
	if (!Controller)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RLRecorder] No local player controller to own the panel."));
		return;
	}

	ConfigWidget = CreateWidget<UUserWidget>(Controller, WidgetClass);
	if (ConfigWidget)
	{
		ConfigWidget->AddToViewport(1000);
	}
}

URLRecorderSubsystem* URLRecorderSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<URLRecorderSubsystem>() : nullptr;
}

void URLRecorderSubsystem::OpenRecordingFolder()
{
	const FString Directory = GetRecordingDirectory();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Directory))
	{
		PlatformFile.CreateDirectoryTree(*Directory);
	}
	FPlatformProcess::ExploreFolder(*Directory);
}

int32 URLRecorderSubsystem::ClearRecordings()
{
	// Stop first: deleting the file we are appending to would leave the handle writing into nothing.
	StopRecording();

	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(GetRecordingDirectory() / TEXT("*.jsonl")), true, false);

	int32 Removed = 0;
	for (const FString& File : Files)
	{
		if (IFileManager::Get().Delete(*(GetRecordingDirectory() / File)))
		{
			++Removed;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[RLRecorder] Deleted %d recording(s)."), Removed);
	return Removed;
}

FRLRecordingStatus URLRecorderSubsystem::GetStatus() const
{
	FRLRecordingStatus Status;
	Status.bRecording = bIsRecording;
	Status.SampleCount = SampleCount;
	Status.FilePath = CurrentFilePath;
	Status.Directory = GetRecordingDirectory();

	// The stored-files figures need a directory scan, and a UI polling this from Tick would do that every
	// frame. Refresh it at most once a second so callers can be as naive as they like.
	const double Now = FPlatformTime::Seconds();
	if (Now - CachedListingTime > 1.0)
	{
		CachedListingTime = Now;

		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *(Status.Directory / TEXT("*.jsonl")), true, false);
		CachedFileCount = Files.Num();

		int64 TotalBytes = 0;
		for (const FString& File : Files)
		{
			TotalBytes += IFileManager::Get().FileSize(*(Status.Directory / File));
		}
		CachedMegabytes = static_cast<float>(TotalBytes) / (1024.f * 1024.f);
	}

	Status.StoredFileCount = CachedFileCount;
	Status.StoredMegabytes = CachedMegabytes;

	Status.Summary = bIsRecording
		? FText::FromString(FString::Printf(TEXT("Recording - %d samples this session | %d file(s), %.1f MB stored"),
			SampleCount, Status.StoredFileCount, Status.StoredMegabytes))
		: FText::FromString(FString::Printf(TEXT("Idle | %d file(s), %.1f MB stored"),
			Status.StoredFileCount, Status.StoredMegabytes));

	return Status;
}

void URLRecorderSubsystem::SetSourceFilter(ERLSampleSource Source, bool bEnabled)
{
	const uint8 Bit = 1 << static_cast<uint8>(Source);
	EnabledSourceMask = bEnabled ? (EnabledSourceMask | Bit) : (EnabledSourceMask & ~Bit);
}

bool URLRecorderSubsystem::IsSourceEnabled(ERLSampleSource Source) const
{
	return (EnabledSourceMask & (1 << static_cast<uint8>(Source))) != 0;
}

void URLRecorderSubsystem::RecordSample(int32 TeamId, const TArray<float>& State, int32 ActionIndex, ERLSampleSource Source)
{
	if (!bIsRecording || !IsSourceEnabled(Source) || State.Num() == 0)
	{
		return;
	}

	// ERTSAIAction::None (255) means "the brain declined to act". Those are not decisions and would teach the
	// network to do nothing, so they never reach the file.
	if (ActionIndex < 0 || ActionIndex >= 255)
	{
		return;
	}

	FString StateCsv;
	StateCsv.Reserve(State.Num() * 10);
	for (int32 i = 0; i < State.Num(); ++i)
	{
		if (i > 0) StateCsv.AppendChar(TEXT(','));
		StateCsv.Appendf(TEXT("%.4f"), State[i]);
	}

	const float TimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const FString Line = FString::Printf(
		TEXT("{\"t\":%.2f,\"team\":%d,\"src\":\"%s\",\"s\":[%s],\"a\":%d}"),
		TimeSeconds, TeamId, SourceToToken(Source), *StateCsv, ActionIndex);

	{
		FScopeLock Lock(&BufferLock);
		PendingLines.Add(Line);
		++SampleCount;

		if (PendingLines.Num() < FlushThreshold)
		{
			return;
		}
	}

	FlushBuffer();
}

void URLRecorderSubsystem::RecordSampleFromGameState(int32 TeamId, const FGameStateData& GameState, int32 ActionIndex, ERLSampleSource Source)
{
	if (!bIsRecording || !IsSourceEnabled(Source))
	{
		return;
	}

	RecordSample(TeamId, UInferenceComponent::StateToArray(GameState), ActionIndex, Source);
}

void URLRecorderSubsystem::FlushBuffer()
{
	TArray<FString> ToWrite;
	{
		FScopeLock Lock(&BufferLock);
		if (PendingLines.Num() == 0 || CurrentFilePath.IsEmpty())
		{
			return;
		}
		ToWrite = MoveTemp(PendingLines);
		PendingLines.Reset();
	}

	FString Blob;
	for (const FString& Line : ToWrite)
	{
		Blob += Line;
		Blob += LINE_TERMINATOR;
	}

	if (!FFileHelper::SaveStringToFile(Blob, *CurrentFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
		&IFileManager::Get(), EFileWrite::FILEWRITE_Append))
	{
		UE_LOG(LogTemp, Error, TEXT("[RLRecorder] Append to '%s' failed; %d samples lost."), *CurrentFilePath, ToWrite.Num());
	}
}
