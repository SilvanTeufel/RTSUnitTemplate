// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LogisticsData.generated.h"

/**
 * What a building does with resources that are brought to it.
 *
 * Replaces the old IsBase flag. IsBase still exists as a read-only mirror, true for the two types
 * workers deliver to (CollectAndStore, CollectOnly) - so every existing worker path keeps working
 * and a StoreOnly depot stays invisible to workers.
 */
UENUM(BlueprintType)
enum class EBaseType : uint8
{
	/** Not a base. Workers and logistics units ignore it. */
	None            UMETA(DisplayName = "None"),

	/** Workers deliver here and the resources are credited to the team at once (the old IsBase = true). */
	CollectAndStore UMETA(DisplayName = "Collect And Store"),

	/**
	 * Workers deliver here, but the resources are only kept in this building. Logistics units carry
	 * them over a road to the nearest CollectAndStore / StoreOnly base, where they are credited.
	 */
	CollectOnly     UMETA(DisplayName = "Collect Only"),

	/** Only logistics units deliver here; what they bring is credited to the team. */
	StoreOnly       UMETA(DisplayName = "Store Only"),

	/**
	 * Sits on a resource place and extracts it by itself - no workers. What it extracts is kept in
	 * the building like CollectOnly, and logistics units haul it to the nearest storing base. Built
	 * through a BuildArea that only snaps onto a free resource place of its resource type.
	 */
	CollectOnlyWithoutWorker UMETA(DisplayName = "Collect Only (Without Worker)"),
};

/**
 * What a unit is for. Replaces the old IsWorker flag, which stays as a read-only mirror
 * (true only for Worker) - a logistics unit is deliberately NOT a worker, so the worker AI,
 * idle-worker selection, building and repairing all leave it alone.
 */
UENUM(BlueprintType)
enum class EUnitRole : uint8
{
	/** Ordinary unit (the old IsWorker = false). */
	Combat    UMETA(DisplayName = "None (Combat)"),

	/** Gathers, builds and repairs (the old IsWorker = true). */
	Worker    UMETA(DisplayName = "Worker"),

	/** Automatically drives the roads between CollectOnly bases and the bases that store. */
	Logistics UMETA(DisplayName = "Logistics"),
};

/** Where a logistics unit is in its round trip. Only meaningful on the server. */
UENUM(BlueprintType)
enum class ELogisticsJobState : uint8
{
	Idle,
	ToPickup,
	Loading,
	ToDropoff,
	Unloading,
	/** The player (or combat) took over; the dispatcher waits until the unit has stood idle for a while. */
	Paused,
};
