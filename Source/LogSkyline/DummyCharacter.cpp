#include "DummyCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

ADummyCharacter::ADummyCharacter()
{
	// 目前不需要每帧逻辑，关掉 Tick：能用事件/定时器驱动的就别每帧跑
	PrimaryActorTick.bCanEverTick = false;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->MaxWalkSpeed = 400.f;

	// 骨骼网格体（SKM_Manny）和动画蓝图（ABP_Manny）在蓝图子类的细节面板里指定，
	// C++ 不直接引用 Content 资源，这是 UE 的惯例。
	// 注意：没有 AnimBlueprint 时 GetAnimInstance() 为空，蒙太奇播不出来。
}

void ADummyCharacter::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Warning, TEXT("Dummy 已生成：%s"), *GetName());
}

void ADummyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	// 第二步：在这里把攻击输入（IA_Attack）绑定到 Attack()
}

void ADummyCharacter::Attack()
{
	if (IsValid(AttackMontage))
	{
		PlayAnimMontage(AttackMontage);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Dummy: 还没指定 AttackMontage"));
	}
}
