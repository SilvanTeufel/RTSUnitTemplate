// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/UnitData.h"
#include "Kismet/GameplayStatics.h"
#include "Sound\SoundCue.h"
#include "Selectable.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API ASelectable : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASelectable();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "TriggerCapsule", Keywords = "SideScroller3D TriggerCapsule"), Category = TopDownRTSTemplate)
	class UCapsuleComponent* TriggerCapsule;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Type", Keywords = "Type CameraState"), Category = TopDownRTSTemplate)
	TEnumAsByte<SelectableData::SelectableType> Type = SelectableData::Health;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	USoundBase* Sound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	int WeaponId; // Needed for MagazineCount
	
protected:
	// Called when the game starts or when spawneds
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void OnOverlapBegin(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Amount", Keywords = "Amount CameraState"), Category = TopDownRTSTemplate)
	float Amount = 500.f;
};
