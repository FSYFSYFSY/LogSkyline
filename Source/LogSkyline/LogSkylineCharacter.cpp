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
#include "Components/StaticMeshComponent.h"
#include "CombatDamageable.h"
#include "Animation/AnimInstance.h"
#include "Blueprint/UserWidget.h"
#include "CombatLifeBar.h"
#include "Engine/DamageEvents.h"


// ============================================================================
//  一、构造与生命周期
// ============================================================================

ALogSkylineCharacter::ALogSkylineCharacter()
{
	// 胶囊体尺寸：半径 42、半高 96，第三人称角色的标准值
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// 控制器旋转不带动角色本体，只影响相机（否则转视角时角色会跟着扭）
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// 角色移动配置：朝移动方向转身，允许蹲下，转身速度 500 度/秒
	GetCharacterMovement()->bOrientRotationToMovement = true;
	// 蹲下开关。项目统一用 GetNavAgentPropertiesRef() 这种写法，直接访问 NavAgentProps 会编译不过
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// 提示：下面这些移动参数在角色蓝图里也能改，调手感不必重新编译
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->CrouchedHalfHeight = 40.0f;
	GetCharacterMovement()->MaxWalkSpeedCrouched = 150.0f;


	// 创建弹簧臂：相机吊在身后 400cm，后方有障碍时自动拉近
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// 创建跟随相机，挂在弹簧臂末端
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// 剑：初始挂到骨架的 weapon_back socket 上（就是 spine_03 上那个“剑鞘”挂点）。
	// socket 名写错不会报错，剑会静默落到 Mesh 原点（角色脚下）——看到剑在脚底先回来查名字
	SwordMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwordMesh"));
	SwordMesh->SetupAttachment(GetMesh(), TEXT("weapon_back"));

	// 剑不参与碰撞：它比胶囊体长得多，开碰撞会卡住角色自己的移动、贴着墙把自己顶出去，
	// 还可能被近战扫掠当成敌人命中。打击判定只靠手骨扫掠，不需要剑身碰撞
	SwordMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SwordMesh->SetGenerateOverlapEvents(false);

	// 提示：骨骼网格体和动画蓝图在派生的角色蓝图里指定，避免 C++ 里硬引用资产
}

void ALogSkylineCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 从满血开始
	CurrentHP = MaxHP;

	// 只有本地玩家才建 HUD：联机时每个客户端都有自己的 PlayerController，但只该建自己那份
	if (!IsLocallyControlled())
	{
		return;
	}

	if (!HUDWidgetClass)
	{
		UE_LOG(LogLogSkyline, Warning, TEXT("[Player] 未配置 HUDWidgetClass，屏幕上看不到血条。去 BP_MyplayerCharacter 的 Class Defaults → Damage → HUD Widget Class 选一个继承 CombatLifeBar 的控件"));
		return;
	}

	HUDWidget = CreateWidget<UCombatLifeBar>(GetWorld(), HUDWidgetClass);

	if (HUDWidget)
	{
		HUDWidget->AddToViewport();
		HUDWidget->SetLifePercentage(1.0f);
	}
}


// ============================================================================
//  二、输入
// ============================================================================

void ALogSkylineCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// 绑定输入动作
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {

		// 跳跃：按下起跳，松开停跳
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// 移动
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ALogSkylineCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ALogSkylineCharacter::Look);

		// 视角（键盘 / 手柄）
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ALogSkylineCharacter::Look);

		// 攻击：按下即触发一次，不需要持续按住
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &ALogSkylineCharacter::DoAttack);

	}
	else
	{
		UE_LOG(LogLogSkyline, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ALogSkylineCharacter::Move(const FInputActionValue& Value)
{
	// 输入是二维向量：X = 左右，Y = 前后
	FVector2D MovementVector = Value.Get<FVector2D>();

	// 转交给公共入口，UI 调用走的也是同一个函数
	DoMove(MovementVector.X, MovementVector.Y);
}

void ALogSkylineCharacter::Look(const FInputActionValue& Value)
{
	// 输入是二维向量：X = 水平偏航，Y = 俯仰
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// 转交给公共入口
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void ALogSkylineCharacter::DoMove(float Right, float Forward)
{
	// 攻击期间不接受移动输入。空手的动画自带根运动，引擎本来就会丢弃输入；
	// 刺击没有根运动，只能在这里挡住，否则会出现“边刺边飘”
	if (bIsAttacking)
	{
		return;
	}

	if (GetController() != nullptr)
	{
		// 取控制器朝向，只保留水平分量（Yaw），否则抬头时移动会带向空中
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// 前方向
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// 右方向
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// 叠加到移动输入
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void ALogSkylineCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// 把偏航 / 俯仰交给控制器
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ALogSkylineCharacter::DoJumpStart()
{
	// 触发跳跃
	Jump();
}

void ALogSkylineCharacter::DoJumpEnd()
{
	// 通知引擎停止跳跃
	StopJumping();
}


// ============================================================================
//  三、攻击与连击
// ============================================================================

void ALogSkylineCharacter::DoAttack()
{
	// 1) 状态守卫：上一次攻击还没结束，就只记下按键时刻，等 CheckCombo 回看
	if (bIsAttacking)
	{
		CachedAttackInputTime = GetWorld()->GetTimeSeconds();
		return;
	}

	// 2) 资源守卫：按当前武器状态选出这次该播的蒙太奇
	UAnimMontage* MontageToPlay = SelectComboMontage();
	if (!IsValid(MontageToPlay))
	{
		UE_LOG(LogLogSkyline, Warning, TEXT("[Combo] 当前状态下没有可用的攻击蒙太奇（持剑=%s）"),
			bIsArmed ? TEXT("是") : TEXT("否"));
		return;
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!IsValid(AnimInstance))
	{
		return;
	}

	// 3) Montage_Play 返回实际播放时长；返回 0 表示播放失败
	const float Duration = AnimInstance->Montage_Play(MontageToPlay);
	if (Duration <= 0.f)
	{
		return;
	}

	bIsAttacking = true;
	ComboCount = 0;
	ActiveComboMontage = MontageToPlay;   // 锁定本次使用的那一套，后续跳段 / 伤害都以它为准

	// 配置自检：数组少于两项时，连招只可能停在第一段，提前在日志里点名，避免静默失效
	if (GetActiveComboSteps().Num() < 2)
	{
		UE_LOG(LogLogSkyline, Warning, TEXT("[Combo] 当前连击配置只有 %d 项，缺少后续 Section，连招会退化为单段"), GetActiveComboSteps().Num());
	}

	// 4) 表现交给蓝图：C++ 只触发事件，蓝图实现音效 / 特效（未实现时为空操作）
	BP_OnAttackStarted();

	// 5) 蒙太奇播完或被中断时自动解锁，不用 Tick、不用定时器
	FOnMontageEnded EndDelegate;
	EndDelegate.BindLambda([this](UAnimMontage*, bool)
	{
		bIsAttacking = false;
	});
	AnimInstance->Montage_SetEndDelegate(EndDelegate, MontageToPlay);
}

UAnimMontage* ALogSkylineCharacter::SelectComboMontage() const
{
	// 持剑就用刺击那套；没配蒙太奇时明确报错，不要悄悄退回空手（否则配置遗漏很难发现）
	if (bIsArmed)
	{
		if (!IsValid(SwordComboMontage))
		{
			UE_LOG(LogLogSkyline, Warning, TEXT("[Combo] 已持剑但 SwordComboMontage 未指定，请在角色蓝图里指定 AM_SwordCombo"));
			return nullptr;
		}
		return SwordComboMontage;
	}

	return AttackMontage;
}

const TArray<FComboStepInfo>& ALogSkylineCharacter::GetActiveComboSteps() const
{
	// 只有“确实在播持剑蒙太奇”时才用持剑的伤害表，避免蒙太奇和配置错配
	if (IsValid(SwordComboMontage) && ActiveComboMontage == SwordComboMontage)
	{
		return SwordComboSteps;
	}

	return ComboSteps;
}

void ALogSkylineCharacter::CheckCombo()
{
	if (bIsAttacking)
	{
		// 回看：这次通知触发前的“窗口时长”内，有没有按下过攻击
		if (GetWorld()->GetTimeSeconds() - CachedAttackInputTime <= ComboInputCacheTimeTolerance)
		{
			CachedAttackInputTime = 0.0f;   // 消费掉，避免同一次按键被用两次
			++ComboCount;

			const TArray<FComboStepInfo>& Steps = GetActiveComboSteps();
			if (ComboCount < Steps.Num())
			{
				const FName NextSection = Steps[ComboCount].SectionName;
				UE_LOG(LogLogSkyline, Log, TEXT("Combo step %d -> %s (持剑=%s)"), ComboCount, *NextSection.ToString(),
					bIsArmed ? TEXT("是") : TEXT("否"));

				if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
				{
					// 必须用本次实际在播的那个蒙太奇跳段：传错对象会静默失败，第二刀永远出不来
					AnimInstance->Montage_JumpToSection(NextSection, ActiveComboMontage);
					BP_OnComboStep(ComboCount);
				}
			}
		}
	}
}

float ALogSkylineCharacter::GetDamageForCurrentComboStep() const
{
	// 取本次攻击实际使用的那套配置（空手与持剑都是 1 / 1 / 2）
	const TArray<FComboStepInfo>& Steps = GetActiveComboSteps();

	// 正常情况下 ComboCount 就是当前段的索引
	if (Steps.IsValidIndex(ComboCount))
	{
		return Steps[ComboCount].Damage;
	}

	// 走不到这里才对：连击打完后 ComboCount 会等于段数，而数组下标最大是段数 - 1
	return MeleeDamage;
}


// ============================================================================
//  四、近战判定（ICombatAttacker 接口实现）
// ============================================================================

void ALogSkylineCharacter::DoAttackTrace(FName DamageSourceBone)
{
	// ① 起点：通知指定的骨骼在世界空间的位置（就是蒙太奇里填的 hand_r）
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

			// 按当前连击段取伤害：第 1/2 段 1 点，第 3 段 2 点（空手与持剑相同）
			const float StepDamage = GetDamageForCurrentComboStep();
			Damageable->ApplyDamage(StepDamage, this, Hit.ImpactPoint, Impulse);

			UE_LOG(LogLogSkyline, Log, TEXT("打击: %s 受到 %.1f 伤害"), *GetNameSafe(HitActor), StepDamage);
		}
	}

}

void ALogSkylineCharacter::CheckChargedAttack()
{
	// 蓄力攻击暂不使用
}


// ============================================================================
//  五、受击与死亡（ICombatDamageable 接口实现 + 引擎标准伤害入口）
// ============================================================================

float ALogSkylineCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// 已经死了就不再处理，避免尸体继续掉血、重复触发 HandleDeath
	if (CurrentHP <= 0.0f)
	{
		return 0.0f;
	}

	// 交给父类：它会按伤害类型做修正，并把 OnTakeAnyDamage 委托广播出去（蓝图里能接）
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (ActualDamage <= 0.0f)
	{
		return 0.0f;
	}

	CurrentHP = FMath::Max(0.0f, CurrentHP - ActualDamage);

	// 血条是 HP 的镜像：HP 变一次就同步一次，这是唯一出口
	if (HUDWidget)
	{
		HUDWidget->SetLifePercentage(MaxHP > 0.0f ? CurrentHP / MaxHP : 0.0f);
	}

	UE_LOG(LogLogSkyline, Log, TEXT("[Player] HP %.0f / %.0f"), CurrentHP, MaxHP);

	if (CurrentHP <= 0.0f)
	{
		HandleDeath();
	}

	return ActualDamage;
}

void ALogSkylineCharacter::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	// 走引擎标准伤害管线：扣血、更新血条、判定死亡全在 TakeDamage 里统一处理，
	// 这样以后别人用 UGameplayStatics::ApplyDamage 打玩家也是同一条路径
	FDamageEvent DamageEvent;
	const float ActualDamage = TakeDamage(Damage, DamageEvent, nullptr, DamageCauser);

	if (ActualDamage <= 0.0f)
	{
		return;
	}

	// 受击击退。和模板 ACombatCharacter 一致：往 CharacterMovement 上直接加冲量
	GetCharacterMovement()->AddImpulse(DamageImpulse, true);
}

void ALogSkylineCharacter::HandleDeath()
{
	// 死后不能再移动
	GetCharacterMovement()->DisableMovement();

	// 血条从屏幕上撤掉（保留指针，以后做重生只要重新设回可见）
	if (HUDWidget)
	{
		HUDWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	UE_LOG(LogLogSkyline, Log, TEXT("[Player] 已死亡"));

	// 状态改完了，把表现交给蓝图（蓝图没实现就是个空实现，不会崩）
	BP_OnDeath();
}

void ALogSkylineCharacter::ApplyHealing(float Healing, AActor* Healer)
{
	// 暂不实现治疗
}

void ALogSkylineCharacter::NotifyDanger(const FVector& DangerLocation, AActor* DangerSource)
{
	// 暂不实现受击预警
}


// ============================================================================
//  六、调试命令（只在 PIE 控制台里有效）
// ============================================================================

void ALogSkylineCharacter::DamageSelf(float Amount)
{
	// 【临时测试】走和真实受击完全相同的链路，用来验证 HUD
	ApplyDamage(Amount, this, GetActorLocation() + FVector(0.0f, 0.0f, 50.0f), FVector::ZeroVector);
}

void ALogSkylineCharacter::ToggleArmed()
{
	// 【临时调试】切换持剑状态，用来在没做拔剑输入前验证两套连招
	bIsArmed = !bIsArmed;
	UE_LOG(LogLogSkyline, Log, TEXT("[Weapon] 持剑状态 = %s"), bIsArmed ? TEXT("是") : TEXT("否"));
}
