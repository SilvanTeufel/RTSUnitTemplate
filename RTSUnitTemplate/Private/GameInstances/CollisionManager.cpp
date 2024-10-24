// Fill out your copyright notice in the Description page of Project Settings.

#include "GameInstances/CollisionManager.h"
#include "Engine/World.h"  


void UCollisionManager::RegisterCollision()
{
	CollisionCount++;
}

bool UCollisionManager::CanProcessCollision() const
{
	return CollisionCount < MaxCollisionsPerSecond;
}

void UCollisionManager::ResetCollisionCountOnTime()
{
	float CurrentTime = GetWorld()->GetTimeSeconds();

	if (CurrentTime - LastResetTime > TimeWindow)
	{
		CollisionCount = 0;
		LastResetTime = CurrentTime;
	}
	
}