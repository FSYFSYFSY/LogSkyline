// Copyright Epic Games, Inc. All Rights Reserved.


#include "CombatDummy.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

ACombatDummy::ACombatDummy()
{
 	PrimaryActorTick.bCanEverTick = true;

	// create the root
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// create the base plate
	BasePlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base Plate"));
	BasePlate->SetupAttachment(RootComponent);

	// create the dummy
	Dummy = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Dummy"));
	Dummy->SetupAttachment(RootComponent);

	Dummy->SetSimulatePhysics(true);

	// create the physics constraint
	PhysicsConstraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("Physics Constraint"));
	PhysicsConstraint->SetupAttachment(RootComponent);

	PhysicsConstraint->SetConstrainedComponents(BasePlate, NAME_None, Dummy, NAME_None);
}

void ACombatDummy::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{

	if (CurrentHP <= 0.0f)
	{
		return;
	}

	CurrentHP = FMath::Max(0.0f, CurrentHP - Damage);
	UE_LOG(LogTemp, Log, TEXT("[Dummy] HP %.0f / %.0f"), CurrentHP, MaxHP);

	// apply impulse to the dummy
	Dummy->AddImpulseAtLocation(DamageImpulse, DamageLocation);

	// call the BP handler
	BP_OnDummyDamaged(DamageLocation, DamageImpulse.GetSafeNormal());

	if (CurrentHP <= 0.0f)
	{
		HandleDeath();
	}
}

void ACombatDummy::HandleDeath()
{
	// 断开约束：靶体不再被钉在底座上
	if (PhysicsConstraint)
	{
		PhysicsConstraint->BreakConstraint();
	}

	UE_LOG(LogTemp, Log, TEXT("[Dummy] 已击毁，约束已断开"));
}

void ACombatDummy::ApplyHealing(float Healing, AActor* Healer)
{
	// unused
}

void ACombatDummy::NotifyDanger(const FVector& DangerLocation, AActor* DangerSource)
{
	// unused
}

void ACombatDummy::BeginPlay()
{
	Super::BeginPlay();
	CurrentHP = MaxHP;
}

