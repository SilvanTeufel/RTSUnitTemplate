// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "RunStateProcessor.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API URunStateProcessor : public UMassProcessor
{
	GENERATED_BODY()

	URunStateProcessor();
public:
	
	virtual void ConfigureQueries() override;
};
