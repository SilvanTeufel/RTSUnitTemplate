// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/PerformanceUnit.h"

#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Widgets/UnitBaseHealthBar.h"
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

#include "Engine/World.h"              // For UWorld, GetWorld()
#include "GameFramework/Actor.h"         // For FActorSpawnParameters, SpawnActo

APerformanceUnit::APerformanceUnit(const FObjectInitializer& ObjectInitializer):Super(ObjectInitializer)
{
	bReplicates = true;
/*
	// Create the binding component and attach it to the actor's root.
	MassBindingComponent = CreateDefaultSubobject<UMassActorBindingComponent>(TEXT("MassBindingComponent"));
	if(MassBindingComponent)
	{
		MassBindingComponent->SetupAttachment(RootComponent);
	}*/
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
	DOREPLIFETIME(APerformanceUnit, HealthWidgetCompLocation);

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


	//SetOwningPlayerController();
	
}

void APerformanceUnit::Destroyed()
{
	// MassActorBindingComponent->CleanupMassEntity();
	Super::Destroyed();
	//DestroyFogManager();
}
/*
void APerformanceUnit::DestroyFogManager()
{
	
	if (SpawnedFogManager)
	{
		// Detach the FogManager from this unit
		SpawnedFogManager->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

		// Destroy the FogManager
		SpawnedFogManager->Destroy();
		SpawnedFogManager = nullptr;
	}
	
}
*/
/*
void APerformanceUnit::SetOwningPlayerController_Implementation()
{
	UWorld* World = GetWorld();
	if (!World) return;  // Safety check
	
	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (PlayerController)
	{
		ACustomControllerBase* ControllerBase = Cast<ACustomControllerBase>(PlayerController);
		if (ControllerBase && (ControllerBase->SelectableTeamId == TeamId || ControllerBase->SelectableTeamId == 0) && ControllerBase->SelectableTeamId != -1)
		{
			OwningPlayerController = ControllerBase;
			//ControllerBase->Multi_SetFogManagerUnit(this);
		}
	}
}
*/
/*
void APerformanceUnit::SpawnFogOfWarManager(APlayerController* PC)
{


	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("World is not valid."));
		return;
	}

	if (!EnableFog || !FogOfWarManagerClass) return;
	
	AControllerBase* ControllerBase = Cast<AControllerBase>(PC);
	
	if(SpawnedFogManager) return;
	
	if (FogOfWarManagerClass &&
		ControllerBase  &&
		(ControllerBase->SelectableTeamId == TeamId ||
			ControllerBase->SelectableTeamId == 0))
	{
		
					FVector SpawnLocation = GetActorLocation();
					FRotator SpawnRotation = FRotator::ZeroRotator;
					FTransform SpawnTransform(SpawnRotation, SpawnLocation);
					// Spawn the FogOfWarManager at the new location
					SpawnedFogManager = World->SpawnActorDeferred<AFogOfWarManager>(FogOfWarManagerClass, SpawnTransform);

					if (SpawnedFogManager)
					{
						SpawnedFogManager->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("rootSocket"));
					}

					if (SpawnedFogManager && SpawnedFogManager->Mesh)
					{
						// Scale the Mesh component of the spawned FogOfWarManager
						SpawnedFogManager->PlayerController = ControllerBase;
						SpawnedFogManager->TeamId = TeamId;
						SpawnedFogManager->PlayerTeamId =  ControllerBase->SelectableTeamId;//ControllerBase->SelectableTeamId;
						SpawnedFogManager->OwningUnit = this;
						SpawnedFogManager->Mesh->SetWorldScale3D(FVector(SightRadius, SightRadius, 1.f )*FogManagerMultiplier);
						SpawnedFogManager->Mesh->SetRelativeLocation(FogManagerPositionOffset);
						// Bind overlap events using the appropriate class and instance
						SpawnedFogManager->Mesh->OnComponentBeginOverlap.AddDynamic(SpawnedFogManager, &AFogOfWarManager::OnMeshBeginOverlap);
						SpawnedFogManager->Mesh->OnComponentEndOverlap.AddDynamic(SpawnedFogManager, &AFogOfWarManager::OnMeshEndOverlap);
	
				
						// Finish spawning the actor. This will trigger its BeginPlay().
						UGameplayStatics::FinishSpawningActor(SpawnedFogManager, SpawnTransform);
						UE_LOG(LogTemp, Warning, TEXT("SpawnedFogManager! %d"), SpawnedFogManager->PlayerTeamId);
						if (SpawnedFogManager)
						{
							SpawnedFogManager->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("rootSocket"));
						}

						
						for (TActorIterator<AFogOfWarCentralManager> It(GetWorld()); It; ++It)
						{
							It->FogManagers.Add(SpawnedFogManager);
							break;
						}
						
					}
	}

}
*/

/*
void APerformanceUnit::SpawnFogOfWarManagerTeamIndependent(APlayerController* PC)
{
	UWorld* World = GetWorld();
	if (!World)
	{

		return;
	}

	if (!EnableFog || !FogOfWarManagerClass) return;
	
	AControllerBase* ControllerBase = Cast<AControllerBase>(PC);
	
	if(SpawnedFogManager) return;
	
	if (FogOfWarManagerClass && ControllerBase)
	{
					FVector SpawnLocation = GetActorLocation();
					FRotator SpawnRotation = FRotator::ZeroRotator;
					FTransform SpawnTransform(SpawnRotation, SpawnLocation);
					// Spawn the FogOfWarManager at the new location
					SpawnedFogManager = World->SpawnActorDeferred<AFogOfWarManager>(FogOfWarManagerClass, SpawnTransform);

					if (SpawnedFogManager)
					{
						SpawnedFogManager->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("rootSocket"));
					}

					if (SpawnedFogManager && SpawnedFogManager->Mesh)
					{
						// Scale the Mesh component of the spawned FogOfWarManager
						SpawnedFogManager->ManagerSetsVisibility = false;
						SpawnedFogManager->PlayerController = ControllerBase;
						SpawnedFogManager->TeamId = TeamId;
						SpawnedFogManager->PlayerTeamId = TeamId;//ControllerBase->SelectableTeamId;
						SpawnedFogManager->OwningUnit = this;
						SpawnedFogManager->Mesh->SetWorldScale3D(FVector(SightRadius, SightRadius, 1.f )*FogManagerMultiplier);
						SpawnedFogManager->Mesh->SetRelativeLocation(FogManagerPositionOffset);
						// Bind overlap events using the appropriate class and instance
						SpawnedFogManager->Mesh->OnComponentBeginOverlap.AddDynamic(SpawnedFogManager, &AFogOfWarManager::OnMeshBeginOverlap);
						SpawnedFogManager->Mesh->OnComponentEndOverlap.AddDynamic(SpawnedFogManager, &AFogOfWarManager::OnMeshEndOverlap);
	
						SpawnedFogManager->Mesh->SetAffectDistanceFieldLighting(false);
						// Finish spawning the actor. This will trigger its BeginPlay().
						UGameplayStatics::FinishSpawningActor(SpawnedFogManager, SpawnTransform);

						if (SpawnedFogManager)
						{
							SpawnedFogManager->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("rootSocket"));
						}
					}
	}
	
}
*/

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
	

	FVector ALocation = GetActorLocation();

	if (!bUseSkeletalMovement)
	{
		FTransform Xform;
		ISMComponent->GetInstanceTransform(InstanceIndex, Xform, true);
		ALocation = Xform.GetLocation();
	}
		
	
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
	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		FVector2D ScreenPosition;
		UGameplayStatics::ProjectWorldToScreen(PlayerController, WorldPosition, ScreenPosition);

		int32 ViewportSizeX, ViewportSizeY;
		PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);

		return ScreenPosition.X >= -Offset && ScreenPosition.X <= ViewportSizeX + Offset && ScreenPosition.Y >= -Offset && ScreenPosition.Y <= ViewportSizeY + Offset;
	}

	return false;
}

void APerformanceUnit::CheckHealthBarVisibility()
{
	if(HealthWidgetComp)
	if (UUnitBaseHealthBar* HealthBarWidget = Cast<UUnitBaseHealthBar>(HealthWidgetComp->GetUserWidgetObject()))
	{
		if (IsOnViewport && OpenHealthWidget && !HealthBarUpdateTriggered && (!EnableFog || IsVisibleEnemy || IsMyTeam))
		{
			HealthBarWidget->SetVisibility(ESlateVisibility::Visible);
			HealthBarUpdateTriggered = true;
		}
		else if(HealthBarUpdateTriggered && !OpenHealthWidget)
		{
			HealthBarWidget->SetVisibility(ESlateVisibility::Collapsed);
			HealthBarUpdateTriggered = false;
		}
		
		if(HealthBarUpdateTriggered)	
			HealthBarWidget->UpdateWidget();


		if(!bUseSkeletalMovement && OpenHealthWidget && IsOnViewport)
		{
			FTransform Xform;
			ISMComponent->GetInstanceTransform(InstanceIndex, Xform, true);
			FVector ALocation = Xform.GetLocation();

			HealthWidgetComp->SetWorldLocation(ALocation);
		}
	}
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
				//WorkArea->Mesh->SetVisibility(IsVisible, /* PropagateToChildren = */ true);
				//WorkArea->SceneRoot->SetVisibility(true, true);
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

	//UE_LOG(LogTemp, Warning, TEXT("IsVisible: %d"), IsVisible);
	if (IsOnViewport && (!EnableFog || IsVisibleEnemy || IsMyTeam))
	{
		FVector LocationToFireEffects = GetActorLocation();
		if (!bUseSkeletalMovement && ISMComponent)
		{
			FTransform InstanceTransform;
			// Get the world-space transform of this specific unit's instance
			ISMComponent->GetInstanceTransform(InstanceIndex, /*out*/ InstanceTransform, /*worldSpace=*/ true);
			LocationToFireEffects = InstanceTransform.GetLocation();
		}
		
		UWorld* World = GetWorld();

		if (!World)
		{
			return;
		}
		
		if (World)
		{
			// Spawn the Niagara visual effect at the projectile's location and rotation
			if (ImpactVFX)
			{

				if (EffectDelay > 0.f)
				{
					// Set a timer to spawn the visual effect again after EffectDelay seconds
					FTimerHandle VisualEffectTimerHandle;
					World->GetTimerManager().SetTimer(
							VisualEffectTimerHandle,
							[this, ImpactVFX, ScaleVFX, LocationToFireEffects]()
							{
								if (GetWorld())
								{
									UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactVFX, LocationToFireEffects, GetActorRotation(), ScaleVFX);
								}
							},
							EffectDelay,
							false
					);
				}else
				{
					UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactVFX, LocationToFireEffects, GetActorRotation(), ScaleVFX);
				}

			}

			// Play the impact sound at the projectile's location
			if (ImpactSound)
			{
				if (SoundDelay > 0.f)
				{
					FTimerHandle SoundTimerHandle;
					World->GetTimerManager().SetTimer(
						SoundTimerHandle,
						[this, ImpactSound, ScaleSound, LocationToFireEffects]()
						{
							if (GetWorld())
							{
								UGameplayStatics::PlaySoundAtLocation(GetWorld(), ImpactSound, LocationToFireEffects, ScaleSound);
							}
						},
						SoundDelay,
						false
					);
				}else
				{
					UGameplayStatics::PlaySoundAtLocation(GetWorld(), ImpactSound, LocationToFireEffects, ScaleSound);
				}
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
			FTimerHandle TimerHandle;
			World->GetTimerManager().SetTimer(TimerHandle, [NiagaraComp, AudioComp]()
			{
				// Stop and clean up the Niagara effect
				if (NiagaraComp)
				{
					NiagaraComp->Deactivate();
					NiagaraComp->DestroyComponent();
				}
				// Stop and clean up the audio effect
				if (AudioComp)
				{
					AudioComp->Stop();
					AudioComp->DestroyComponent();
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
		}
		else
		{
			TimerWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

bool APerformanceUnit::IsNetVisible() const
{
	// Server’s own visibility logic:
	const bool ServerVis = ComputeLocalVisibility();

	// OR in the last‐reported client result:
	return ServerVis || bClientIsVisible;
}
void APerformanceUnit::SetClientVisibility(bool bVisible)
{
	bClientIsVisible = bVisible;
}

void APerformanceUnit::MulticastSetEnemyVisibility_Implementation(APerformanceUnit* DetectingActor, bool bVisible)
{
	if (IsMyTeam) return;
	if (IsVisibleEnemy == bVisible) return;

	//if (!HasAuthority()) UE_LOG(LogTemp, Log, TEXT("FIRST CHECK OK!!!!"));
	
	UWorld* World = GetWorld();
	if (!World) return;  // Safety check

	if (!OwningPlayerController)
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController) OwningPlayerController = PlayerController;
	}

	if (OwningPlayerController)
	if (ACustomControllerBase* MyController = Cast<ACustomControllerBase>(OwningPlayerController))
	{
		//if (!HasAuthority())
			//UE_LOG(LogTemp, Log, TEXT("FOUDN CONTROLLER %d // %d DetectingACTOR"), MyController->SelectableTeamId, DetectingActor->TeamId);
		
		if (MyController->IsValidLowLevel() && MyController->SelectableTeamId == DetectingActor->TeamId)
		{
			//if (!HasAuthority()) UE_LOG(LogTemp, Log, TEXT("FOUND MYControler!!!!! %d"), MyController->SelectableTeamId);
			//if (TeamId == DetectingActor->TeamId) return;

			//if (!HasAuthority())
				//UE_LOG(LogTemp, Log, TEXT("SET!!!!!"));
	
	
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
	// UE_LOG(LogTemp, Log, TEXT("!!!!Trying UpdateClientVisibility!!!"));
	// Only run this on the *owning* client, never on the server:
	
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