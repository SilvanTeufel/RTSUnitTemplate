// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeatherActor.generated.h"

class UAudioComponent;
class UMaterialInterface;
class UMaterialParameterCollection;
class UPostProcessComponent;
class USoundBase;

/** What kind of weather event it is (all drawn by the one post process material). */
UENUM(BlueprintType)
enum class EWeatherKind : uint8
{
	Rain,
	Wind UMETA(DisplayName = "Wind Gusts")
};

/** One stretch of the weather curve: the intensity goes from From to To in Duration seconds from StartTime (server world time). */
USTRUCT(BlueprintType)
struct FWeatherRainSegment
{
	GENERATED_BODY()

	/** The current weather event (set when it starts, stays while it fades out). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather")
	EWeatherKind Kind = EWeatherKind::Rain;

	/** Wind direction of the event (yaw, degrees). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather")
	float WindYaw = 0.f;

	/**
	 * How stormy this rain is (0..1, rolled per event in MinStorm..MaxStorm): heavier rain, more slanted in the wind,
	 * showers sweeping over the land and more frequent lightning.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Weather")
	float Storm = 0.5f;

	UPROPERTY(BlueprintReadOnly, Category = "Weather")
	float From = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Weather")
	float To = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Weather")
	float StartTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Weather")
	float Duration = 0.f;
};

/** Sunbeams in fine weather: from StartTime (server world time) they fade in, hold Duration seconds and fade out. */
USTRUCT(BlueprintType)
struct FWeatherSunbeams
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Weather")
	float StartTime = -1000.f;

	UPROPERTY(BlueprintReadOnly, Category = "Weather")
	float Duration = 0.f;

	/** Where the gaps in the clouds lie this time (0..1). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather")
	float Seed = 0.f;
};

/**
 * Purely optical weather that comes and goes: rain, wind gusts or whirlwinds. Put one into a map that should have
 * weather (at ground height) - maps without it pay nothing.
 *
 * The server rolls calm phases and weather events (rain or wind gusts by RainWeight / WindWeight; each rain gets its
 * own storminess: strength, slant, sweeping showers, lightning) and
 * replicates only the current stretch of the weather curve. Every machine drives its own look from it: the parameter
 * collection (RainIntensity, RainStorm, WindIntensity, WeatherWind, Lightning, Wetness, LensWet, AfterRain, SunDir,
 * RainCamHeight, RainCamOffset, LensDrop0..3) that the weather post process material reads, the post process itself
 * and the rain or wind sound.
 * Now and then in fine weather (dry, calm, no after-rain look) the clouds open for a few seconds and shafts of sunlight
 * fall slanting onto the land (Sunbeams).
 * After a rain the clouds break up for a short while (AfterRain: warm patches of sun drifting over the land, a light
 * haze over the wet ground) - once per rain, then gone until the next one.
 * While it is dry, the ground has dried and the after-rain look is over, the actor does not tick, the post process
 * is disabled (no pass at all) and the sound is stopped.
 *
 * Console: RTS.Weather.Rain <0..1> [blend] [storm 0..1] / RTS.Weather.Wind <0..1> force that weather (server), -1
 * hands it back to the random phases. RTS.Weather.Sunbeams [seconds] lets the sunbeams come now (server).
 * RTS.Weather.LensDrop adds a drop on the camera, RTS.Weather.LensDrops 0 turns them off.
 */
UCLASS(meta = (DisplayName = "Weather"))
class RTSUNITTEMPLATE_API AWeatherActor : public AActor
{
	GENERATED_BODY()

public:
	AWeatherActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Off: no rain at all in this map. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	bool bEnabled = true;

	/** Dry time at the start of a match, before the first rain (seconds, random in the range). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Timing", meta = (ClampMin = "0"))
	float MinFirstDryTime = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Timing", meta = (ClampMin = "0"))
	float MaxFirstDryTime = 90.f;

	/** Dry time between two rains. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Timing", meta = (ClampMin = "1"))
	float MinDryTime = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Timing", meta = (ClampMin = "1"))
	float MaxDryTime = 150.f;

	/** How long a weather event (rain, wind, whirlwinds) lasts, fade in and out not counted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Timing", meta = (ClampMin = "1"))
	float MinRainTime = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Timing", meta = (ClampMin = "1"))
	float MaxRainTime = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Timing", meta = (ClampMin = "0.1"))
	float FadeInTime = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Timing", meta = (ClampMin = "0.1"))
	float FadeOutTime = 15.f;

	/** How often each kind of weather event comes (relative weights). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Kinds", meta = (ClampMin = "0"))
	float RainWeight = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Kinds", meta = (ClampMin = "0"))
	float WindWeight = 0.55f;

	/** Storminess of a rain, rolled per rain in this range (keep it narrow for weather that varies, but not wildly). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Kinds", meta = (ClampMin = "0", ClampMax = "1"))
	float MinStorm = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Kinds", meta = (ClampMin = "0", ClampMax = "1"))
	float MaxStorm = 0.85f;

	/**
	 * Level of an event: its base lies between MinIntensity and MaxIntensity by its storminess, and every
	 * MinGustTime..MaxGustTime seconds it changes by up to IntensityVariation around that base.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Intensity", meta = (ClampMin = "0", ClampMax = "1"))
	float MinIntensity = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Intensity", meta = (ClampMin = "0", ClampMax = "1"))
	float MaxIntensity = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Intensity", meta = (ClampMin = "0", ClampMax = "1"))
	float IntensityVariation = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Intensity", meta = (ClampMin = "1"))
	float MinGustTime = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Intensity", meta = (ClampMin = "1"))
	float MaxGustTime = 25.f;

	/** Seconds one intensity change takes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Intensity", meta = (ClampMin = "0.1"))
	float GustBlendTime = 6.f;

	/** Seconds of full rain until the ground is fully wet, and seconds until it has dried again. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Wetness", meta = (ClampMin = "0.1"))
	float WetnessRiseTime = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Wetness", meta = (ClampMin = "0.1"))
	float WetnessDryTime = 45.f;

	/** Seconds the small drops on the camera need to dry up after the rain. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Wetness", meta = (ClampMin = "0.1"))
	float LensDryTime = 12.f;

	/** After a rain the clouds break up for a short while: warm patches of sun drift over the land, a light haze rises. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|After Rain")
	bool bAfterRain = true;

	/** Only after a rain at least this strong. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|After Rain", meta = (ClampMin = "0", ClampMax = "1"))
	float AfterRainMinIntensity = 0.3f;

	/** Seconds it takes to come (starts halfway through the fade out of the rain), stays and fades away. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|After Rain", meta = (ClampMin = "0.1"))
	float AfterRainFadeInTime = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|After Rain", meta = (ClampMin = "0"))
	float AfterRainHoldTime = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|After Rain", meta = (ClampMin = "0.1"))
	float AfterRainFadeOutTime = 10.f;

	/** Now and then in fine weather the clouds open for a few seconds and shafts of sunlight fall onto the land. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Sunbeams")
	bool bSunbeams = true;

	/** Seconds between two tries (random in the range); a try only shows when it is dry, calm and the next weather is not near. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Sunbeams", meta = (ClampMin = "5"))
	float MinSunbeamTime = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Sunbeams", meta = (ClampMin = "5"))
	float MaxSunbeamTime = 80.f;

	/** Seconds the beams hold at full strength (fading in and out comes on top: SunbeamFadeInTime / SunbeamFadeOutTime). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Sunbeams", meta = (ClampMin = "0.5"))
	float MinSunbeamDuration = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Sunbeams", meta = (ClampMin = "0.5"))
	float MaxSunbeamDuration = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Sunbeams", meta = (ClampMin = "0.1"))
	float SunbeamFadeInTime = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Sunbeams", meta = (ClampMin = "0.1"))
	float SunbeamFadeOutTime = 2.f;

	/** Read by the rain material: RainIntensity, Wetness, LensWet, AfterRain, SunDir, RainCamHeight, LensDrop0..LensDrop3 (x, y, age, size). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Look")
	TObjectPtr<UMaterialParameterCollection> ParameterCollection;

	/** Post process material (rain, wet ground, lens drops). Only rendered while it rains or the ground is wet. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Look")
	TObjectPtr<UMaterialInterface> RainMaterial;

	/** Looping rain sound, its volume follows the intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Look")
	TObjectPtr<USoundBase> RainSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Look", meta = (ClampMin = "0"))
	float RainSoundVolume = 0.8f;

	/** Looping wind sound for wind gusts and whirlwinds, its volume follows the intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Look")
	TObjectPtr<USoundBase> WindSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Look", meta = (ClampMin = "0"))
	float WindSoundVolume = 0.5f;

	/** Now and then a lightning flash in heavy rain (the material brightens the picture briefly), the thunder follows. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Lightning")
	bool bLightning = true;

	/** During rain at least this strong; the stormier the rain, the more often (MaxLightningTime calm .. MinLightningTime stormy). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Lightning", meta = (ClampMin = "0", ClampMax = "1"))
	float LightningMinIntensity = 0.35f;

	/** Seconds between two flashes (random in the range). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Lightning", meta = (ClampMin = "1"))
	float MinLightningTime = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Lightning", meta = (ClampMin = "1"))
	float MaxLightningTime = 60.f;

	/** Thunder sounds (one at random), MinThunderDelay..MaxThunderDelay seconds after the flash - the storm is far away. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Lightning")
	TArray<TObjectPtr<USoundBase>> ThunderSounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Lightning", meta = (ClampMin = "0"))
	float ThunderVolume = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Lightning", meta = (ClampMin = "0"))
	float MinThunderDelay = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Lightning", meta = (ClampMin = "0"))
	float MaxThunderDelay = 4.f;

	/** Now and then a drop hits the camera, leaves a spot and runs down (at full rain every MinLensDropTime..MaxLensDropTime seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Lens Drops")
	bool bLensDrops = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Lens Drops", meta = (ClampMin = "0.5"))
	float MinLensDropTime = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Lens Drops", meta = (ClampMin = "0.5"))
	float MaxLensDropTime = 10.f;

	/** No drops below this rain intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Lens Drops", meta = (ClampMin = "0", ClampMax = "1"))
	float LensDropMinIntensity = 0.15f;

	/** Radius of a running drop (share of the screen height). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Lens Drops", meta = (ClampMin = "0.001"))
	float MinLensDropSize = 0.012f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Lens Drops", meta = (ClampMin = "0.001"))
	float MaxLensDropSize = 0.022f;

	/** Seconds a running lens drop lives (the material fades it out in the last 1.5 s of 9). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Lens Drops", meta = (ClampMin = "1"))
	float LensDropLifetime = 9.f;

	/** Current rain intensity (0..1) on this machine. */
	UFUNCTION(BlueprintPure, Category = "Weather")
	float GetRainIntensity() const;

	/** The current (or last) weather event. */
	UFUNCTION(BlueprintPure, Category = "Weather")
	EWeatherKind GetWeatherKind() const { return RainSegment.Kind; }

	/** Server: forces that weather to Intensity (0..1) within BlendTime seconds and holds it; Intensity < 0 hands it back to the random phases. */
	UFUNCTION(BlueprintCallable, Category = "Weather")
	void ForceWeather(EWeatherKind Kind, float Intensity, float BlendTime = 3.f);

	/** Server: sets the storminess of the current event (0..1). */
	UFUNCTION(BlueprintCallable, Category = "Weather")
	void ForceStorm(float Storm);

	/** Server: forces the rain to Intensity (0..1) within BlendTime seconds and holds it; Intensity < 0 hands it back to the random phases. */
	UFUNCTION(BlueprintCallable, Category = "Weather")
	void ForceRain(float Intensity, float BlendTime = 3.f);

	/** Server: the sunbeams come now for Duration seconds (also when the weather is not fine - for tests). */
	UFUNCTION(BlueprintCallable, Category = "Weather")
	void StartSunbeams(float Duration);

	/** Local: a drop hits the camera now. */
	UFUNCTION(BlueprintCallable, Category = "Weather")
	void AddLensDrop();

protected:
	UPROPERTY(VisibleAnywhere, Category = "Weather")
	TObjectPtr<UPostProcessComponent> PostProcess;

	UPROPERTY(ReplicatedUsing = OnRep_RainSegment, BlueprintReadOnly, Category = "Weather")
	FWeatherRainSegment RainSegment;

	UFUNCTION()
	void OnRep_RainSegment();

	UPROPERTY(ReplicatedUsing = OnRep_Sunbeams, BlueprintReadOnly, Category = "Weather")
	FWeatherSunbeams Sunbeams;

	UFUNCTION()
	void OnRep_Sunbeams();

private:
	enum class EPhase : uint8 { Dry, Raining, Ending, Forced };

	/** Server: next step of the random phases. */
	void AdvancePhase();

	/** Server: starts a new stretch of the rain curve from the current intensity. */
	void SetRainTarget(float To, float Duration);

	/** Seconds on the server clock (about the same on every machine). */
	double GetServerTime() const;

	/** Something changed - the local look follows it (ticks until everything is dry again). */
	void WakeLook();

	/** Writes the look; false when there is nothing left to show (dry, no drops). */
	bool UpdateLook(float DeltaSeconds);

	/** A new level for the current event, around its base. */
	float RollLevel() const;

	/** Writes the parameter collection. */
	void WriteParameters(float Intensity, float CamHeight) const;

	/** Strength of the after-rain look (0..1) now - the same on every machine, from the rain curve. */
	float GetAfterRain(double Now) const;

	/** Server: a try for sunbeams - they come when it is fine weather and the next weather event is not near. */
	void TrySunbeams();

	/** Strength of the sunbeams (0..1) now - the same on every machine. */
	float GetSunbeams(double Now) const;

	FTimerHandle SunbeamTimer;
	float SunbeamLevel = 0.f;

	/** Direction to the sun (the brightest directional light of the map). */
	FVector SunDirection = FVector(0.3, 0.3, 0.9).GetSafeNormal();

	/**
	 * How far the camera has moved sideways (X) and forward (Y) along its own horizontal axes while it rained. The
	 * material shifts the rain layers around the camera by it, so the rain stays put in the world when the camera pans.
	 */
	FVector2D RainCamOffset = FVector2D::ZeroVector;
	FVector PrevCamLocation = FVector::ZeroVector;
	bool bHasPrevCamLocation = false;

	EPhase Phase = EPhase::Dry;
	double RainEndTime = 0.0;
	FTimerHandle PhaseTimer;

	float Wetness = 0.f;
	float LensWet = 0.f;

	/** Lightning: seconds since the flash (< 0 none), when the next one comes and when its thunder is heard (local). */
	float LightningAge = -1.f;
	double NextLightningTime = 0.0;
	double ThunderTime = 0.0;
	float LightningFlash = 0.f;
	float AfterRain = 0.f;
	double NextLensDropTime = 0.0;

	struct FLensDrop
	{
		FVector2f Pos = FVector2f::ZeroVector;
		float Age = -1.f;
		float Size = 0.f;
	};
	FLensDrop LensDrops[4];

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> RainAudio;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> WindAudio;

	/** Starts, adjusts or stops a looping 2D sound. */
	void UpdateLoopSound(TObjectPtr<UAudioComponent>& Audio, USoundBase* Sound, float Volume);
};
