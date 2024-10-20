#include "Actors/FogOfWarManager.h"
#include "Hud/HUDBase.h"

#include "Characters/Unit/PerformanceUnit.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Net/UnrealNetwork.h"


AFogOfWarManager::AFogOfWarManager()
{
    PrimaryActorTick.bCanEverTick = true;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
    Mesh->SetRenderInMainPass(false);
    Mesh->SetRenderInDepthPass(false);
    Mesh->SetCastShadow(false);

    // Register overlap events
   // Mesh->OnComponentBeginOverlap.AddDynamic(this, &AFogOfWarManager::OnMeshBeginOverlap);
   // Mesh->OnComponentEndOverlap.AddDynamic(this, &AFogOfWarManager::OnMeshEndOverlap);
    bReplicates = true;

}


void AFogOfWarManager::BeginPlay()
{
    Super::BeginPlay();

    // Get a reference to the PlayerController and cast to AControllerBase

        AControllerBase* ControllerBase = Cast<AControllerBase>(GetWorld()->GetFirstPlayerController());
        if (ControllerBase)
        {
            PlayerTeamId = ControllerBase->SelectableTeamId;
            if (ControllerBase)
            {
                HUDBase = Cast<AHUDBase>(ControllerBase->GetHUD());
            }
        }



}

void AFogOfWarManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    /*
    AHUDBase* HUD = Cast<AHUDBase>(HUDBase);
    // Iterate through all units in the AllUnits array

    for (AActor* Actor : HUD->AllUnits)
    {
        APerformanceUnit* Unit = Cast<APerformanceUnit>(Actor);
        
        if (Unit)
        {
            float Distance = FVector::Dist(Unit->GetActorLocation(), GetActorLocation());
            
            if (Distance <= 3000.f && Unit->TeamId != PlayerTeamId)
            {
                Unit->SetVisibility(true, PlayerTeamId);
            }
            else if (Distance > 3000.f && Unit->TeamId != PlayerTeamId)
            {
                Unit->SetVisibility(false, PlayerTeamId);
            }
        }
    }
    */
}


void AFogOfWarManager::OnMeshBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    //UE_LOG(LogTemp, Warning, TEXT("FogOfWarManager OnMeshBeginOverlap called."));
    APerformanceUnit* Unit = Cast<APerformanceUnit>(OtherActor);
    
    if (Unit && PlayerTeamId != Unit->TeamId &&  !Unit->IsOnViewport)
    {
            Unit->SetVisibility(true, PlayerTeamId);
    }
}

void AFogOfWarManager::OnMeshEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
   // UE_LOG(LogTemp, Warning, TEXT("FogOfWarManager OnMeshEndOverlap called."));
    APerformanceUnit* Unit = Cast<APerformanceUnit>(OtherActor);
    if (Unit && PlayerTeamId != Unit->TeamId && Unit->IsOnViewport)
    {
            Unit->SetVisibility(false, PlayerTeamId);
    }
}
