// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/UnitData.h"
#include "Kismet/GameplayStatics.h"
#include "Sound\SoundCue.h"
#include "GameplayEffect.h"
#include "Pickup.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API APickup : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	APickup();

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category = RTSUnitTemplate)
	class UCapsuleComponent* TriggerCapsule;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Type", Keywords = "Type CameraState"), Category = RTSUnitTemplate)
	TEnumAsByte<SelectableData::SelectableType> Type = SelectableData::Health;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USoundBase* Sound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int WeaponId; // Needed for MagazineCount
	
protected:
	// Called when the game starts or when spawneds
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void OnOverlapBegin(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float Amount = 500.f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<UGameplayEffect> PickupEffect;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<UGameplayEffect> PickupEffectTwo;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	float DestructionDelayTime = 0.1f;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void DestroySelectableWithDelay();
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void DestroySelectable();

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void ImpactEvent();

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	int TeamId = 0;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	float PickUpDistance = 25.f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool FollowTarget = false;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	AActor* Target;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float MovementSpeed = 13.f;
};
