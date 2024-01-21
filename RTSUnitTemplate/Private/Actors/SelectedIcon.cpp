// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Actors/SelectedIcon.h"
#include "Components/SphereComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/Material.h"
#include "Engine/StaticMesh.h"

// Sets default values
ASelectedIcon::ASelectedIcon()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;
    IconMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SphereMesh"));
    RootComponent = IconMesh;
    IconMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // QueryAndPhysics
    //IconMesh->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshAsset(*SphereMeshAssetPath);
    if (SphereMeshAsset.Succeeded())
    {
        IconMesh->SetStaticMesh(SphereMeshAsset.Object);
        IconMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -40.0f));

        static ConstructorHelpers::FObjectFinder<UMaterial> MaterialBlue(*MaterialBluePath);
        static ConstructorHelpers::FObjectFinder<UMaterial> MaterialAction(*MaterialActionPath);

        if (MaterialBlue.Object != NULL)
        {
            BlueMaterial = (UMaterial*)MaterialBlue.Object;
            IconMesh->SetMaterial(0, BlueMaterial);
        }

        if (MaterialAction.Object != NULL)
        {
            ActionMaterial = (UMaterial*)MaterialAction.Object;
            IconMesh->SetMaterial(0, ActionMaterial);
        }

        IconMesh->bHiddenInGame = true;
    }
}

// Called when the game starts or when spawned
void ASelectedIcon::BeginPlay()
{
    Super::BeginPlay();
}

// Called every frame
void ASelectedIcon::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

}

void ASelectedIcon::ChangeMaterialColour(FVector4d Colour)
{
    DynMaterial = UMaterialInstanceDynamic::Create(BlueMaterial, this);
    DynMaterial->SetVectorParameterValue(TEXT("RingColor"), Colour); //FVector4d(100.f, 10.f, 5.f, 0.5f)
    IconMesh->SetMaterial(0, DynMaterial);
}

void ASelectedIcon::ChangeMaterialToAction()
{
    DynMaterial = UMaterialInstanceDynamic::Create(ActionMaterial, this);
    IconMesh->SetMaterial(0, DynMaterial);
}

