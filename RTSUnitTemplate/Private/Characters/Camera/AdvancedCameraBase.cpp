
#include "Characters/Camera/AdvancedCameraBase.h"

AAdvancedCameraBase::AAdvancedCameraBase(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// Initialize default custom offset

	// Adjust as necessary
}

void AAdvancedCameraBase::BeginPlay()
{
	Super::BeginPlay(); // Call the parent class's BeginPlay
	CustomSpawnPlatform(); // Additionally, spawn a custom platform
}

void AAdvancedCameraBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime); // Call the parent class's Tick
	// Here you can add any custom tick behaviors for the child class
}

void AAdvancedCameraBase::CustomSpawnPlatform()
{
	if (!CustomAttachedPlatform && CustomSpawnPlatformClass != nullptr)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		//FVector Location = CameraComp->GetComponentLocation() + CameraComp->GetForwardVector() * CustomPlatformOffset.X + CameraComp->GetUpVector() * CustomPlatformOffset.Z;
		FVector Location = CameraComp->GetComponentLocation() 
					+ CameraComp->GetForwardVector() * CustomPlatformOffset.X  // Forward offset (local X-axis)
					+ CameraComp->GetRightVector() * CustomPlatformOffset.Y    // Right offset (local Y-axis)
					+ CameraComp->GetUpVector() * CustomPlatformOffset.Z;      // Upward offset (local Z-axis)


		FRotator Rotation = FRotator(0.0f, CameraComp->GetComponentRotation().Yaw, 0.0f);

		CustomAttachedPlatform = GetWorld()->SpawnActor<AUnitSpawnPlatform>(CustomSpawnPlatformClass, Location, Rotation, SpawnParameters);
		if (CustomAttachedPlatform)
		{
			CustomAttachedPlatform->AttachToComponent(CameraComp, FAttachmentTransformRules::KeepWorldTransform);
			UE_LOG(LogTemp, Log, TEXT("CustomSpawnPlatform: Platform spawned and attached successfully."));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("CustomSpawnPlatform: Failed to spawn platform."));
		}
	}else
	{
		if (CustomAttachedPlatform)
		{
			UE_LOG(LogTemp, Warning, TEXT("CustomSpawnPlatform: Platform already exists."));
		}
		else if (CustomSpawnPlatformClass == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("CustomSpawnPlatform: No platform class specified."));
		}
	}
}