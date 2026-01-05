// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/PerformanceUnit.h"

#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Widgets/UnitBaseHealthBar.h"
#include "Widgets/SquadHealthBar.h"
#include "Widgets/UnitTimerWidget.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Controller/AIController/UnitControllerBase.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "Engine/SkeletalMesh.h" // For USkeletalMesh
#include "Engine/SkeletalMeshLODSettings.h" // To access LOD settings
#include "Engine/SkinnedAssetCommon.h"
#include "EngineUtils.h"
#include <limits>

#include "Engine/World.h"              // For UWorld, GetWorld()
#include "GameFramework/Actor.h"         // For FActorSpawnParameters, SpawnActo
#include "Logging/LogMacros.h"
#include "NiagaraComponent.h"
#include "Components/AudioComponent.h"
#include "TimerManager.h"
#include "System/StoryTriggerQueueSubsystem.h"
#include "Engine/GameInstance.h"
#include "Controller/PlayerController/ControllerBase.h"

// Debug category for squad healthbar visibility
DEFINE_LOG_CATEGORY_STATIC(LogSquadHB, Log, All);

APerformanceUnit::APerformanceUnit(const FObjectInitializer& ObjectInitializer):Super(ObjectInitializer)
{
	if (RootComponent == nullptr) {
		RootComponent = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("Root"));
	}

	HealthWidgetComp = ObjectInitializer.CreateDefaultSubobject<UWidgetComponent>(this, TEXT("Healthbar"));
	HealthWidgetComp->SetupAttachment(RootComponent);
	HealthWidgetComp->SetVisibility(true);

	float CapsuleHalfHeight = 88.f;
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		CapsuleHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	}

	HealthWidgetRelativeOffset = FVector(0.f, 0.f, CapsuleHalfHeight + HealthWidgetHeightOffset);
	HealthWidgetComp->SetRelativeLocation(HealthWidgetRelativeOffset);

	TimerWidgetComp = ObjectInitializer.CreateDefaultSubobject<UWidgetComponent>(this, TEXT("Timer"));
	TimerWidgetComp->SetupAttachment(RootComponent);
	TimerWidgetRelativeOffset = FVector(0.f, 0.f, CapsuleHalfHeight + TimerWidgetHeightOffset);
	TimerWidgetComp->SetRelativeLocation(TimerWidgetRelativeOffset);
}

void APerformanceUnit::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	CheckViewport();
	CheckTeamVisibility();
	
	if (StopVisibilityTick) return;
		
	if(EnableFog)VisibilityTickFog();
	else SetCharacterVisibility(IsOnViewport);
	
	CheckHealthBarVisibility();
	CheckTimerVisibility();
	
}

void APerformanceUnit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(APerformanceUnit, HealthWidgetComp);
	DOREPLIFETIME(APerformanceUnit, TimerWidgetComp);
	DOREPLIFETIME(APerformanceUnit, HealthWidgetRelativeOffset);
	DOREPLIFETIME(APerformanceUnit, TimerWidgetRelativeOffset);

	DOREPLIFETIME(APerformanceUnit, MeleeImpactVFX);
	DOREPLIFETIME(APerformanceUnit, MeleeImpactSound);
	DOREPLIFETIME(APerformanceUnit, ScaleImpactSound);
	DOREPLIFETIME(APerformanceUnit, ScaleImpactVFX);
	DOREPLIFETIME(APerformanceUnit, RotateImpactVFX);
	DOREPLIFETIME(APerformanceUnit, DeadSound);
	DOREPLIFETIME(APerformanceUnit, DeadVFX);
	DOREPLIFETIME(APerformanceUnit, ScaleDeadSound);
	DOREPLIFETIME(APerformanceUnit, ScaleDeadVFX);
	DOREPLIFETIME(APerformanceUnit, DelayDeadSound);
	DOREPLIFETIME(APerformanceUnit, DelayDeadVFX);

	DOREPLIFETIME(APerformanceUnit, MeeleImpactVFXDelay);
	DOREPLIFETIME(APerformanceUnit, MeleeImpactSoundDelay);

	DOREPLIFETIME(APerformanceUnit, StopVisibilityTick);
	DOREPLIFETIME(APerformanceUnit, AbilityIndicatorVisibility);

	
}

void APerformanceUnit::BeginPlay()
{
	Super::BeginPlay();

	if (bForceWidgetPosition)
	{
		float CapsuleHalfHeight = 88.f;
		if (UCapsuleComponent* Capsule = GetCapsuleComponent())
		{
			CapsuleHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
		}

		if (HealthWidgetComp)
		{
			HealthWidgetRelativeOffset = FVector(0.f, 0.f, CapsuleHalfHeight + HealthWidgetHeightOffset);
			HealthWidgetComp->SetRelativeLocation(HealthWidgetRelativeOffset);
		}

		if (TimerWidgetComp)
		{
			TimerWidgetRelativeOffset = FVector(0.f, 0.f, CapsuleHalfHeight + TimerWidgetHeightOffset);
			TimerWidgetComp->SetRelativeLocation(TimerWidgetRelativeOffset);
		}
	}
}

void APerformanceUnit::Destroyed()
{
	Super::Destroyed();
}

void APerformanceUnit::SetCharacterVisibility(bool desiredVisibility)
{
	UCapsuleComponent* Capsule = GetCapsuleComponent();

	if (Capsule)
		Capsule->SetVisibility(desiredVisibility, true);
	
	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (SkelMesh && bUseSkeletalMovement)
		{
			SkelMesh->SetVisibility(desiredVisibility, true);
			SkelMesh->bPauseAnims = !desiredVisibility;
		}

	if (ISMComponent && !bUseSkeletalMovement)
	{
		ISMComponent->SetVisibility(desiredVisibility, true);
		ISMComponent->SetHiddenInGame(!desiredVisibility);
	}
}


void APerformanceUnit::VisibilityTickFog()
{
	if (IsMyTeam)
	{
		SetCharacterVisibility(IsOnViewport);
	}else
	{
		if(IsOnViewport)SetCharacterVisibility(IsVisibleEnemy);
		else SetCharacterVisibility(false);
	}
}

void APerformanceUnit::CheckViewport()
{
	FVector ALocation = GetMassActorLocation();
	
	if (IsInViewport(ALocation, VisibilityOffset))
	{
		IsOnViewport = true;
	}
	else
	{
		IsOnViewport = false;
	}
}

void APerformanceUnit::CheckTeamVisibility()
{
	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		AControllerBase* ControllerBase = Cast<AControllerBase>(PlayerController);
		if(ControllerBase && (ControllerBase->SelectableTeamId == TeamId || ControllerBase->SelectableTeamId == 0))
		{
			IsMyTeam = true;
		}
		else
		{
			IsMyTeam = false;
		}
	}
}


bool APerformanceUnit::IsInViewport(FVector WorldPosition, float Offset)
{

	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (!PlayerController) return false;

	FVector2D ScreenPosition;
	// Check the boolean return value of the function here!
	if (UGameplayStatics::ProjectWorldToScreen(PlayerController, WorldPosition, ScreenPosition))
	{
		int32 ViewportSizeX, ViewportSizeY;
		PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);

		bool IsInViewportRange = ScreenPosition.X >= -Offset && ScreenPosition.X <= ViewportSizeX + Offset &&
			   ScreenPosition.Y >= -Offset && ScreenPosition.Y <= ViewportSizeY + Offset;
		
		return IsInViewportRange;
	}

	return false;
}

void APerformanceUnit::HandleSquadHealthBarVisibility()
{
	if (!HealthWidgetComp) return;

	UUnitBaseHealthBar* HealthBarWidget = Cast<UUnitBaseHealthBar>(HealthWidgetComp->GetUserWidgetObject());
	if (!HealthBarWidget)
	{
		HealthWidgetComp->InitWidget();
		HealthBarWidget = Cast<UUnitBaseHealthBar>(HealthWidgetComp->GetUserWidgetObject());
	}

	const bool bFogAllows = (!EnableFog || IsVisibleEnemy || IsMyTeam);

	if (SquadId <= 0) return;

	// Determine owner among alive squadmates with same team
	AUnitBase* ThisAsUnit = Cast<AUnitBase>(this);
	AUnitBase* Best = nullptr;
	int32 BestIndex = TNumericLimits<int32>::Max();
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AUnitBase> It(World); It; ++It)
		{
			AUnitBase* U = *It;
			if (!U || U->TeamId != TeamId || U->SquadId != SquadId) continue;
			if (U->GetUnitState() == UnitData::Dead) continue;
			int32 Index = U->UnitIndex;
			if (Index < 0) Index = INT_MAX - 1;
			if (!Best || Index < BestIndex)
			{
				Best = U;
				BestIndex = Index;
			}
		}
	}
	
	const bool bIsOwner = (ThisAsUnit && Best == ThisAsUnit);
	if (!bIsOwner)
	{
		if (UUserWidget* UW = HealthWidgetComp->GetUserWidgetObject())
		{
			UW->SetVisibility(ESlateVisibility::Collapsed);
		}
		HealthWidgetComp->SetVisibility(false);
		HealthBarUpdateTriggered = false;
		OpenHealthWidget = false;
		return;
	}

	{
		UUserWidget* ExistingUW = HealthWidgetComp->GetUserWidgetObject();
		const bool bNeedsSwitch = (!ExistingUW || !ExistingUW->IsA(USquadHealthBar::StaticClass()));
		if (bNeedsSwitch)
		{
			HealthWidgetComp->SetWidgetClass(USquadHealthBar::StaticClass());
			HealthWidgetComp->InitWidget();
			HealthBarWidget = Cast<UUnitBaseHealthBar>(HealthWidgetComp->GetUserWidgetObject());
		}
	}
	if (HealthBarWidget)
	{
		if (HealthBarWidget->GetOwnerActor() != ThisAsUnit)
		{
			HealthBarWidget->SetOwnerActor(ThisAsUnit);
		}
	}

	HealthWidgetComp->SetVisibility(true);

	USquadHealthBar* SquadHB = Cast<USquadHealthBar>(HealthBarWidget);
	if (SquadHB)
	{
		const bool bShouldShowSquad = IsOnViewport && bFogAllows && SquadHB->bAlwaysShowSquadHealthbar;

		if (bShouldShowSquad)
		{
			if (!HealthBarUpdateTriggered)
			{
				HealthBarUpdateTriggered = true;
			}
			HealthBarWidget->SetVisibility(ESlateVisibility::Visible);
			HealthBarWidget->UpdateWidget();
			if (!bUseSkeletalMovement)
			{
				FVector ALocation = GetMassActorLocation() + HealthWidgetRelativeOffset;
				HealthWidgetComp->SetWorldLocation(ALocation);
			}
		}
		else
		{
			if (HealthBarUpdateTriggered)
			{
				HealthBarUpdateTriggered = false;
			}
			HealthBarWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void APerformanceUnit::HandleStandardHealthBarVisibility()
{
	if (!HealthWidgetComp) return;

	UUnitBaseHealthBar* HealthBarWidget = Cast<UUnitBaseHealthBar>(HealthWidgetComp->GetUserWidgetObject());
	if (!HealthBarWidget)
	{
		HealthWidgetComp->InitWidget();
		HealthBarWidget = Cast<UUnitBaseHealthBar>(HealthWidgetComp->GetUserWidgetObject());
	}
	if (!HealthBarWidget)
	{
		return;
	}

	const bool bFogAllows = (!EnableFog || IsVisibleEnemy || IsMyTeam);

	if (IsOnViewport && (OpenHealthWidget || bShowLevelOnly) && !HealthBarUpdateTriggered && bFogAllows)
	{
		HealthBarWidget->SetVisibility(ESlateVisibility::Visible);
		HealthBarUpdateTriggered = true;
	}
	else if (HealthBarUpdateTriggered && !OpenHealthWidget && !bShowLevelOnly)
	{
		HealthBarWidget->SetVisibility(ESlateVisibility::Collapsed);
		HealthBarUpdateTriggered = false;
	}

	if (HealthBarUpdateTriggered)
	{
		HealthBarWidget->bShowLevelOnly = bShowLevelOnly;
		HealthBarWidget->UpdateWidget();
	}

	if (!bUseSkeletalMovement && OpenHealthWidget && IsOnViewport)
	{
		FVector ALocation = GetMassActorLocation() + HealthWidgetRelativeOffset;
		HealthWidgetComp->SetWorldLocation(ALocation);
	}
}

void APerformanceUnit::CheckHealthBarVisibility()
{
	if (!HealthWidgetComp)
		return;

	if (SquadId > 0)
	{
		HandleSquadHealthBarVisibility();
		return;
	}

	HandleStandardHealthBarVisibility();
}


void APerformanceUnit::SpawnDamageIndicator_Implementation(const float Damage, FLinearColor HighColor, FLinearColor LowColor, float ColorOffset)
{
	if (IsOnViewport && (!EnableFog || IsVisibleEnemy || IsMyTeam))
	{
		if(Damage > 0 && Attributes->IndicatorBaseClass)
		{
			
			FTransform Transform;
			Transform.SetLocation(GetActorLocation());
			Transform.SetRotation(FQuat(FRotator::ZeroRotator)); // FRotator::ZeroRotator

			const auto MyIndicator = Cast<AIndicatorActor>
								(UGameplayStatics::BeginDeferredActorSpawnFromClass
								(this, Attributes->IndicatorBaseClass, Transform,  ESpawnActorCollisionHandlingMethod::AlwaysSpawn));
			
			
			if (MyIndicator != nullptr)
			{
				UGameplayStatics::FinishSpawningActor(MyIndicator, Transform);
				MyIndicator->SpawnDamageIndicator(Damage, HighColor, LowColor, ColorOffset);
			}
		}
	}
}

void APerformanceUnit::ShowWorkAreaIfNoFog_Implementation(AWorkArea* WorkArea)
{
	if (WorkArea)
	{
		// Example condition checks: adapt these to your game logic
		if (!EnableFog || IsVisibleEnemy || IsMyTeam)
		{
			if (WorkArea->Mesh)
			{
				WorkArea->Mesh->SetHiddenInGame(false);

				if (WorkArea->IsNoBuildZone)
				{
					// Set a timer to call a lambda function after 5 seconds
					GetWorld()->GetTimerManager().SetTimer(WorkArea->HideWorkAreaTimerHandle, [WorkArea]()
					{
						if (WorkArea && WorkArea->Mesh)
						{
							WorkArea->Mesh->SetHiddenInGame(true);
						}
					}, 5.0f, false);
				}
			}
		}
	}
}

void APerformanceUnit::ShowAbilityIndicator_Implementation(AAbilityIndicator* AbilityIndicator)
{
	if (AbilityIndicator)
	{
		// Example condition checks: adapt these to your game logic
		if (IsMyTeam)
		{
			if (AbilityIndicator->IndicatorMesh)
			{
				AbilityIndicator->IndicatorMesh->SetHiddenInGame(false);
				AbilityIndicatorVisibility = true;
			}
		}
	}
}

void APerformanceUnit::HideAbilityIndicator_Implementation(AAbilityIndicator* AbilityIndicator)
{
	if (AbilityIndicator)
	{
		// Example condition checks: adapt these to your game logic
		if (IsMyTeam)
		{
			if (AbilityIndicator->IndicatorMesh)
			{
				AbilityIndicator->IndicatorMesh->SetHiddenInGame(true);
				AbilityIndicatorVisibility = false;
			}
		}
	}
}

void APerformanceUnit::FireEffects_Implementation(UNiagaraSystem* ImpactVFX, USoundBase* ImpactSound, FVector ScaleVFX, float ScaleSound, float EffectDelay, float SoundDelay, int32 ID)
{
    if (IsOnViewport && (!EnableFog || IsVisibleEnemy || IsMyTeam))
    {
        UWorld* World = GetWorld();
        if (!World)
        {
            return;
        }

        // Create a weak pointer to `this` to be safely used in the lambdas.
        TWeakObjectPtr<APerformanceUnit> WeakThis = this;

        // Spawn the Niagara visual effect
        if (ImpactVFX)
        {
            if (EffectDelay > 0.f)
            {
                FTimerHandle VisualEffectTimerHandle;
                World->GetTimerManager().SetTimer(
                    VisualEffectTimerHandle,
                    FTimerDelegate::CreateLambda([WeakThis, ImpactVFX, ScaleVFX, ID]()
                    {
                        if (APerformanceUnit* Unit = WeakThis.Get())
                        {
                            FVector Location = Unit->GetMassActorLocation();
                            FRotator Rotation = Unit->GetActorRotation();
                            if (UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(Unit->GetWorld(), ImpactVFX, Location, Rotation, ScaleVFX))
                            {
                                Unit->ActiveNiagara.Add(FActiveNiagaraEffect(NiagaraComp, ID));
                            }
                        }
                    }),
                    EffectDelay,
                    false
                );
                PendingEffectTimers.Add(VisualEffectTimerHandle);
            }
            else
            {
                if (UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactVFX, GetMassActorLocation(), GetActorRotation(), ScaleVFX))
                {
                    ActiveNiagara.Add(FActiveNiagaraEffect(NiagaraComp, ID));
                }
            }
        }

        // Play the impact sound
        if (ImpactSound)
        {
            if (SoundDelay > 0.f)
            {
                FTimerHandle SoundTimerHandle;
                World->GetTimerManager().SetTimer(
                    SoundTimerHandle,
                    FTimerDelegate::CreateLambda([WeakThis, ImpactSound, ScaleSound]()
                    {
                        if (APerformanceUnit* Unit = WeakThis.Get())
                        {
                            FVector Location = Unit->GetMassActorLocation();
                            FRotator Rotation = Unit->GetActorRotation();

                            float Multiplier = ScaleSound;
                            if (UGameInstance* GI = Unit->GetGameInstance())
                            {
                                if (APlayerController* PC = GI->GetFirstLocalPlayerController())
                                {
                                    if (AControllerBase* ControllerBase = Cast<AControllerBase>(PC))
                                    {
                                        Multiplier *= ControllerBase->GetSoundMultiplier();
                                    }
                                }
                            }

                            if (UAudioComponent* AudioComp = UGameplayStatics::SpawnSoundAtLocation(Unit->GetWorld(), ImpactSound, Location, Rotation, Multiplier))
                            {
                                Unit->ActiveAudio.Add(AudioComp);
                            }
                        }
                    }),
                    SoundDelay,
                    false
                );
                PendingEffectTimers.Add(SoundTimerHandle);
            }
            else
            {
                float Multiplier = ScaleSound;
                if (UGameInstance* GI = GetGameInstance())
                {
                    if (APlayerController* PC = GI->GetFirstLocalPlayerController())
                    {
                        if (AControllerBase* ControllerBase = Cast<AControllerBase>(PC))
                        {
                            Multiplier *= ControllerBase->GetSoundMultiplier();
                        }
                    }
                }

                if (UAudioComponent* AudioComp = UGameplayStatics::SpawnSoundAtLocation(GetWorld(), ImpactSound, GetMassActorLocation(), GetActorRotation(), Multiplier))
                {
                    ActiveAudio.Add(AudioComp);
                }
            }
        }
    }
}

void APerformanceUnit::FireEffectsAtLocation_Implementation(UNiagaraSystem* ImpactVFX, USoundBase* ImpactSound, FVector ScaleVFX, float ScaleSound, const FVector Location, float KillDelay, FRotator Rotation, int32 ID)
{
    if (IsOnViewport && (!EnableFog || IsVisibleEnemy || IsMyTeam))
    {
        UWorld* World = GetWorld();
        if (!World)
        {
            return;
        }

        // Spawn the Niagara visual effect
        UNiagaraComponent* NiagaraComp = nullptr;
        if (ImpactVFX)
        {
            NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, ImpactVFX, Location, Rotation, ScaleVFX);
            if (NiagaraComp)
            {
                ActiveNiagara.Add(FActiveNiagaraEffect(NiagaraComp, ID));
            }
        }

        // Spawn the sound effect and get its audio component
        UAudioComponent* AudioComp = nullptr;
        if (ImpactSound)
        {
            float Multiplier = ScaleSound;
            if (UGameInstance* GI = World->GetGameInstance())
            {
                if (APlayerController* PC = GI->GetFirstLocalPlayerController())
                {
                    if (AControllerBase* ControllerBase = Cast<AControllerBase>(PC))
                    {
                        Multiplier *= ControllerBase->GetSoundMultiplier();
                    }
                }
            }
            AudioComp = UGameplayStatics::SpawnSoundAtLocation(World, ImpactSound, Location, Rotation, Multiplier);
            if (AudioComp)
            {
                ActiveAudio.Add(AudioComp);
            }
        }

        // If either component is valid and a kill delay is provided, set up a timer to kill them.
        if ((NiagaraComp || AudioComp) && KillDelay > 0.0f)
        {
            // Create weak pointers to safely reference the components in the lambda
            TWeakObjectPtr<UNiagaraComponent> WeakNiagaraComp = NiagaraComp;
            TWeakObjectPtr<UAudioComponent> WeakAudioComp = AudioComp;

            FTimerHandle TimerHandle;
            World->GetTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateLambda([WeakNiagaraComp, WeakAudioComp]()
            {
                // Safely check if the Niagara component still exists and then clean it up
                if (UNiagaraComponent* NC = WeakNiagaraComp.Get())
                {
                    NC->Deactivate();
                    NC->DestroyComponent();
                }
                
                // Safely check if the Audio component still exists and then clean it up
                if (UAudioComponent* AC = WeakAudioComp.Get())
                {
                    AC->Stop();
                    AC->DestroyComponent();
                }
            }), KillDelay, false);

            PendingEffectTimers.Add(TimerHandle);
        }
    }
}

void APerformanceUnit::CheckTimerVisibility()
{
	if (UUnitTimerWidget* TimerWidget = Cast<UUnitTimerWidget>(TimerWidgetComp->GetUserWidgetObject())) // Assuming you have a UUnitBaseTimer class for the timer widget
	{

		if (IsOnViewport && IsMyTeam)
		{
			TimerWidget->SetVisibility(ESlateVisibility::Visible);

			if(!bUseSkeletalMovement)
			{
				FVector ALocation = GetMassActorLocation()+TimerWidgetRelativeOffset;
				TimerWidgetComp->SetWorldLocation(ALocation);
			}
		}
		else
		{
			TimerWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}


void APerformanceUnit::SetEnemyVisibility(APerformanceUnit* DetectingActor, bool bVisible)
{
	// Do nothing for own team or if state already matches
	if (IsMyTeam) return;
	if (IsVisibleEnemy == bVisible) return;

	// DetectingActor may be null/not replicated yet on some clients; guard before use
	if (DetectingActor == nullptr || !DetectingActor->IsValidLowLevelFast())
	{
		return;
	}
	
	UWorld* World = GetWorld();
	if (!World) return;  // Safety check

	// Ensure we have a controller reference and cast safely
	ACustomControllerBase* MyController = nullptr;
	if (OwningPlayerController)
	{
		MyController = Cast<ACustomControllerBase>(OwningPlayerController);
	}
	else
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			OwningPlayerController = PlayerController;
			MyController = Cast<ACustomControllerBase>(OwningPlayerController);
		}
	}

	if (MyController && MyController->IsValidLowLevelFast())
	{
		if (MyController->SelectableTeamId == DetectingActor->TeamId)
		{
			IsVisibleEnemy = bVisible;
		}
	}
}

void APerformanceUnit::MulticastSetEnemyVisibility_Implementation(APerformanceUnit* DetectingActor, bool bVisible)
{
	// Do nothing for own team or if state already matches
	if (IsMyTeam) return;
	if (IsVisibleEnemy == bVisible) return;

	// DetectingActor may be null/not replicated yet on some clients; guard before use
	if (DetectingActor == nullptr || !DetectingActor->IsValidLowLevelFast())
	{
		return;
	}
	
	UWorld* World = GetWorld();
	if (!World) return;  // Safety check

	// Ensure we have a controller reference and cast safely
	ACustomControllerBase* MyController = nullptr;
	if (OwningPlayerController)
	{
		MyController = Cast<ACustomControllerBase>(OwningPlayerController);
	}
	else
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			OwningPlayerController = PlayerController;
			MyController = Cast<ACustomControllerBase>(OwningPlayerController);
		}
	}

	if (MyController && MyController->IsValidLowLevelFast())
	{
		if (MyController->SelectableTeamId == DetectingActor->TeamId)
		{
			IsVisibleEnemy = bVisible;
		}
	}
}


bool APerformanceUnit::ComputeLocalVisibility() const
{
	return IsOnViewport
		&& ( !EnableFog
		   || IsVisibleEnemy
		   || IsMyTeam );
}

void APerformanceUnit::StopNiagaraComponent(UNiagaraComponent* NC, float FadeTime)
{
    if (!NC) return;
    UWorld* World = GetWorld();
    if (FadeTime > 0.f && World)
    {
        NC->Deactivate();
        FVector InitialScale = NC->GetRelativeScale3D();
        TWeakObjectPtr<UNiagaraComponent> WeakNC = NC;
        const float Delays[] = { 0.25f, 0.50f, 0.75f, 0.98f };
        const float Multipliers[] = { 0.75f, 0.50f, 0.25f, 0.0f };

        for (int32 Step = 0; Step < 4; ++Step)
        {
            FTimerHandle H;
            World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([WeakNC, InitialScale, M = Multipliers[Step]]()
            {
                if (UNiagaraComponent* C = WeakNC.Get())
                {
                    C->SetRelativeScale3D(InitialScale * M);
                }
            }), FadeTime * Delays[Step], false);
            PendingEffectTimers.Add(H);
        }

        FTimerHandle DestroyHandle;
        World->GetTimerManager().SetTimer(DestroyHandle, FTimerDelegate::CreateLambda([WeakNC]()
        {
            if (UNiagaraComponent* C = WeakNC.Get())
            {
                C->DeactivateImmediate();
                C->DestroyComponent();
            }
        }), FadeTime + 0.01f, false);
        PendingEffectTimers.Add(DestroyHandle);
    }
    else
    {
        NC->DeactivateImmediate();
        NC->DestroyComponent();
    }
}

void APerformanceUnit::StopAudioComponent(UAudioComponent* AC, bool bFade, float FadeTime)
{
    if (!AC) return;
    if (bFade && FadeTime > 0.f)
    {
        AC->FadeOut(FadeTime, 0.f);
        if (UWorld* World = GetWorld())
        {
            TWeakObjectPtr<UAudioComponent> WeakAC = AC;
            FTimerHandle DestroyHandle;
            World->GetTimerManager().SetTimer(DestroyHandle, FTimerDelegate::CreateLambda([WeakAC]()
            {
                if (UAudioComponent* SafeAC = WeakAC.Get())
                {
                    SafeAC->Stop();
                    SafeAC->DestroyComponent();
                }
            }), FadeTime + 0.01f, false);
            PendingEffectTimers.Add(DestroyHandle);
        }
    }
    else
    {
        AC->Stop();
        AC->DestroyComponent();
    }
}

void APerformanceUnit::StopNiagaraByID_Implementation(int32 ID, float FadeTime)
{
    for (int32 i = ActiveNiagara.Num() - 1; i >= 0; --i)
    {
        if (ActiveNiagara[i].Id == ID)
        {
            if (UNiagaraComponent* NC = ActiveNiagara[i].Component.Get())
            {
                StopNiagaraComponent(NC, FadeTime);
            }
            ActiveNiagara.RemoveAt(i);
        }
    }
}

void APerformanceUnit::StopAllEffects_Implementation(bool bFadeAudio, float FadeTime)
{
    UWorld* World = GetWorld();
    if (World)
    {
        for (FTimerHandle& H : PendingEffectTimers)
        {
            World->GetTimerManager().ClearTimer(H);
        }
    }
    PendingEffectTimers.Empty();

    // Stop and destroy VFX
    for (int32 i = ActiveNiagara.Num() - 1; i >= 0; --i)
    {
        if (UNiagaraComponent* NC = ActiveNiagara[i].Component.Get())
        {
            StopNiagaraComponent(NC, FadeTime);
        }
    }
    ActiveNiagara.Empty();

    // Stop and destroy SFX
    for (int32 i = ActiveAudio.Num() - 1; i >= 0; --i)
    {
        if (UAudioComponent* AC = ActiveAudio[i].Get())
        {
            StopAudioComponent(AC, bFadeAudio, FadeTime);
        }
    }
    ActiveAudio.Empty();
}
