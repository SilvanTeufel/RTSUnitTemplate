// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/UnitActor.h"
#include "AbilitySystemComponent.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "Net/UnrealNetwork.h"

// Sets default values
AUnitActor::AUnitActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	// Root Component = Capsule (works for both modes)
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
	CapsuleComponent->InitCapsuleSize(34.f, 88.f);
	CapsuleComponent->SetSimulatePhysics(true);
	RootComponent = CapsuleComponent;

	// Skeletal Mesh (only visible when in Skeletal mode)
	SkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));
	SkeletalMesh->SetupAttachment(CapsuleComponent);
	SkeletalMesh->SetVisibility(false);

	// ISM Component (optional — only visible in ISM mode)
	ISMComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ISMComponent"));
	ISMComponent->SetupAttachment(CapsuleComponent);
	ISMComponent->SetVisibility(false);

	MovementComponent = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("MovementComponent"));
	MovementComponent->UpdatedComponent = CapsuleComponent;
	// Unit Logic (shared gameplay logic)

	SightSphere = CreateDefaultSubobject<USphereComponent>(TEXT("SightSphere"));
	SightSphere->SetupAttachment(RootComponent);

	// Make this a true “overlap trigger” for any dynamic (pawn) actors
	SightSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

	// Ensure we start disabled; we’ll turn it on per client
	SightSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SightSphere->SetGenerateOverlapEvents(false);

	// Make sure it can move at runtime
	SightSphere->SetMobility(EComponentMobility::Movable);

	
}

void AUnitActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUnitActor, TeamId);
	
	DOREPLIFETIME(AUnitActor, CanOnlyAttackGround);
	DOREPLIFETIME(AUnitActor, CanOnlyAttackFlying);
	DOREPLIFETIME(AUnitActor, CanDetectInvisible);
	DOREPLIFETIME(AUnitActor, CanAttack);
	DOREPLIFETIME(AUnitActor, IsInvisible);
	DOREPLIFETIME(AUnitActor, IsFlying);

	DOREPLIFETIME(AUnitActor, Attributes);
}

void AUnitActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	SetReplicates(true);
	SetReplicateMovement(true);
}

// Called when the game starts or when spawned
void AUnitActor::BeginPlay()
{
	Super::BeginPlay();
	
	InitializeUnitMode();


}

void AUnitActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Only add once (or whenever mesh/flag changes)
	if (!bUseSkeletalMovement 
		&& ISMComponent 
		&& ISMComponent->GetStaticMesh() 
		&& InstanceIndex == INDEX_NONE)
	{
		// This is still early enough that the ISM data manager
		// will allow an append from INDEX_NONE
		ISMComponent->ClearInstances();
		InstanceIndex = ISMComponent->AddInstance(Transform, /*bWorldSpace=*/true);
	}
}

// Called every frame
void AUnitActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}



void AUnitActor::InitializeUnitMode()
{
	if (bUseSkeletalMovement)
	{
		SkeletalMesh->SetVisibility(true);
		ISMComponent->SetVisibility(false);
	}
	else
	{
		SkeletalMesh->SetVisibility(false);
		ISMComponent->SetVisibility(true);
	}
}

void AUnitActor::CreateISMInstance()
{
	if (!bUseSkeletalMovement && ISMComponent && ISMComponent->GetStaticMesh())
	{
		//const FTransform InstanceTransform = GetActorTransform();
		//InstanceIndex = ISMComponent->AddInstance(InstanceTransform, true);

		// 1) Prepare a single-element array of transforms
		TArray<FTransform> NewTransforms;
		NewTransforms.Add(GetActorTransform());

		// 2) AddInstances with bShouldReturnIndices=true, bWorldSpace=true
		TArray<int32> NewIndices = ISMComponent->AddInstances(
			NewTransforms,
			/* bShouldReturnIndices = */ true,
			/* bWorldSpace = */ true
			// bUpdateNavigation defaults to true
		);

		// 3) Store the returned index
		if (NewIndices.Num() > 0)
		{
			InstanceIndex = NewIndices[0];
		}
	}
}


void AUnitActor::MulticastSetEnemyVisibility_Implementation(AUnitActor* DetectingActor, bool bVisible)
{
	
	UWorld* World = GetWorld();
	if (!World) return;  // Safety check
	
	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!PlayerController || Cast<ACustomControllerBase>(PlayerController)->SelectableTeamId != DetectingActor->TeamId) return;


	IsVisibleEnemy = bVisible;
}

void AUnitActor::HandleBeginOverlapDetection(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	
	//UE_LOG(LogTemp, Log, TEXT("SphereBeginOverlap! "));
	
    if (!OtherActor || !OtherActor->IsValidLowLevel() || !IsValid(OtherActor))
    {
    	///UE_LOG(LogTemp, Warning, TEXT("  -> Ignored invalid OtherActor"));
        return;
    }
    
    AUnitActor* DetectedUnit = Cast<AUnitActor>(OtherActor);

    
    if (DetectedUnit && TeamId != DetectedUnit->TeamId)
    {
        if (CanDetectInvisible && DetectedUnit->IsInvisible)
        {
            DetectedUnit->IsInvisible = false;
            DetectedUnit->DetectorOverlaps++;
        }
        
        DetectedUnit->IsVisibleEnemy = true;
        DetectedUnit->FogManagerOverlaps++;
    }
    
}


// Existing code: Handle end overlap adjustments
void AUnitActor::HandleEndOverlapDetection(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex
)
{
	
    if (!OtherActor || !OtherActor->IsValidLowLevel() || !IsValid(OtherActor))
    {
        return;
    }
    
    AUnitActor* DetectedUnit = Cast<AUnitActor>(OtherActor);

    
    if (DetectedUnit && TeamId != DetectedUnit->TeamId)
    {
        DetectedUnit->FogManagerOverlaps--;
        if(DetectedUnit->FogManagerOverlaps < 0) DetectedUnit->FogManagerOverlaps = 0;

        if (CanDetectInvisible && DetectedUnit->DetectorOverlaps > 0)
        {
            DetectedUnit->DetectorOverlaps--;

            if (DetectedUnit->DetectorOverlaps <= 0)
            {
                FTimerHandle TimerHandleDetector;
                GetWorld()->GetTimerManager().SetTimer(TimerHandleDetector, [DetectedUnit]()
                {
                    DetectedUnit->IsInvisible = true;
                }, 1.f, false);
            }
        }
        
        if(DetectedUnit->FogManagerOverlaps > 0) return;

  
        if (DetectedUnit->Attributes->GetHealth() <= 0.f) // Unit->GetUnitState() == UnitData::Dead
        {
            FTimerHandle TimerHandle;
            GetWorld()->GetTimerManager().SetTimer(TimerHandle, [DetectedUnit]() {
                DetectedUnit->IsVisibleEnemy = false;
            }, DetectedUnit->FogDeadVisibilityTime, false);
        }else
        {
            DetectedUnit->IsVisibleEnemy = false;
        }
    }
}