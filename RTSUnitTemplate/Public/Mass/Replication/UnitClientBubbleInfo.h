#pragma once

#include "CoreMinimal.h"
#include "MassClientBubbleInfoBase.h"
#include "Mass/Replication/UnitReplicationPayload.h"
#include "UnitClientBubbleInfo.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API AUnitClientBubbleInfo : public AMassClientBubbleInfoBase
{
	GENERATED_BODY()
public:
	AUnitClientBubbleInfo(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITORONLY_DATA
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// Die replizierten Agenten für diese Bubble
	UPROPERTY(ReplicatedUsing=OnRep_Agents)
	FUnitReplicationArray Agents;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_Agents();

protected:
	virtual void BeginPlay() override;
};
