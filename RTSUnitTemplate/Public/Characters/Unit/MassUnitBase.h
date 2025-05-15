// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/Unit/WorkingUnitBase.h"
#include "MassUnitBase.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API AMassUnitBase : public AWorkingUnitBase
{
	GENERATED_BODY()

public:
	
	AMassUnitBase(const FObjectInitializer& ObjectInitializer);

	// The Mass Actor Binding Component
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mass)
	UMassActorBindingComponent* MassActorBindingComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Mode")
	bool bUseSkeletalMovement = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components")
	UInstancedStaticMeshComponent* ISMComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ISM")
	int32 InstanceIndex = INDEX_NONE;
	/**
* Adds a specific Mass Tag to the entity associated with this Actor.
* @param TagToAdd The script struct representing the tag type to add (e.g., FMassStateIdleTag::StaticStruct()).
* @return True if the tag was added successfully or already existed, false otherwise (e.g., invalid entity, invalid tag struct).
*/
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool AddTagToEntity(UScriptStruct* TagToAdd);

	/**
	 * Removes a specific Mass Tag from the entity associated with this Actor.
	 * @param TagToRemove The script struct representing the tag type to remove (e.g., FMassStateIdleTag::StaticStruct()).
	 * @return True if the tag was removed successfully or didn't exist, false otherwise (e.g., invalid entity, invalid tag struct).
	 */
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool RemoveTagFromEntity(UScriptStruct* TagToRemove);

	/**
	 * Removes ALL Mass Tags from the entity associated with this Actor. Use with caution!
	 * @return True if tags were iterated and removal attempted, false if the entity was invalid.
	 */
	UFUNCTION(BlueprintCallable, Category = Mass)
	bool SwitchEntityTag(UScriptStruct* TagToAdd);

	UFUNCTION(BlueprintCallable, Category = Mass)
	bool SyncTranslation();
	
	bool GetMassEntityData(FMassEntityManager*& OutEntityManager, FMassEntityHandle& OutEntityHandle);

protected:
	virtual void BeginPlay() override;
	
	virtual void OnConstruction(const FTransform& Transform) override;
	
	void InitializeUnitMode();

};
