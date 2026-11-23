// Copyright Epic Games, Inc. All Rights Reserved.

#include "LogSkylineCharacter.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "LogSkyline.h"
#include "Components/SkeletalMeshComponent.h"
#include "CombatDamageable.h"
#include "Animation/AnimInstance.h"


ALogSkylineCharacter::ALogSkylineCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->CrouchedHalfHeight = 40.0f;
	GetCharacterMovement()->MaxWalkSpeedCrouched = 150.0f;


	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void ALogSkylineCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ALogSkylineCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ALogSkylineCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ALogSkylineCharacter::Look);

		// Attacking
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &ALogSkylineCharacter::DoAttack);

	}
	else
	{
		UE_LOG(LogLogSkyline, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ALogSkylineCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void ALogSkylineCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void ALogSkylineCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void ALogSkylineCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ALogSkylineCharacter::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

void ALogSkylineCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}

void ALogSkylineCharacter::DoAttack()
{
	// 1) 状态守卫：上一次攻击还没结束，直接忽略这次输入
	if (bIsAttacking)
	{
		CachedAttackInputTime = GetWorld()->GetTimeSeconds();
		return;
	}

	// 2) 资源守卫：IsValid 同时挡住空指针和 pending-kill 对象
	if (!IsValid(AttackMontage))
	{
		UE_LOG(LogLogSkyline, Warning, TEXT("Dummy: AttackMontage 未指定"));
		return;
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!IsValid(AnimInstance))
	{
		return;
	}

	// 3) Montage_Play 返回实际播放时长；返回 0 表示播放失败
	const float Duration = AnimInstance->Montage_Play(AttackMontage);
	if (Duration <= 0.f)
	{
		return;
	}

	bIsAttacking = true;
	ComboCount = 0;

	// 配置自检：数组少于两项时，连招只可能停在第一段，提前在日志里点名，避免静默失效
	if (ComboSteps.Num() < 2)
	{
		UE_LOG(LogLogSkyline, Warning, TEXT("[Combo] ComboSteps 只有 %d 项，缺少后续 Section，连招会退化为单段"), ComboSteps.Num());
	}

	// 4) 表现交给蓝图：C++ 只触发事件，蓝图实现音效 / 特效（未实现时为空操作）
	BP_OnAttackStarted();

	// 5) 蒙太奇播完或被中断时自动解锁，不用 Tick、不用定时器
	FOnMontageEnded EndDelegate;
	EndDelegate.BindLambda([this](UAnimMontage*, bool)
	{
		bIsAttacking = false;
	});
	AnimInstance->Montage_SetEndDelegate(EndDelegate, AttackMontage);
}

void ALogSkylineCharacter::DoAttackTrace(FName DamageSourceBone)
{
	// ① 起点：通知指定的骨骼在世界空间的位置（就是刚才日志里的 hand_r）
	const FVector TraceStart = GetMesh()->GetSocketLocation(DamageSourceBone);

	// ② 终点：从起点沿角色正前方伸出 MeleeTraceDistance
	const FVector TraceEnd = TraceStart + GetActorForwardVector() * MeleeTraceDistance;

	// ③ 判定形状：一个半径 MeleeTraceRadius 的球，从起点扫到终点
	FCollisionShape Sphere;
	Sphere.SetSphere(MeleeTraceRadius);

	// ④ 只关心两类东西：角色(Pawn) 和 会动的物理物体(WorldDynamic)
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	// ⑤ 别扫到自己
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	// ⑥ 扫掠，一次可能撞到多个物体，所以用数组接
	TArray<FHitResult> Hits;
	const bool bHit = GetWorld()->SweepMultiByObjectType(
		Hits, TraceStart, TraceEnd, FQuat::Identity, ObjectParams, Sphere, QueryParams);

	if (!bHit)
	{
		UE_LOG(LogLogSkyline, Log, TEXT("打击: 没扫到任何东西"));
		return;
	}

	// 同一个 Actor 可能有多个碰撞体，扫掠会给每个都返回一条命中；
// 记一下已经打过的，避免一次挥砍造成多份伤害、冲量翻倍
	TSet<AActor*> AlreadyHit;

	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!IsValid(HitActor) || AlreadyHit.Contains(HitActor))
		{
			continue;
		}

		// 只有实现了伤害接口的对象才会挨打
		if (ICombatDamageable* Damageable = Cast<ICombatDamageable>(HitActor))
		{
			AlreadyHit.Add(HitActor);

			// 冲量：沿碰撞法线反向推离自己，再叠加一个向上的分量
			const FVector Impulse = (Hit.ImpactNormal * -MeleeKnockbackImpulse)
				+ (FVector::UpVector * MeleeLaunchImpulse);

			// 按当前连击段取伤害：第 1/2 段 1 点，第 3 段 2 点
			const float StepDamage = GetDamageForCurrentComboStep();
			Damageable->ApplyDamage(StepDamage, this, Hit.ImpactPoint, Impulse);

			UE_LOG(LogLogSkyline, Log, TEXT("打击: %s 受到 %.1f 伤害"), *GetNameSafe(HitActor), StepDamage);
		}
	}

}


void ALogSkylineCharacter::CheckCombo()
{
	if (bIsAttacking)
	{
		if (GetWorld()->GetTimeSeconds() - CachedAttackInputTime <= ComboInputCacheTimeTolerance)
		{
			CachedAttackInputTime = 0.0f;   // 消费掉，避免同一下被用两次
			++ComboCount;

			if (ComboCount < ComboSteps.Num())
			{
				const FName NextSection = ComboSteps[ComboCount].SectionName;
				UE_LOG(LogLogSkyline, Log, TEXT("Combo step %d -> %s"), ComboCount, *NextSection.ToString());

				if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
				{
					AnimInstance->Montage_JumpToSection(NextSection, AttackMontage);
					BP_OnComboStep(ComboCount);
				}
			}
		}
	}
}

void ALogSkylineCharacter::CheckChargedAttack()
{
	// 蓄力攻击暂不使用
}


float ALogSkylineCharacter::GetDamageForCurrentComboStep() const
{
	// 正常情况下 ComboCount 就是当前段的索引
	if (ComboSteps.IsValidIndex(ComboCount))
	{
		return ComboSteps[ComboCount].Damage;
	}

	// 走不到这里才对：连击打完后 ComboCount 会到 3，数组只有三格
	return MeleeDamage;
}



