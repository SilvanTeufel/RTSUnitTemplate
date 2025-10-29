// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/PerformanceUnit.h"

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

// Debug category for squad healthbar visibility
DEFINE_LOG_CATEGORY_STATIC(LogSquadHB, Log, All);

APerformanceUnit::APerformanceUnit(const FObjectInitializer& ObjectInitializer):Super(ObjectInitializer)
{
	//bReplicates = true;
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

	UpdateClientVisibility();
	
}

void APerformanceUnit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(APerformanceUnit, OpenHealthWidget);
	DOREPLIFETIME(APerformanceUnit, HealthWidgetComp);
	DOREPLIFETIME(APerformanceUnit, TimerWidgetComp);
	DOREPLIFETIME(APerformanceUnit, HealthWidgetRelativeOffset);
	DOREPLIFETIME(APerformanceUnit, TimerWidgetRelativeOffset);

	DOREPLIFETIME(APerformanceUnit, MeleeImpactVFX);
	DOREPLIFETIME(APerformanceUnit, MeleeImpactSound);
	DOREPLIFETIME(APerformanceUnit, ScaleImpactSound);
	DOREPLIFETIME(APerformanceUnit, ScaleImpactVFX);
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
	DOREPLIFETIME(APerformanceUnit, bClientIsVisible);
	
}

void APerformanceUnit::BeginPlay()
{
	Super::BeginPlay();

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

	if (IsOnViewport && OpenHealthWidget && !HealthBarUpdateTriggered && bFogAllows)
	{
		HealthBarWidget->SetVisibility(ESlateVisibility::Visible);
		HealthBarUpdateTriggered = true;
	}
	else if (HealthBarUpdateTriggered && !OpenHealthWidget)
	{
		HealthBarWidget->SetVisibility(ESlateVisibility::Collapsed);
		HealthBarUpdateTriggered = false;
	}

	if (HealthBarUpdateTriggered)
	{
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

void APerformanceUnit::FireEffects_Implementation(UNiagaraSystem* ImpactVFX, USoundBase* ImpactSound, FVector ScaleVFX, float ScaleSound, float EffectDelay, float SoundDelay)
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
                    [WeakThis, ImpactVFX, ScaleVFX]() // Capture the weak pointer
                    {
                        // First, check if the Actor is still valid
                        if (WeakThis.IsValid())
                        {
                            // It's safe to use the actor's functions now
                            FVector Location = WeakThis->GetMassActorLocation();
                            FRotator Rotation = WeakThis->GetActorRotation();
                            UNiagaraFunctionLibrary::SpawnSystemAtLocation(WeakThis->GetWorld(), ImpactVFX, Location, Rotation, ScaleVFX);
                        }
                    },
                    EffectDelay,
                    false
                );
            }
            else
            {
                UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactVFX, GetMassActorLocation(), GetActorRotation(), ScaleVFX);
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
                    [WeakThis, ImpactSound, ScaleSound]() // Capture the weak pointer
                    {
                        // First, check if the Actor is still valid
                        if (WeakThis.IsValid())
                        {
                            // It's safe to use the actor's functions now
                            FVector Location = WeakThis->GetMassActorLocation();
                            UGameplayStatics::PlaySoundAtLocation(WeakThis->GetWorld(), ImpactSound, Location, ScaleSound);
                        }
                    },
                    SoundDelay,
                    false
                );
            }
            else
            {
                UGameplayStatics::PlaySoundAtLocation(GetWorld(), ImpactSound, GetMassActorLocation(), ScaleSound);
            }
        }
    }
}

void APerformanceUnit::FireEffectsAtLocation_Implementation(UNiagaraSystem* ImpactVFX, USoundBase* ImpactSound, FVector ScaleVFX, float ScaleSound, const FVector Location, float KillDelay, FRotator Rotation)
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
        }

        // Spawn the sound effect and get its audio component
        UAudioComponent* AudioComp = nullptr;
        if (ImpactSound)
        {
            AudioComp = UGameplayStatics::SpawnSoundAtLocation(World, ImpactSound, Location, Rotation, ScaleSound);
        }

        // If either component is valid and a kill delay is provided, set up a timer to kill them.
        if ((NiagaraComp || AudioComp) && KillDelay > 0.0f)
        {
            // Create weak pointers to safely reference the components in the lambda
            TWeakObjectPtr<UNiagaraComponent> WeakNiagaraComp = NiagaraComp;
            TWeakObjectPtr<UAudioComponent> WeakAudioComp = AudioComp;

            FTimerHandle TimerHandle;
            World->GetTimerManager().SetTimer(TimerHandle, [WeakNiagaraComp, WeakAudioComp]()
            {
                // Safely check if the Niagara component still exists and then clean it up
                if (WeakNiagaraComp.IsValid())
                {
                    WeakNiagaraComp->Deactivate();
                    WeakNiagaraComp->DestroyComponent();
                }
                
                // Safely check if the Audio component still exists and then clean it up
                if (WeakAudioComp.IsValid())
                {
                    WeakAudioComp->Stop();
                    WeakAudioComp->DestroyComponent();
                }
            }, KillDelay, false);
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


void APerformanceUnit::SetClientVisibility(bool bVisible)
{
	bClientIsVisible = bVisible;
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

void APerformanceUnit::UpdateClientVisibility()
{
	if (!HasAuthority()) // only on client
	{
		const bool bNew = ComputeLocalVisibility();
		if (bNew != bClientIsVisible)
		{
			if (ACustomControllerBase* PC = Cast<ACustomControllerBase>(GetWorld()->GetFirstPlayerController()))
			{
				PC->Server_ReportUnitVisibility(this, bNew);
			}
		}
	}
}