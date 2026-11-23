// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "CombatAttacker.h"
#include "CombatDamageable.h"
#include "LogSkylineCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UAnimMontage;
class UCombatLifeBar;
struct FInputActionValue;

USTRUCT(BlueprintType)
struct FComboStepInfo
{
	GENERATED_BODY()

	/** 蒙太奇里的 Section 名。第 0 项是起手段，不参与跳段 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo")
	FName SectionName;

	/** 这一段的伤害 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo", meta = (ClampMin = 0, ClampMax = 100))
	float Damage = 1.0f;
};


DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class ALogSkylineCharacter : public ACharacter, public ICombatAttacker, public ICombatDamageable
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Input")
	UInputAction* AttackAction;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation")
	UAnimMontage* AttackMontage;


	bool bIsAttacking = false; //是否正在播放攻击蒙太奇，防止连按打断当前动作（普通 C++ 成员，不需要 UPROPERTY）
	float CachedAttackInputTime = 0.0f;   // 攻击中那次按键发生的游戏时刻

	UPROPERTY(EditAnywhere, Category = "Combat|Combo", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float ComboInputCacheTimeTolerance = 0.45f;   // 连击窗口：按键多久内算有效
	int32 ComboCount = 0;                          // 运行时状态，留 C++ 不暴露

	UPROPERTY(EditAnywhere, Category = "Combat|Combo")
	TArray<FComboStepInfo> ComboSteps = {
		{ FName(TEXT("Melee01")), 1.0f },
		{ FName(TEXT("Melee02")), 1.0f },
		{ FName(TEXT("Melee03")), 2.0f }
	};

	UPROPERTY(EditAnywhere, Category = "Combat|Melee", meta = (ClampMin = 0, ClampMax = 500, Units = "cm"))
	float MeleeTraceDistance = 75.0f;      // 从手往前扫多远

	UPROPERTY(EditAnywhere, Category = "Combat|Melee", meta = (ClampMin = 0, ClampMax = 200, Units = "cm"))
	float MeleeTraceRadius = 75.0f;        // 球的半径，决定"判定有多宽"

	UPROPERTY(EditAnywhere, Category = "Combat|Melee", meta = (ClampMin = 0, ClampMax = 100))
	float MeleeDamage = 1.0f;              // 伤害值

	UPROPERTY(EditAnywhere, Category = "Combat|Melee", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm/s"))
	float MeleeKnockbackImpulse = 250.0f;  // 水平击退力度

	UPROPERTY(EditAnywhere, Category = "Combat|Melee", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm/s"))
	float MeleeLaunchImpulse = 300.0f;     // 上抛力度

	/** 最大血量。和模板 ACombatCharacter 同款，在角色蓝图里配 */
	UPROPERTY(EditAnywhere, Category = "Damage", meta = (ClampMin = 0, ClampMax = 100))
	float MaxHP = 5.0f;

	/** 当前血量。运行时由 TakeDamage 修改；蓝图只读，方便 HUD 做低血提示 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage", meta = (AllowPrivateAccess = "true"))
	float CurrentHP = 0.0f;

	/** 屏幕 HUD 血条的控件类。必须是 UCombatLifeBar 的子类，在角色蓝图的 Class Defaults 里指定 */
	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	TSubclassOf<UCombatLifeBar> HUDWidgetClass;

	/** BeginPlay 里创建出来的 HUD 实例。只在运行时存在，不存进资产 */
	UPROPERTY(Transient)
	TObjectPtr<UCombatLifeBar> HUDWidget;


public:

	/** Constructor */
	ALogSkylineCharacter();	

protected:

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** 初始化血量，并在本地玩家身上创建 HUD 血条 */
	virtual void BeginPlay() override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Called when the attack input is pressed. Blueprint / UI 也能直接调用 */
	UFUNCTION(BlueprintCallable, Category="Combat")
	void DoAttack();

	/** 攻击开始时交给蓝图做表现（音效 / 特效）。C++ 只负责触发，不写实现 */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat", meta=(DisplayName="On Attack Started"))
	void BP_OnAttackStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Combat", meta = (DisplayName = "On Combo Step"))
	void BP_OnComboStep(int32 ComboStep);

	float GetDamageForCurrentComboStep() const;


public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

public:
	// ~begin CombatAttacker interface
	virtual void DoAttackTrace(FName DamageSourceBone) override;
	virtual void CheckCombo() override;
	virtual void CheckChargedAttack() override;
	// ~end CombatAttacker interface

public:
	// ~begin CombatDamageable interface
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;
	virtual void HandleDeath() override;
	virtual void ApplyHealing(float Healing, AActor* Healer) override;
	virtual void NotifyDanger(const FVector& DangerLocation, AActor* DangerSource) override;
	// ~end CombatDamageable interface

	/** 死亡时交给蓝图做表现（倒地动画 / 死亡音效 / Game Over）。C++ 只负责状态，表现留给蓝图 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat", meta = (DisplayName = "On Death"))
	void BP_OnDeath();

	/** 引擎标准伤害入口。ApplyDamage 和 GameplayStatics::ApplyDamage 最终都汇总到这里 */
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** 【临时测试】PIE 控制台输入 DamageSelf 1 给自己扣血，用来验证 HUD。验证完删掉 */
	UFUNCTION(Exec)
	void DamageSelf(float Amount = 1.0f);

};

