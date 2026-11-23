// Copyright Epic Games, Inc. All Rights Reserved.


#include "CombatDummy.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "CombatLifeBar.h"

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

	// create the life bar widget component
	LifeBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("LifeBar"));
	LifeBar->SetupAttachment(RootComponent);
}

void ACombatDummy::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{

	if (CurrentHP <= 0.0f)
	{
		return;
	}

	CurrentHP = FMath::Max(0.0f, CurrentHP - Damage);
	UE_LOG(LogTemp, Log, TEXT("[Dummy] HP %.0f / %.0f"), CurrentHP, MaxHP);

	// 推血条。MaxHP 允许被配成 0（属性面板 ClampMin=0），先挡一下除零
	if (LifeBarWidget)
	{
		LifeBarWidget->SetLifePercentage(MaxHP > 0.0f ? CurrentHP / MaxHP : 0.0f);
	}

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

	// 死了就别再显示血条
	if (LifeBar)
	{
		LifeBar->SetHiddenInGame(true);
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

	// 从组件里取出蓝图子类的控件实例（具体是哪个蓝图，由蓝图面板的 Widget Class 决定）
	LifeBarWidget = Cast<UCombatLifeBar>(LifeBar ? LifeBar->GetUserWidgetObject() : nullptr);

	if (LifeBarWidget)
	{
		LifeBarWidget->SetLifePercentage(1.0f);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Dummy] 血条未就绪：检查 BP_CombatDummy 上 LifeBar 组件的 Widget Class 是否设为 UI_LifeBar"));
	}
}

