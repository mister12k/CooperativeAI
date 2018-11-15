// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CooperativeAIGameMode.h"
#include "CooperativeAICharacter.h"
#include "STrackerBot.h"
#include "SHealthComponent.h"
#include "SGameState.h"
#include "SPlayerState.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

ACooperativeAIGameMode::ACooperativeAIGameMode()
{
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
	
	TimeBetweenWaves = 30.0f;

	GameStateClass = ASGameState::StaticClass();
	PlayerStateClass = ASPlayerState::StaticClass();

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.0f;

	BestGlobalAngle = 0.0f;

	// Initializing the data structures
	float AttackAnglesAroundPlayer = 360 / BOTS;
	float Angle = 0.0f;
	for (int i = 0; i < BOTS; i++, Angle += AttackAnglesAroundPlayer) {
		PossibleAttackAngles.Add(Angle);
		LocalAttackAngles.Add(Angle);
		AnglesDamaged.Add(Angle, false);
		PheromoneQuantities.Add(Angle, 1.0f); // Pheromone set initially to 1.0 to avoid division by zero in selection probabilities
		HitAngles.Add(Angle, 0.0f);
	}
}

void ACooperativeAIGameMode::BeginPlay()
{
	Super::BeginPlay();

	/*APlayerController* localPlayer2 = UGameplayStatics::CreatePlayer(GetWorld(), -1, true);
	APlayerController* localPlayer3 = UGameplayStatics::CreatePlayer(GetWorld(), -1, true);
	APlayerController* localPlayer4 = UGameplayStatics::CreatePlayer(GetWorld(), -1, true);*/

}


void ACooperativeAIGameMode::StartWave()
{
	NrOfBotsToSpawn = BOTS;

	GetWorldTimerManager().SetTimer(TimerHandle_BotSpawner, this, &ACooperativeAIGameMode::SpawnBotTimerElapsed, 0.01f, true, 5.0f);

	SetWaveState(EWaveState::WaveInProgress);

}


void ACooperativeAIGameMode::EndWave()
{
	GetWorldTimerManager().ClearTimer(TimerHandle_BotSpawner);

	// Diffusion phase, the non damaging bots seek new hypothesis for themselves in SDS
	if (bStochasticMode) {
		StochasticDiffusionSearch();
	}

	// Ant selection of solution and deposit/evaporation of pheromones
	else if (bAntColonyMode) {
		AntColonyOptimization();
	}

	else if (bParticleSwarmMode) {
		ParticleSwarmOptimization();
	}

	

	PrepareForNextWave();
}


void ACooperativeAIGameMode::StochasticDiffusionSearch() {

	//  Set random hypothesis to the newly spawned bots
	int Random;
	for (TActorIterator<ASTrackerBot> ActorItr(GetWorld()); ActorItr; ++ActorItr)
	{
		Random = (rand() % static_cast<int>((BOTS - 1) - 1));
		if (ActorItr->AttackAngle == 0.0f) {
			ActorItr->AttackAngle = PossibleAttackAngles[Random];
		}
	}

	// Check if hypothesis has damaged players and if not select another at random up to twice
	for (TActorIterator<ASTrackerBot> ActorItr(GetWorld()); ActorItr; ++ActorItr)
	{
		if (AnglesDamaged[ActorItr->AttackAngle]) {
			continue;
		}
		else {
			Random = (rand() % static_cast<int>((NrOfBotsToSpawn - 1) - 1));

			if (AnglesDamaged[PossibleAttackAngles[Random]]) {
				ActorItr->AttackAngle = PossibleAttackAngles[Random];
			}
			else {
				Random = (rand() % static_cast<int>((NrOfBotsToSpawn - 1) - 1));
				ActorItr->AttackAngle = PossibleAttackAngles[Random];
			}
		}
	}
}

void ACooperativeAIGameMode::AntColonyOptimization() {
	
	float Random;
	float TotalPheromones = 0.0f;

	//  Set random hypothesis to the newly spawned bots (will be changed by the algorithm)
	for (TActorIterator<ASTrackerBot> ActorItr(GetWorld()); ActorItr; ++ActorItr)
	{
		Random = (rand() % static_cast<int>((BOTS - 1) - 1));
		if (ActorItr->AttackAngle == 0.0f) {
			ActorItr->AttackAngle = PossibleAttackAngles[Random];
		}
	}

	// Evaporation and deposit of pheromones
	for (auto Itr = PheromoneQuantities.CreateIterator(); Itr; ++Itr)
	{
		PheromoneQuantities.Add(Itr.Key(), (1 - EVAPORATION_RATE) * Itr.Value());

		if (AnglesDamaged[Itr.Key()]) {
			PheromoneQuantities[Itr.Key()] += 1.0f;
		}

		TotalPheromones += Itr.Value();
	}

	// Selection

	// <AttackAngle, Pheromones as a percentage of total pheromones>
	TMap<float, float> SelectionProbability;
	for (auto Itr = PheromoneQuantities.CreateIterator(); Itr; ++Itr)
	{
		SelectionProbability.Add(Itr.Key(), (Itr.Value() / TotalPheromones));
	}

	for (TActorIterator<ASTrackerBot> ActorItr(GetWorld()); ActorItr; ++ActorItr)
	{
		float GeneratedProbability = 0.0f, PreviousProbability = 0.0f;
		Random = static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / 1.0f));
		for (auto Itr = SelectionProbability.CreateIterator(); Itr; ++Itr)
		{
			GeneratedProbability += Itr.Value();
			if (Random < GeneratedProbability && Random > PreviousProbability) {
				ActorItr->AttackAngle = Itr.Key();
				break;
			}
			PreviousProbability = GeneratedProbability;
		}
	}
}

void ACooperativeAIGameMode::ParticleSwarmOptimization() {

	float Random;

	//  Set random hypothesis to the newly spawned bots (will be changed by the algorithm)
	for (TActorIterator<ASTrackerBot> ActorItr(GetWorld()); ActorItr; ++ActorItr)
	{
		Random = (rand() % static_cast<int>((BOTS - 1) - 1));
		if (ActorItr->AttackAngle == 0.0f) {
			ActorItr->AttackAngle = PossibleAttackAngles[Random];
		}

		Random = (rand() % static_cast<int>((NrOfBotsToSpawn - 1) - 1));
		ActorItr->BestLocalAngle = LocalAttackAngles[Random];
	}

	// Random numbers to continue the solution space search both locally (one actor) and globally (whole swarm)
	float RandomLocal, RandomGlobal;

	// Weights on how much do the actual actor's angle, actor's best known angle and swarm's best known angle affect the next selected angle
	float ConstantWeight = 0.1f, LocalWeight = 0.45f, GlobalWeight = 0.45f;

	// The new angle an actor will take
	float NewAngle;

	for (TActorIterator<ASTrackerBot> ActorItr(GetWorld()); ActorItr; ++ActorItr)
	{
		
		RandomLocal = static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / 1.0f));
		RandomGlobal = static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / 1.0f));

		NewAngle = (ConstantWeight * ActorItr->AttackAngle) + (LocalWeight * RandomLocal * ActorItr->BestLocalAngle) + (GlobalWeight * RandomGlobal * BestGlobalAngle);

		NewAngle = fmod(NewAngle,360.0f);

		for (int AngleIndex = 0; AngleIndex < PossibleAttackAngles.Num(); AngleIndex++)
		{
			if (NewAngle < PossibleAttackAngles[AngleIndex]) {
				NewAngle = PossibleAttackAngles[AngleIndex];
				break;
			}
		}

		ActorItr->AttackAngle = NewAngle;

		if (HitAngles[NewAngle] > HitAngles[ActorItr->BestLocalAngle]) {
			Random = (rand() % static_cast<int>((BOTS - 1) - 1));
			LocalAttackAngles[Random] = NewAngle;
			
			if (HitAngles[ActorItr->BestLocalAngle] > HitAngles[BestGlobalAngle]) {
				BestGlobalAngle = ActorItr->BestLocalAngle;
			}
		}
	}
}


void ACooperativeAIGameMode::PrepareForNextWave()
{
	GetWorldTimerManager().SetTimer(TimerHandle_NextWaveStart, this, &ACooperativeAIGameMode::StartWave, TimeBetweenWaves, false);

	SetWaveState(EWaveState::WaitingForNextWave);

	RestartDeadPlayers();
}


void ACooperativeAIGameMode::CheckAnyPlayerAlive()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->GetPawn())
		{
			APawn* MyPawn = PC->GetPawn();
			USHealthComponent* HealthComp = Cast<USHealthComponent>(MyPawn->GetComponentByClass(USHealthComponent::StaticClass()));
			if (ensure(HealthComp) && HealthComp->GetHealth() > 0.0f)
			{
				// A player is still alive.
				return;
			}
		}
	}

	// No player alive
	GameOver();
}


void ACooperativeAIGameMode::GameOver()
{
	EndWave();


	SetWaveState(EWaveState::GameOver);

	UE_LOG(LogTemp, Log, TEXT("GAME OVER! Players Died"));
}


void ACooperativeAIGameMode::SetWaveState(EWaveState NewState)
{
	ASGameState* GS = GetGameState<ASGameState>();
	if (ensureAlways(GS))
	{
		GS->SetWaveState(NewState);
	}
}


void ACooperativeAIGameMode::RestartDeadPlayers()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->GetPawn() == nullptr)
		{
			RestartPlayer(PC);
		}
	}
}


void ACooperativeAIGameMode::StartPlay()
{
	Super::StartPlay();

	StartWave();
}


void ACooperativeAIGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	CheckAnyPlayerAlive();

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASTrackerBot::StaticClass(), FoundActors);

	if (FoundActors.Num() > BOTS * 3) {
		NrOfBotsToSpawn = 0;
	}
}

void ACooperativeAIGameMode::SetStochasticMode()
{
	bStochasticMode = true;
}

void ACooperativeAIGameMode::SetAntColonyMode()
{
	bAntColonyMode = true;
}

void ACooperativeAIGameMode::SetParticleSwarmMode()
{
	bParticleSwarmMode = true;
}

void ACooperativeAIGameMode::SpawnBotTimerElapsed()
{
	SpawnNewBot();

	NrOfBotsToSpawn--;

	if (NrOfBotsToSpawn <= 0)
	{
		EndWave();
	}


}