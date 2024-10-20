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
	/*
	// Initialize FogOfWarLight (PointLight or SpotLight)
	//FogOfWarLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FogOfWarLight"));
	FogOfWarLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("FogOfWarLight"));
	// Attach the light component to the root or another component
	FogOfWarLight->SetupAttachment(RootComponent);

	// Set initial light properties
	FogOfWarLight->Intensity = 2.f;             // Adjust intensity as needed
	FogOfWarLight->bUseInverseSquaredFalloff = false; // Avoid using inverse squared falloff for performance
	FogOfWarLight->LightFalloffExponent = 0.5f;     // Exponent for smoother falloff (optional)
	FogOfWarLight->AttenuationRadius = 500.f;      // Default range (will update this to SightRange later)
	FogOfWarLight->CastShadows = false;

	// Rotate the light to point downwards at the character
	FogOfWarLight->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f)); // Point directly down
	*/
	/*
	// Load the light function material
	static ConstructorHelpers::FObjectFinder<UMaterial> LightFunctionMaterialFinder(*FogMaterial);
	if (LightFunctionMaterialFinder.Succeeded())
	{
		UE_LOG(LogTemp, Warning, TEXT("LightFunctionMaterialFinder Found!"));
	
		UMaterialInterface* FogOfWarLightFunctionMaterial = LightFunctionMaterialFinder.Object;

		// Create a dynamic material instance
		FogOfWarLightFunctionMaterialInstance = UMaterialInstanceDynamic::Create(FogOfWarLightFunctionMaterial, this);

		//FogOfWarLightFunctionMaterialInstance->SetScalarParameterValue(FName("MaxBrightness"), 0.5f);
		// Assign the material instance to the light function
		FogOfWarLight->LightFunctionMaterial = FogOfWarLightFunctionMaterialInstance;
	}*/
}

void APerformanceUnit::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	//VisibilityTick();
	//CheckHealthBarVisibility();
	//CheckTimerVisibility();
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
	

	if(!EnableFog) return;

	
	AControllerBase* ControllerBase = Cast<AControllerBase>(GetWorld()->GetFirstPlayerController());


	if(ControllerBase)
	{
		//SetVisibility(false, ControllerBase->SelectableTeamId);
		AUnitControllerBase* UnitController = Cast<AUnitControllerBase>(GetController());
		
		if(UnitController)
		{
			//SetFogOfWarLight(ControllerBase->SelectableTeamId, UnitController->SightRadius);
			//SetVisibileTeamId(ControllerBase->SelectableTeamId);
			ClientSetMeshVisibility(true); 
		}
	}
	
}

void APerformanceUnit::SpawnFogOfWarManager(FVector Scale)
{
	if (FogOfWarManagerClass)
	{
		// Get the world reference
		UWorld* World = GetWorld();
		if (World)
		{
			// Calculate the spawn location by applying the offset to the current actor location
			FVector SpawnLocation = GetActorLocation();

			// Spawn the FogOfWarManager at the new location
			SpawnedFogManager = World->SpawnActor<AFogOfWarManager>(FogOfWarManagerClass, SpawnLocation, FRotator::ZeroRotator);

			// Check if the actor was spawned successfully
			if (SpawnedFogManager && SpawnedFogManager->Mesh)
			{
				// Scale the Mesh component of the spawned FogOfWarManager
				SpawnedFogManager->Mesh->SetWorldScale3D(Scale*FogManagerMultiplier);
				
				SpawnedFogManager->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("rootSocket"));
				SpawnedFogManager->Mesh->SetRelativeLocation(FogManagerPositionOffset);

				// Bind overlap events using the appropriate class and instance
				SpawnedFogManager->Mesh->OnComponentBeginOverlap.AddDynamic(SpawnedFogManager, &AFogOfWarManager::OnMeshBeginOverlap);
				SpawnedFogManager->Mesh->OnComponentEndOverlap.AddDynamic(SpawnedFogManager, &AFogOfWarManager::OnMeshEndOverlap);
			}
		}
	}
}

void APerformanceUnit::SetFogOfWarLight(int PlayerTeamId, float SightRange)
{
	bool IsFriendly = PlayerTeamId == TeamId;

	if (IsFriendly)
	{
		SpawnFogOfWarManager(FVector(SightRange, SightRange, 1.f));
	}
}

void APerformanceUnit::SetVisibileTeamId(int PlayerTeamId)
{
UE_LOG(LogTemp, Warning, TEXT("SetVisibilityTeamId! PlayerTeamId: %d"), PlayerTeamId);
	bool IsFriendly = PlayerTeamId == TeamId;
	
	if( IsFriendly || !IsFriendly)
	{
		UE_LOG(LogTemp, Warning, TEXT("Set Visible!"));
		USkeletalMeshComponent* SkelMesh = GetMesh();
	
		if(SkelMesh)
		{
			//UE_LOG(LogTemp, Warning, TEXT("Disabled Mesh!"));
			UE_LOG(LogTemp, Warning, TEXT("Set Visible!"));
			SkelMesh->SetVisibility(true);
			SkelMesh->bPauseAnims = true;
		}
	}
}

void APerformanceUnit::SetVisibility(bool IsVisible, int PlayerTeamId)
{
	//UE_LOG(LogTemp, Warning, TEXT("Calle SetVisibility! %d"), IsVisible);
	//UE_LOG(LogTemp, Warning, TEXT("TeamId! %d"), TeamId);
	//UE_LOG(LogTemp, Warning, TEXT("PlayerTeamId! %d"), PlayerTeamId);
	bool IsFriendly = PlayerTeamId == TeamId;
	UE_LOG(LogTemp, Warning, TEXT("TeamId! %d"), TeamId);
	UE_LOG(LogTemp, Warning, TEXT("PlayerTeamId! %d"), PlayerTeamId);
	if (HasAuthority())
	{
		if (!IsOnViewport && IsInViewport(GetActorLocation(), VisibilityOffset) && IsVisible && !IsFriendly)
		{
			UE_LOG(LogTemp, Warning, TEXT("Set Visible!"));
			USkeletalMeshComponent* SkelMesh = GetMesh();
			IsOnViewport = true;
			if(SkelMesh)
			{
				SkelMesh->SetVisibility(true);
				SkelMesh->bPauseAnims = false;
	
			}
		}
		else if(IsOnViewport && !IsVisible && !IsFriendly)
		{
			UE_LOG(LogTemp, Warning, TEXT("Set Invisible!"));
			USkeletalMeshComponent* SkelMesh = GetMesh();
			IsOnViewport = false;
			if(SkelMesh)
			{
				//UE_LOG(LogTemp, Warning, TEXT("Disabled Mesh!"));
				SkelMesh->SetVisibility(false);
				SkelMesh->bPauseAnims = true;
			}
		}
	}else
	{
		// Client-specific visibility (only executed on clients)
		if (IsFriendly)
		{
			//ClientSetMeshVisibility(true);  // Call client RPC to set visibility
		}
		else
		{
			//ClientSetMeshVisibility(false);  // Call client RPC to set visibility
		}
		
	}
}

void APerformanceUnit::ClientSetMeshVisibility_Implementation(bool bIsVisible)
{
	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (SkelMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("Client: Setting mesh visibility to %s."), bIsVisible ? TEXT("true") : TEXT("false"));
		SkelMesh->SetVisibility(bIsVisible);
	}
}

void APerformanceUnit::VisibilityTick()
{
	if(EnableFog) return;
	
	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (IsInViewport(GetActorLocation(), VisibilityOffset))
	{
		if(SkelMesh)
		{
			SkelMesh->SetVisibility(true);
			SkelMesh->bPauseAnims = false;
		}
	}
	else
	{
		if(SkelMesh)
		{
			SkelMesh->SetVisibility(false);
			SkelMesh->bPauseAnims = true;
		}
	}
}

void APerformanceUnit::CheckVisibility(int PlayerTeamId)
{


	if (IsInViewport(GetActorLocation(), VisibilityOffset) && PlayerTeamId == TeamId)
	{
		USkeletalMeshComponent* SkelMesh = GetMesh();
		IsOnViewport = true;
		if(SkelMesh)
		{
			SkelMesh->SetVisibility(true);
			SkelMesh->bPauseAnims = false;
		}
	}
	else if(PlayerTeamId == TeamId)
	{
		USkeletalMeshComponent* SkelMesh = GetMesh();
		IsOnViewport = false;
		if(SkelMesh)
		{
			SkelMesh->SetVisibility(false);
			SkelMesh->bPauseAnims = true;
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