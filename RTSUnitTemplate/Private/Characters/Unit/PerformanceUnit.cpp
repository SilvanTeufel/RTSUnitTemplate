// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/PerformanceUnit.h"

#include "Net/UnrealNetwork.h"
#include "Widgets/UnitBaseHealthBar.h"
#include "Widgets/UnitTimerWidget.h"
#include "Components/SkeletalMeshComponent.h"
#include "Controller/AIController/UnitControllerBase.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Engine/SkeletalMesh.h" // For USkeletalMesh
#include "Engine/SkeletalMeshLODSettings.h" // To access LOD settings
#include "Engine/SkinnedAssetCommon.h"

APerformanceUnit::APerformanceUnit(const FObjectInitializer& ObjectInitializer):Super(ObjectInitializer)
{

}

void APerformanceUnit::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	CheckViewport();
	CheckTeamVisibility();
	VisibilityTick();
	CheckHealthBarVisibility();
	CheckTimerVisibility();
	
}

void APerformanceUnit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(APerformanceUnit, HealthWidgetComp);
	DOREPLIFETIME(APerformanceUnit, TimerWidgetComp);
}

void APerformanceUnit::BeginPlay()
{
	Super::BeginPlay();
	

	

	//SetCharacterVisibility(false);
	SpawnFogOfWarManager();

	//FTimerHandle UnusedHandle;
	//GetWorldTimerManager().SetTimer(UnusedHandle, this, &APerformanceUnit::SetInvisibileIfNoOverlap, 1.1f, false);
}

void APerformanceUnit::Destroyed()
{
	Super::Destroyed();

	if (SpawnedFogManager)
	{
		// Detach the FogManager from this unit
		SpawnedFogManager->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

		// Destroy the FogManager
		SpawnedFogManager->Destroy();
		SpawnedFogManager = nullptr;
	}
}

void APerformanceUnit::SpawnFogOfWarManager()
{
	if(SpawnedFogManager) return;
	
	if (FogOfWarManagerClass)
	{
		if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
		{
			AControllerBase* ControllerBase = Cast<AControllerBase>(PlayerController);
			// Get the world reference
			if(ControllerBase && ControllerBase->SelectableTeamId == TeamId)
			{
				UWorld* World = GetWorld();
				if (World)
				{
					// Calculate the spawn location by applying the offset to the current actor location
					FVector SpawnLocation = GetActorLocation();

					// Spawn the FogOfWarManager at the new location
					SpawnedFogManager = World->SpawnActor<AFogOfWarManager>(FogOfWarManagerClass, SpawnLocation, FRotator::ZeroRotator);
					// Check if the actor was spawned successfully
					if (SpawnedFogManager)
					{
						SpawnedFogManager->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("rootSocket"));
					}
		
					if (SpawnedFogManager && SpawnedFogManager->Mesh)
					{
						// Scale the Mesh component of the spawned FogOfWarManager
			
							SpawnedFogManager->TeamId = TeamId;
							SpawnedFogManager->PlayerTeamId = ControllerBase->SelectableTeamId;
							SpawnedFogManager->Mesh->SetWorldScale3D(FVector(SightRadius, SightRadius, 1.f )*FogManagerMultiplier);
							SpawnedFogManager->Mesh->SetRelativeLocation(FogManagerPositionOffset);
							// Bind overlap events using the appropriate class and instance
							SpawnedFogManager->Mesh->OnComponentBeginOverlap.AddDynamic(SpawnedFogManager, &AFogOfWarManager::OnMeshBeginOverlap);
							SpawnedFogManager->Mesh->OnComponentEndOverlap.AddDynamic(SpawnedFogManager, &AFogOfWarManager::OnMeshEndOverlap);
							//SpawnedFogManager->Mesh->bHiddenInGame = true;
						
					}
				}
			}
		}
	}
}

void APerformanceUnit::SetInvisibileIfNoOverlap()
{
	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		AControllerBase* ControllerBase = Cast<AControllerBase>(PlayerController);

		if(!ControllerBase) return;
		
		if(!IsOverlappingFogOfWarManager(ControllerBase->SelectableTeamId))
			SetEnemyVisibility(false, ControllerBase->SelectableTeamId);
	}
}

bool APerformanceUnit::IsOverlappingFogOfWarManager(int PlayerTeamId)
{
	TArray<AActor*> OverlappingActors;
	GetOverlappingActors(OverlappingActors, AFogOfWarManager::StaticClass());

	for (AActor* Actor : OverlappingActors)
	{
		AFogOfWarManager* FogManager = Cast<AFogOfWarManager>(Actor);
		if (FogManager && FogManager->TeamId != PlayerTeamId)
		{
			UE_LOG(LogTemp, Warning, TEXT("!!!!!YESSSSSSSSSSSSSS Overlapped with FOG!!!!"));
			return true;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("!!!!!NOOOOOOOOOOOOOOOOO Overlapped with FOG!!!!"));
	return false;
}


void APerformanceUnit::SetEnemyVisibility(bool IsVisible, int PlayerTeamId)
{

	bool IsFriendly = PlayerTeamId == TeamId;


		if (IsVisible && !IsFriendly)
		{
			IsVisibileEnemy = true;
			SetCharacterVisibility(true);
		}
		else if(!IsVisible && !IsFriendly)
		{
			IsVisibileEnemy = false;
			SetCharacterVisibility(false);
		}

}

void APerformanceUnit::SetCharacterVisibility(bool desiredVisibility)
{

		USkeletalMeshComponent* SkelMesh = GetMesh();
		if (SkelMesh)
		{
			SkelMesh->SetVisibility(desiredVisibility, true);
			SkelMesh->bPauseAnims = !desiredVisibility;
			CurrentVisibility = desiredVisibility;
		}
}


void APerformanceUnit::VisibilityTick()
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
		if(ControllerBase->SelectableTeamId == TeamId)
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
		if (IsOnViewport)
		{
			//HealthBarWidget->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			HealthBarWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void APerformanceUnit::CheckTimerVisibility()
{
	if (UUnitTimerWidget* TimerWidget = Cast<UUnitTimerWidget>(TimerWidgetComp->GetUserWidgetObject())) // Assuming you have a UUnitBaseTimer class for the timer widget
	{
		if (IsOnViewport)
		{
			TimerWidget->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			TimerWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}