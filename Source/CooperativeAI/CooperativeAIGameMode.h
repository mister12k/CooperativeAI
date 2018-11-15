// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CooperativeAIGameMode.generated.h"
#define BOTS 20
#define EVAPORATION_RATE 0.25f
enum class EWaveState : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnActorKilled, AActor*, VictimActor, AActor*, KillerActor, AController*, KillerController);

UCLASS()
class ACooperativeAIGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACooperativeAIGameMode();
	virtual void BeginPlay() override;
protected:

	FTimerHandle TimerHandle_BotSpawner;

	FTimerHandle TimerHandle_NextWaveStart;

	// Bots to spawn in current wave
	int32 NrOfBotsToSpawn;

	UPROPERTY(EditDefaultsOnly, Category = "GameMode")
		float TimeBetweenWaves;

	// Array of angles from which the bots can attack
	TArray<float> PossibleAttackAngles;

protected:

	// Hook for BP to spawn a single bot
	UFUNCTION(BlueprintImplementableEvent, Category = "GameMode")
		void SpawnNewBot();

	void StochasticDiffusionSearch();

	void AntColonyOptimization();

	void ParticleSwarmOptimization();

	void SpawnBotTimerElapsed();

	// Start Spawning Bots
	void StartWave();

	// Stop Spawning Bots
	void EndWave();

	// Set timer for next startwave
	void PrepareForNextWave();

	void CheckAnyPlayerAlive();

	void GameOver();

	void SetWaveState(EWaveState NewState);

	void RestartDeadPlayers();

public:

	virtual void StartPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(BlueprintAssignable, Category = "GameMode")
		FOnActorKilled OnActorKilled;

	bool bStochasticMode;

	bool bAntColonyMode;

	bool bParticleSwarmMode;

	UFUNCTION(BlueprintCallable, Category = "GameMode")
		void SetStochasticMode();

	UFUNCTION(BlueprintCallable, Category = "GameMode")
		void SetAntColonyMode();

	UFUNCTION(BlueprintCallable, Category = "GameMode")
		void SetParticleSwarmMode();


	// <AttackAngle, Has damage been done through this angle on the previous wave>
	TMap<float, bool> AnglesDamaged;

	// <AttackAngle, Pheromone quantity>
	TMap<float, float> PheromoneQuantities;

	// <AttackAngle, Times succesfully hit>
	TMap<float, float> HitAngles;

	// Stores the current best angle for the swarm in PSO mode
	float BestGlobalAngle;

	// Array of best local angles to transmit information between waves in PSO
	TArray<float> LocalAttackAngles;
};



