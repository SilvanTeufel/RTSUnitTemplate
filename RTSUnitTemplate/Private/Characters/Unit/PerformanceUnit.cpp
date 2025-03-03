// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/PerformanceUnit.h"

#include "Net/UnrealNetwork.h"
#include "Widgets/UnitBaseHealthBar.h"
#include "Widgets/UnitTimerWidget.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Controller/AIController/UnitControllerBase.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Engine/SkeletalMesh.h" // For USkeletalMesh
#include "Engine/SkeletalMeshLODSettings.h" // To access LOD settings
#include "Engine/SkinnedAssetCommon.h"

APerformanceUnit::APerformanceUnit(const FObjectInitializer& ObjectInitializer):Super(ObjectInitializer)
{
	bReplicates = true;
}

void APerformanceUnit::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	CheckViewport();
	CheckTeamVisibility();
	
	if(EnableFog)VisibilityTickFog();
	else SetCharacterVisibility(IsOnViewport);
	
	CheckHealthBarVisibility();
	CheckTimerVisibility();
	
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

	DOREPLIFETIME(APerformanceUnit, MeeleImpactVFXDelay);
	DOREPLIFETIME(APerformanceUnit, MeleeImpactSoundDelay);
	
}

void APerformanceUnit::BeginPlay()
{
	Super::BeginPlay();


	SetOwningPlayerControllerAndSpawnFogManager();
}

void APerformanceUnit::Destroyed()
{
	Super::Destroyed();

	DestroyFogManager();
}

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

void APerformanceUnit::SetOwningPlayerControllerAndSpawnFogManager()
{
	
	UWorld* World = GetWorld();
	if (!World) return;  // Safety check
	
	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (PlayerController)
	{
		AExtendedControllerBase* ControllerBase = Cast<AExtendedControllerBase>(PlayerController);
		if (ControllerBase && (ControllerBase->SelectableTeamId == TeamId || ControllerBase->SelectableTeamId == 0) && ControllerBase->SelectableTeamId != -1)
		{
			OwningPlayerController = ControllerBase;
			ControllerBase->Multi_SetFogManagerUnit(this);
		}
	}
}

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
						SpawnedFogManager->TeamId = TeamId;
						SpawnedFogManager->PlayerTeamId =  ControllerBase->SelectableTeamId;//ControllerBase->SelectableTeamId;
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
						
					}
	}
	
}

void APerformanceUnit::SetCharacterVisibility(bool desiredVisibility)
{
	UCapsuleComponent* Capsule = GetCapsuleComponent();

	if (Capsule)
		Capsule->SetVisibility(desiredVisibility, true);
	
	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (SkelMesh)
		{
			SkelMesh->SetVisibility(desiredVisibility, true);
			SkelMesh->bPauseAnims = !desiredVisibility;
		}

}


void APerformanceUnit::VisibilityTickFog()
{
	
	if (IsMyTeam)
	{
		SetCharacterVisibility(IsOnViewport);
	}else
	{
		if(IsOnViewport)SetCharacterVisibility(IsVisibileEnemy);
		else SetCharacterVisibility(false);
	}

}

void APerformanceUnit::CheckViewport()
{
	if (IsInViewport(GetActorLocation(), VisibilityOffset))
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
		if (IsOnViewport && OpenHealthWidget && !HealthBarUpdateTriggered && (!EnableFog || IsVisibileEnemy || IsMyTeam))
		{
			HealthBarWidget->SetVisibility(ESlateVisibility::Visible);
			HealthBarUpdateTriggered = true;
			//if(Projectile) ProjectileAndEffectsVisibility(Projectile->Mesh, Projectile->Mesh_B, Projectile->Niagara, Projectile->Niagara_B);
			//if(Projectile) Projectile->SetProjectileVisibility(true);
		}
		else if(HealthBarUpdateTriggered && !OpenHealthWidget)
		{
			//if(Projectile) Projectile->SetVisibility(false);
			HealthBarWidget->SetVisibility(ESlateVisibility::Collapsed);
			HealthBarUpdateTriggered = false;
		}
		
		if(HealthBarUpdateTriggered)	
			HealthBarWidget->UpdateWidget();
	}
}

void APerformanceUnit::ShowWorkAreaIfNoFog_Implementation(AWorkArea* WorkArea)
{
	if (WorkArea)
	{
		// Example condition checks: adapt these to your game logic
		if (IsOnViewport && (!EnableFog || IsVisibileEnemy || IsMyTeam))
		{
			if (WorkArea->Mesh)
			{
				//WorkArea->Mesh->SetVisibility(IsVisible, /* PropagateToChildren = */ true);
				WorkArea->Mesh->SetHiddenInGame(false);
			}
		}
	}
}

void APerformanceUnit::FireEffects_Implementation(UNiagaraSystem* ImpactVFX, USoundBase* ImpactSound, FVector ScaleVFX, float ScaleSound, float EffectDelay, float SoundDelay)
{

	//UE_LOG(LogTemp, Warning, TEXT("IsVisible: %d"), IsVisible);
	if (IsOnViewport && (!EnableFog || IsVisibileEnemy || IsMyTeam))
	{
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
							[this, ImpactVFX, ScaleVFX]()
							{
								if (GetWorld())
								{
									UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactVFX, GetActorLocation(), GetActorRotation(), ScaleVFX);
								}
							},
							EffectDelay,
							false
					);
				}else
				{
					UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactVFX, GetActorLocation(), GetActorRotation(), ScaleVFX);
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
						[this, ImpactSound, ScaleSound]()
						{
							if (GetWorld())
							{
								UGameplayStatics::PlaySoundAtLocation(GetWorld(), ImpactSound, GetActorLocation(), ScaleSound);
							}
						},
						SoundDelay,
						false
					);
				}else
				{
					UGameplayStatics::PlaySoundAtLocation(GetWorld(), ImpactSound, GetActorLocation(), ScaleSound);
				}
			}
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