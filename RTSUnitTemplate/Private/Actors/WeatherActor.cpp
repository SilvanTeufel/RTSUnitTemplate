// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/WeatherActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/PostProcessComponent.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

static TAutoConsoleVariable<int32> CVarWeatherLensDrops(
	TEXT("RTS.Weather.LensDrops"), 1,
	TEXT("1 = rain drops now and then hit the camera and run down, 0 = off."));

static void ForceWeatherCommand(EWeatherKind Kind, const TArray<FString>& Args, UWorld* World)
{
	if (!World || Args.Num() < 1)
	{
		return;
	}
	const float Intensity = FCString::Atof(*Args[0]);
	const float Blend = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 3.f;
	for (TActorIterator<AWeatherActor> It(World); It; ++It)
	{
		It->ForceWeather(Kind, Intensity, Blend);
		if (Args.Num() > 2)
		{
			It->ForceStorm(FCString::Atof(*Args[2]));
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs CmdWeatherRain(
	TEXT("RTS.Weather.Rain"),
	TEXT("RTS.Weather.Rain <0..1> [blend seconds]: forces the rain (server). -1 hands it back to the random phases."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ForceWeatherCommand(EWeatherKind::Rain, Args, World);
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdWeatherWind(
	TEXT("RTS.Weather.Wind"),
	TEXT("RTS.Weather.Wind <0..1> [blend seconds]: forces wind gusts (server). -1 hands it back to the random phases."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ForceWeatherCommand(EWeatherKind::Wind, Args, World);
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdWeatherSunbeams(
	TEXT("RTS.Weather.Sunbeams"),
	TEXT("RTS.Weather.Sunbeams [seconds]: the sunbeams come now (server), default 5 s."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}
		const float Duration = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 5.f;
		for (TActorIterator<AWeatherActor> It(World); It; ++It)
		{
			It->StartSunbeams(Duration);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdWeatherLensDrop(
	TEXT("RTS.Weather.LensDrop"),
	TEXT("RTS.Weather.LensDrop: a rain drop hits the camera now (local)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}
		for (TActorIterator<AWeatherActor> It(World); It; ++It)
		{
			It->AddLensDrop();
		}
	}));

AWeatherActor::AWeatherActor()
{
	PrimaryActorTick.bCanEverTick = true;
	// Only ticks while something is to be shown (WakeLook) - a dry map costs nothing.
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(1.f);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("RainPostProcess"));
	PostProcess->SetupAttachment(RootComponent);
	PostProcess->bUnbound = true;
	// Disabled = the post process pass is not rendered at all.
	PostProcess->bEnabled = false;
}

void AWeatherActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWeatherActor, RainSegment);
	DOREPLIFETIME(AWeatherActor, Sunbeams);
}

void AWeatherActor::BeginPlay()
{
	Super::BeginPlay();
	if (!bEnabled)
	{
		return;
	}
	if (RainMaterial)
	{
		PostProcess->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, RainMaterial));
	}
	// The sun is the brightest directional light; it shines along its forward axis, the sun is the other way.
	float SunIntensity = -1.f;
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		const ULightComponent* Light = It->GetLightComponent();
		if (Light && Light->IsVisible() && Light->Intensity > SunIntensity)
		{
			SunIntensity = Light->Intensity;
			SunDirection = -It->GetActorForwardVector();
		}
	}
	WriteParameters(0.f, 3000.f);
	if (HasAuthority())
	{
		Phase = EPhase::Dry;
		GetWorldTimerManager().SetTimer(PhaseTimer, this, &AWeatherActor::AdvancePhase,
			FMath::Max(0.1f, FMath::FRandRange(MinFirstDryTime, FMath::Max(MinFirstDryTime, MaxFirstDryTime))), false);
		if (bSunbeams)
		{
			GetWorldTimerManager().SetTimer(SunbeamTimer, this, &AWeatherActor::TrySunbeams,
				FMath::FRandRange(MinSunbeamTime, FMath::Max(MinSunbeamTime, MaxSunbeamTime)), false);
		}
	}
	// A client joining while it rains gets the stretch with the first replication (OnRep wakes the look).
}

void AWeatherActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(PhaseTimer);
	GetWorldTimerManager().ClearTimer(SunbeamTimer);
	UpdateLoopSound(RainAudio, nullptr, 0.f);
	UpdateLoopSound(WindAudio, nullptr, 0.f);
	Super::EndPlay(EndPlayReason);
}

double AWeatherActor::GetServerTime() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0;
	}
	const AGameStateBase* GameState = World->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

float AWeatherActor::GetAfterRain(double Now) const
{
	const FWeatherRainSegment& S = RainSegment;
	if (!bAfterRain || S.Kind != EWeatherKind::Rain || S.To > 0.f || S.From < AfterRainMinIntensity)
	{
		return 0.f;
	}
	// The sun comes out while the last drizzle fades.
	const float T = float(Now - (S.StartTime + 0.5 * S.Duration));
	if (T <= 0.f)
	{
		return 0.f;
	}
	const float In = FMath::SmoothStep(0.f, 1.f, T / AfterRainFadeInTime);
	const float Out = 1.f - FMath::SmoothStep(0.f, 1.f, (T - AfterRainFadeInTime - AfterRainHoldTime) / AfterRainFadeOutTime);
	return FMath::Clamp(FMath::Min(In, Out), 0.f, 1.f);
}

float AWeatherActor::GetSunbeams(double Now) const
{
	const float T = float(Now - Sunbeams.StartTime);
	if (T <= 0.f || Sunbeams.Duration <= 0.f)
	{
		return 0.f;
	}
	const float In = FMath::SmoothStep(0.f, 1.f, T / SunbeamFadeInTime);
	const float Out = 1.f - FMath::SmoothStep(0.f, 1.f, (T - SunbeamFadeInTime - Sunbeams.Duration) / SunbeamFadeOutTime);
	return FMath::Clamp(FMath::Min(In, Out), 0.f, 1.f);
}

void AWeatherActor::TrySunbeams()
{
	const double Now = GetServerTime();
	const float Duration = FMath::FRandRange(MinSunbeamDuration, FMath::Max(MinSunbeamDuration, MaxSunbeamDuration));
	// Fine weather only: dry and calm, no after-rain look, and the beams are gone before the next event starts.
	const float Total = SunbeamFadeInTime + Duration + SunbeamFadeOutTime;
	const float UntilNext = GetWorldTimerManager().IsTimerActive(PhaseTimer) ? GetWorldTimerManager().GetTimerRemaining(PhaseTimer) : 0.f;
	if (Phase == EPhase::Dry && GetRainIntensity() < 0.01f && GetAfterRain(Now) <= 0.f && UntilNext > Total + 2.f)
	{
		StartSunbeams(Duration);
	}
	GetWorldTimerManager().SetTimer(SunbeamTimer, this, &AWeatherActor::TrySunbeams,
		FMath::FRandRange(MinSunbeamTime, FMath::Max(MinSunbeamTime, MaxSunbeamTime)), false);
}

void AWeatherActor::StartSunbeams(float Duration)
{
	if (!HasAuthority() || !bEnabled)
	{
		return;
	}
	Sunbeams.StartTime = float(GetServerTime());
	Sunbeams.Duration = FMath::Max(0.5f, Duration);
	Sunbeams.Seed = FMath::FRand();
	UE_LOG(LogTemp, Log, TEXT("[Weather] Sunbeams for %.1f s."), Sunbeams.Duration);
	ForceNetUpdate();
	WakeLook();
}

void AWeatherActor::OnRep_Sunbeams()
{
	WakeLook();
}

float AWeatherActor::GetRainIntensity() const
{
	const FWeatherRainSegment& S = RainSegment;
	const float Alpha = S.Duration > 0.f ? FMath::Clamp(float(GetServerTime() - S.StartTime) / S.Duration, 0.f, 1.f) : 1.f;
	return FMath::Clamp(FMath::Lerp(S.From, S.To, FMath::SmoothStep(0.f, 1.f, Alpha)), 0.f, 1.f);
}

void AWeatherActor::SetRainTarget(float To, float Duration)
{
	RainSegment.From = GetRainIntensity();
	RainSegment.To = FMath::Clamp(To, 0.f, 1.f);
	RainSegment.StartTime = float(GetServerTime());
	RainSegment.Duration = FMath::Max(0.01f, Duration);
	ForceNetUpdate();
	WakeLook();
}

void AWeatherActor::AdvancePhase()
{
	const double Now = GetServerTime();
	float Next = 1.f;
	switch (Phase)
	{
	case EPhase::Dry:
	{
		// Which weather comes this time (it starts from calm, so the switch is not seen).
		const float Total = FMath::Max(0.f, RainWeight) + FMath::Max(0.f, WindWeight);
		RainSegment.Kind = Total <= 0.f || FMath::FRand() * Total < RainWeight ? EWeatherKind::Rain : EWeatherKind::Wind;
		RainSegment.WindYaw = FMath::FRandRange(0.f, 360.f);
		RainSegment.Storm = FMath::FRandRange(MinStorm, FMath::Max(MinStorm, MaxStorm));
		Phase = EPhase::Raining;
		RainEndTime = Now + FadeInTime + FMath::FRandRange(MinRainTime, FMath::Max(MinRainTime, MaxRainTime));
		SetRainTarget(RollLevel(), FadeInTime);
		Next = FadeInTime + FMath::FRandRange(MinGustTime, FMath::Max(MinGustTime, MaxGustTime));
		break;
	}
	case EPhase::Raining:
		if (Now >= RainEndTime)
		{
			Phase = EPhase::Ending;
			SetRainTarget(0.f, FadeOutTime);
			Next = FadeOutTime;
		}
		else
		{
			SetRainTarget(RollLevel(), GustBlendTime);
			Next = FMath::Max(GustBlendTime, FMath::Min(FMath::FRandRange(MinGustTime, FMath::Max(MinGustTime, MaxGustTime)), float(RainEndTime - Now)));
		}
		break;
	case EPhase::Ending:
		Phase = EPhase::Dry;
		Next = FMath::FRandRange(MinDryTime, FMath::Max(MinDryTime, MaxDryTime));
		break;
	case EPhase::Forced:
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[Weather] Phase %d, kind %d, %.2f -> %.2f, next step in %.0f s."), int32(Phase), int32(RainSegment.Kind), RainSegment.From, RainSegment.To, Next);
	GetWorldTimerManager().SetTimer(PhaseTimer, this, &AWeatherActor::AdvancePhase, FMath::Max(0.1f, Next), false);
}

float AWeatherActor::RollLevel() const
{
	const float Base = FMath::Lerp(MinIntensity, MaxIntensity, RainSegment.Storm);
	return FMath::Clamp(Base + FMath::FRandRange(-IntensityVariation, IntensityVariation), 0.05f, 1.f);
}

void AWeatherActor::ForceStorm(float Storm)
{
	if (HasAuthority())
	{
		RainSegment.Storm = FMath::Clamp(Storm, 0.f, 1.f);
		ForceNetUpdate();
	}
}

void AWeatherActor::ForceRain(float Intensity, float BlendTime)
{
	ForceWeather(EWeatherKind::Rain, Intensity, BlendTime);
}

void AWeatherActor::ForceWeather(EWeatherKind Kind, float Intensity, float BlendTime)
{
	if (!HasAuthority() || !bEnabled)
	{
		return;
	}
	if (Intensity >= 0.f && Kind != RainSegment.Kind)
	{
		RainSegment.Kind = Kind;
		RainSegment.WindYaw = FMath::FRandRange(0.f, 360.f);
		RainSegment.Storm = FMath::FRandRange(MinStorm, FMath::Max(MinStorm, MaxStorm));
	}
	GetWorldTimerManager().ClearTimer(PhaseTimer);
	if (Intensity < 0.f)
	{
		// Back to the random phases: let it fade out, then the next dry phase starts.
		Phase = EPhase::Ending;
		SetRainTarget(0.f, BlendTime);
		GetWorldTimerManager().SetTimer(PhaseTimer, this, &AWeatherActor::AdvancePhase, FMath::Max(0.1f, BlendTime), false);
		return;
	}
	Phase = EPhase::Forced;
	SetRainTarget(Intensity, BlendTime);
}

void AWeatherActor::OnRep_RainSegment()
{
	WakeLook();
}

void AWeatherActor::AddLensDrop()
{
	if (!bEnabled)
	{
		return;
	}
	// A free slot, or else the oldest drop makes room.
	int32 Slot = 0;
	for (int32 i = 0; i < UE_ARRAY_COUNT(LensDrops); ++i)
	{
		if (LensDrops[i].Age < 0.f)
		{
			Slot = i;
			break;
		}
		if (LensDrops[i].Age > LensDrops[Slot].Age)
		{
			Slot = i;
		}
	}
	FLensDrop& Drop = LensDrops[Slot];
	Drop.Pos = FVector2f(FMath::FRandRange(0.04f, 0.96f), FMath::FRandRange(0.0f, 0.6f));
	Drop.Size = FMath::FRandRange(MinLensDropSize, FMath::Max(MinLensDropSize, MaxLensDropSize));
	Drop.Age = 0.f;
	WakeLook();
}

void AWeatherActor::WakeLook()
{
	// Nothing is drawn or heard on a dedicated server.
	if (bEnabled && GetNetMode() != NM_DedicatedServer && !IsActorTickEnabled())
	{
		SetActorTickEnabled(true);
	}
}

void AWeatherActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!UpdateLook(DeltaSeconds))
	{
		// Dry and nothing left to show: back to zero cost.
		WriteParameters(0.f, 3000.f);
		PostProcess->bEnabled = false;
		UpdateLoopSound(RainAudio, nullptr, 0.f);
		UpdateLoopSound(WindAudio, nullptr, 0.f);
		SetActorTickEnabled(false);
	}
}

bool AWeatherActor::UpdateLook(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	// Level of the current weather event; Intensity is the rain part of it (wetness, drops, rain sound).
	const float Level = GetRainIntensity();
	const float Intensity = RainSegment.Kind == EWeatherKind::Rain ? Level : 0.f;

	// The ground gets wet with the rain and dries slowly after it.
	if (Intensity > 0.05f)
	{
		Wetness = FMath::Min(1.f, Wetness + DeltaSeconds * Intensity / WetnessRiseTime);
	}
	else
	{
		Wetness = FMath::Max(0.f, Wetness - DeltaSeconds / WetnessDryTime);
	}
	// The small drops on the camera follow the rain within a few seconds and dry up after it.
	LensWet = Intensity > LensWet
		? FMath::Min(Intensity, LensWet + DeltaSeconds * 0.3f)
		: FMath::Max(Intensity, LensWet - DeltaSeconds / LensDryTime);
	AfterRain = GetAfterRain(GetServerTime());
	SunbeamLevel = GetSunbeams(GetServerTime());

	bool bAnyDrop = false;
	for (FLensDrop& Drop : LensDrops)
	{
		if (Drop.Age >= 0.f)
		{
			Drop.Age += DeltaSeconds;
			if (Drop.Age > LensDropLifetime)
			{
				Drop.Age = -1.f;
			}
			else
			{
				bAnyDrop = true;
			}
		}
	}
	const double Now = World->GetTimeSeconds();
	if (bLensDrops && CVarWeatherLensDrops.GetValueOnGameThread() > 0 && Intensity >= LensDropMinIntensity)
	{
		// Heavier rain, more drops.
		const float Interval = FMath::FRandRange(MinLensDropTime, FMath::Max(MinLensDropTime, MaxLensDropTime)) / FMath::Max(Intensity, 0.3f);
		if (NextLensDropTime <= 0.0)
		{
			NextLensDropTime = Now + Interval;
		}
		else if (Now >= NextLensDropTime)
		{
			AddLensDrop();
			bAnyDrop = true;
			NextLensDropTime = Now + Interval;
		}
	}
	else
	{
		NextLensDropTime = 0.0;
	}

	// Lightning in heavy rain: a short double flicker, the thunder follows a few seconds later (local, purely optical).
	if (bLightning && Intensity >= LightningMinIntensity)
	{
		if (NextLightningTime <= 0.0)
		{
			NextLightningTime = Now + FMath::Lerp(MaxLightningTime, MinLightningTime, RainSegment.Storm) * FMath::FRandRange(0.3f, 0.8f);
		}
		else if (Now >= NextLightningTime)
		{
			LightningAge = 0.f;
			ThunderTime = Now + FMath::FRandRange(MinThunderDelay, FMath::Max(MinThunderDelay, MaxThunderDelay));
			// Stormier rain, more lightning - a little random around that.
			NextLightningTime = Now + FMath::Lerp(MaxLightningTime, MinLightningTime, RainSegment.Storm) * FMath::FRandRange(0.8f, 1.25f);
		}
	}
	else
	{
		NextLightningTime = 0.0;
	}
	LightningFlash = 0.f;
	if (LightningAge >= 0.f)
	{
		LightningAge += DeltaSeconds;
		const float A = LightningAge;
		LightningFlash = FMath::Exp(-A * 12.f) + (A > 0.18f ? 0.6f * FMath::Exp(-(A - 0.18f) * 14.f) : 0.f);
		if (A > 0.8f)
		{
			LightningAge = -1.f;
			LightningFlash = 0.f;
		}
	}
	if (ThunderTime > 0.0 && Now >= ThunderTime)
	{
		ThunderTime = 0.0;
		const int32 Count = ThunderSounds.Num();
		USoundBase* Thunder = Count > 0 ? ThunderSounds[FMath::RandRange(0, Count - 1)].Get() : nullptr;
		if (Thunder)
		{
			const AControllerBase* Listener = Cast<AControllerBase>(World->GetFirstPlayerController());
			UGameplayStatics::PlaySound2D(this, Thunder, ThunderVolume * FMath::FRandRange(0.6f, 1.f) * (Listener ? Listener->GetSoundMultiplier() : 1.f),
				FMath::FRandRange(0.9f, 1.05f));
		}
	}

	// The rain layers sit at a share of the camera height above the ground (this actor sits at ground height).
	float CamHeight = 3000.f;
	APlayerController* PC = World->GetFirstPlayerController();
	if (PC && PC->PlayerCameraManager)
	{
		const FVector CamLocation = PC->PlayerCameraManager->GetCameraLocation();
		CamHeight = FMath::Max(300.f, float(CamLocation.Z - GetActorLocation().Z));

		// Sum up the camera moves along its own horizontal axes (a jump, e.g. to a minimap click, is left out).
		const FVector Delta = CamLocation - PrevCamLocation;
		if (bHasPrevCamLocation && Delta.SizeSquared2D() < FMath::Square(4000.0))
		{
			const float Yaw = FMath::DegreesToRadians(PC->PlayerCameraManager->GetCameraRotation().Yaw);
			const FVector2D Forward(FMath::Cos(Yaw), FMath::Sin(Yaw));
			const FVector2D Right(-Forward.Y, Forward.X);
			const FVector2D Delta2D(Delta.X, Delta.Y);
			// Backwards the true parallax lifts the rain faster than it falls (the camera pans at ~5000/s), so it
			// would seem to fly up - that part only counts up to 600/s, the rain keeps falling, just slower.
			const float MaxBackward = 600.f * DeltaSeconds;
			const float Ahead = FMath::Max(float(FVector2D::DotProduct(Delta2D, Forward)), -MaxBackward);
			RainCamOffset.X = FMath::Fmod(RainCamOffset.X + FVector2D::DotProduct(Delta2D, Right), 100000.0);
			RainCamOffset.Y = FMath::Fmod(RainCamOffset.Y + Ahead, 100000.0);
		}
		PrevCamLocation = CamLocation;
		bHasPrevCamLocation = true;
	}
	WriteParameters(Level, CamHeight);

	const bool bVisible = Level > 0.001f || Wetness > 0.001f || LensWet > 0.001f || AfterRain > 0.001f || bAnyDrop || LightningFlash > 0.f
		|| SunbeamLevel > 0.001f;
	PostProcess->bEnabled = bVisible;

	const AControllerBase* ControllerBase = Cast<AControllerBase>(PC);
	const float Multiplier = ControllerBase ? ControllerBase->GetSoundMultiplier() : 1.f;
	UpdateLoopSound(RainAudio, RainSound, Intensity * RainSoundVolume * Multiplier);
	UpdateLoopSound(WindAudio, WindSound, (Level - Intensity) * WindSoundVolume * Multiplier);

	// Also keep going while the after-rain look is still to come.
	const double CurveEnd = double(RainSegment.StartTime) + RainSegment.Duration;
	const bool bAfterRainAhead = bAfterRain && RainSegment.To <= 0.f && RainSegment.From >= AfterRainMinIntensity
		&& GetServerTime() < CurveEnd + AfterRainFadeInTime;
	const bool bSunbeamsAhead = GetServerTime() < double(Sunbeams.StartTime) + SunbeamFadeInTime;
	const bool bCurveRunning = GetServerTime() < CurveEnd || bAfterRainAhead || bSunbeamsAhead;
	return bVisible || bCurveRunning;
}

void AWeatherActor::UpdateLoopSound(TObjectPtr<UAudioComponent>& Audio, USoundBase* Sound, float Volume)
{
	if (Sound && Volume > 0.001f)
	{
		if (!Audio)
		{
			Audio = UGameplayStatics::SpawnSound2D(this, Sound, 1.f, 1.f, 0.f, nullptr, false, false);
		}
		if (Audio)
		{
			Audio->SetVolumeMultiplier(Volume);
			if (!Audio->IsPlaying())
			{
				Audio->Play();
			}
		}
	}
	else if (Audio && Audio->IsPlaying())
	{
		Audio->Stop();
	}
}

void AWeatherActor::WriteParameters(float Intensity, float CamHeight) const
{
	UWorld* World = GetWorld();
	UMaterialParameterCollectionInstance* Collection = (World && ParameterCollection) ? World->GetParameterCollectionInstance(ParameterCollection) : nullptr;
	if (!Collection)
	{
		return;
	}
	static const FName RainIntensityName(TEXT("RainIntensity"));
	static const FName WetnessName(TEXT("Wetness"));
	static const FName CamHeightName(TEXT("RainCamHeight"));
	static const FName LensWetName(TEXT("LensWet"));
	static const FName AfterRainName(TEXT("AfterRain"));
	static const FName SunDirName(TEXT("SunDir"));
	static const FName CamOffsetName(TEXT("RainCamOffset"));
	static const FName DropNames[] = { TEXT("LensDrop0"), TEXT("LensDrop1"), TEXT("LensDrop2"), TEXT("LensDrop3") };
	static const FName LightningName(TEXT("Lightning"));
	Collection->SetScalarParameterValue(LightningName, LightningFlash);
	static const FName WindIntensityName(TEXT("WindIntensity"));
	static const FName RainStormName(TEXT("RainStorm"));
	static const FName WeatherWindName(TEXT("WeatherWind"));
	static const FName SunbeamsName(TEXT("Sunbeams"));
	Collection->SetVectorParameterValue(SunbeamsName, FLinearColor(SunbeamLevel, Sunbeams.Seed, 0.f, 0.f));
	// Intensity is the level of the current event - each kind has its own parameter, the others stay 0.
	const EWeatherKind Kind = RainSegment.Kind;
	Collection->SetScalarParameterValue(RainIntensityName, Kind == EWeatherKind::Rain ? Intensity : 0.f);
	Collection->SetScalarParameterValue(WindIntensityName, Kind == EWeatherKind::Wind ? Intensity : 0.f);
	Collection->SetScalarParameterValue(RainStormName, Kind == EWeatherKind::Rain ? RainSegment.Storm : 0.f);
	const float WindYaw = FMath::DegreesToRadians(RainSegment.WindYaw);
	Collection->SetVectorParameterValue(WeatherWindName, FLinearColor(FMath::Cos(WindYaw), FMath::Sin(WindYaw), 0.f, 0.f));
	Collection->SetScalarParameterValue(WetnessName, Wetness);
	Collection->SetScalarParameterValue(CamHeightName, CamHeight);
	Collection->SetScalarParameterValue(LensWetName, LensWet);
	Collection->SetScalarParameterValue(AfterRainName, AfterRain);
	Collection->SetVectorParameterValue(SunDirName, FLinearColor(SunDirection.X, SunDirection.Y, SunDirection.Z, 0.f));
	Collection->SetVectorParameterValue(CamOffsetName, FLinearColor(RainCamOffset.X, RainCamOffset.Y, 0.f, 0.f));
	for (int32 i = 0; i < UE_ARRAY_COUNT(LensDrops); ++i)
	{
		const FLensDrop& Drop = LensDrops[i];
		Collection->SetVectorParameterValue(DropNames[i], FLinearColor(Drop.Pos.X, Drop.Pos.Y, Drop.Age, Drop.Size));
	}
}
