// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "CombatAttacker.h"
#include "CombatDamageable.h"
#include "LogSkylineCharacter.generated.h"

// 前置声明：头文件里只用到这些类型的指针，不需要完整定义，避免 include 扩散
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UAnimMontage;
class UCombatLifeBar;
struct FInputActionValue;

/**
 * 连招里单段的配置。
 * 数组下标 = 第几段；SectionName 指向蒙太奇里的段落，Damage 是这一段打出的伤害。
 */
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
 *  可被玩家操控的第三人称角色。
 *  自带环绕相机、空手 / 持剑两套连招、受击与血量。
 *
 *  本文件的排版约定（加新成员时照此归位）：
 *    private   → 纯内部组件，蓝图只需要只读
 *    protected → 配置项、运行时状态、内部函数（蓝图子类等价于 C++ 派生类，所以蓝图能访问）
 *    public    → 构造函数、输入 API、接口实现、调试命令
 *  每个访问块内部再按功能分小节，小节之间用分隔横幅隔开。
 */
UCLASS(abstract)
class ALogSkylineCharacter : public ACharacter, public ICombatAttacker, public ICombatDamageable
{
	GENERATED_BODY()

private:

	// ========================================================================
	//  组件
	// ========================================================================

	/** 弹簧臂：把相机吊在角色身后，后方有墙时自动缩短臂长 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** 跟随相机 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

protected:

	// ========================================================================
	//  一、输入
	// ========================================================================

	/** 跳跃 */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** 移动 */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** 视角（键盘 / 手柄右摇杆） */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** 视角（鼠标） */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	/** 普攻 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Input")
	UInputAction* AttackAction;

	// ========================================================================
	//  二、战斗 · 动画资源
	// ========================================================================

	/** 空手连招蒙太奇。三段 Section：Melee01 / Melee02 / Melee03 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation")
	UAnimMontage* AttackMontage;

	/** 持剑三段刺击蒙太奇。Section 名与 AttackMontage 相同（Melee01/02/03），
	 *  这样跳段逻辑两套共用，区别只在“播哪一个” */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation")
	UAnimMontage* SwordComboMontage;

	// ========================================================================
	//  三、战斗 · 连击配置
	// ========================================================================

	/** 连击窗口：上一段快播完时按下的攻击，多久之内算“接下一段” */
	UPROPERTY(EditAnywhere, Category = "Combat|Combo", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float ComboInputCacheTimeTolerance = 0.45f;

	/** 空手连招配置：Section 名 + 每段伤害（1 / 1 / 2） */
	UPROPERTY(EditAnywhere, Category = "Combat|Combo")
	TArray<FComboStepInfo> ComboSteps = {
		{ FName(TEXT("Melee01")), 1.0f },
		{ FName(TEXT("Melee02")), 1.0f },
		{ FName(TEXT("Melee03")), 2.0f }
	};

	/** 持剑连招配置：Section 名 + 每段伤害（1 / 1 / 2） */
	UPROPERTY(EditAnywhere, Category = "Combat|Combo")
	TArray<FComboStepInfo> SwordComboSteps = {
		{ FName(TEXT("Melee01")), 1.0f },
		{ FName(TEXT("Melee02")), 1.0f },
		{ FName(TEXT("Melee03")), 2.0f }
	};

	// ========================================================================
	//  四、战斗 · 运行时状态
	// ========================================================================

	/** 是否正在播攻击蒙太奇：用来忽略连按，防止打断当前动作（普通成员，不需要 UPROPERTY） */
	bool bIsAttacking = false;

	/** 攻击中那次按键发生的游戏时刻，供 CheckCombo 回看是否落在窗口内 */
	float CachedAttackInputTime = 0.0f;

	/** 当前连击段索引，纯运行时状态，不暴露 */
	int32 ComboCount = 0;

	/** 当前是否持剑。决定攻击时用哪套蒙太奇、哪套连击配置 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Weapon", meta = (AllowPrivateAccess = "true"))
	bool bIsArmed = false;

	/** 本次攻击实际播放的蒙太奇。DoAttack 开始时锁定，
	 *  后续跳段 / 伤害查询 / 结束回调全以它为准，避免中途状态变化串到另一套 */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveComboMontage;

	// ========================================================================
	//  五、近战判定参数
	// ========================================================================

	/** 从手骨往前扫多远 */
	UPROPERTY(EditAnywhere, Category = "Combat|Melee", meta = (ClampMin = 0, ClampMax = 500, Units = "cm"))
	float MeleeTraceDistance = 75.0f;

	/** 扫掠球的半径，决定“判定有多宽” */
	UPROPERTY(EditAnywhere, Category = "Combat|Melee", meta = (ClampMin = 0, ClampMax = 200, Units = "cm"))
	float MeleeTraceRadius = 75.0f;

	/** 兜底伤害：连击段索引越界时使用 */
	UPROPERTY(EditAnywhere, Category = "Combat|Melee", meta = (ClampMin = 0, ClampMax = 100))
	float MeleeDamage = 1.0f;

	/** 水平击退力度 */
	UPROPERTY(EditAnywhere, Category = "Combat|Melee", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm/s"))
	float MeleeKnockbackImpulse = 250.0f;

	/** 上抛力度 */
	UPROPERTY(EditAnywhere, Category = "Combat|Melee", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm/s"))
	float MeleeLaunchImpulse = 300.0f;

	// ========================================================================
	//  六、血量与 HUD
	// ========================================================================

	/** 最大血量。和模板 ACombatCharacter 同款，在角色蓝图里配 */
	UPROPERTY(EditAnywhere, Category = "Damage", meta = (ClampMin = 0, ClampMax = 100))
	float MaxHP = 5.0f;

	/** 当前血量。运行时由 TakeDamage 修改；蓝图只读，方便 HUD 做低血提示 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage", meta = (AllowPrivateAccess = "true"))
	float CurrentHP = 0.0f;

	/** 屏幕 HUD 血条的控件类。必须是 UCombatLifeBar 的子类，在角色蓝图的类默认值里指定 */
	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	TSubclassOf<UCombatLifeBar> HUDWidgetClass;

	/** BeginPlay 里创建出来的 HUD 实例。只在运行时存在，不存进资产 */
	UPROPERTY(Transient)
	TObjectPtr<UCombatLifeBar> HUDWidget;

	// ========================================================================
	//  七、生命周期与输入转发
	// ========================================================================

	/** 绑定输入动作 */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** 初始化血量，并在本地玩家身上创建 HUD 血条 */
	virtual void BeginPlay() override;

	/** 移动输入的原始回调：把输入值解成二维向量后转交 DoMove */
	void Move(const FInputActionValue& Value);

	/** 视角输入的原始回调：把输入值解成二维向量后转交 DoLook */
	void Look(const FInputActionValue& Value);

	// ========================================================================
	//  八、战斗逻辑
	// ========================================================================

	/** 攻击输入：按当前武器状态选蒙太奇并播放。蓝图 / UI 也能直接调用 */
	UFUNCTION(BlueprintCallable, Category="Combat")
	void DoAttack();

	/** 攻击开始时交给蓝图做表现（音效 / 特效）。C++ 只负责触发，不写实现 */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat", meta=(DisplayName="On Attack Started"))
	void BP_OnAttackStarted();

	/** 连击推进到下一段时通知蓝图 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat", meta = (DisplayName = "On Combo Step"))
	void BP_OnComboStep(int32 ComboStep);

	/** 按当前连击段取伤害 */
	float GetDamageForCurrentComboStep() const;

	/** 按当前武器状态选出这次该播的蒙太奇。持剑但没配蒙太奇时返回 nullptr 并打警告 */
	UAnimMontage* SelectComboMontage() const;

	/** 取出与 ActiveComboMontage 配套的连击配置，保证蒙太奇和伤害表不会错配 */
	const TArray<FComboStepInfo>& GetActiveComboSteps() const;

	// ========================================================================
	//  九、死亡表现
	// ========================================================================

	/** 死亡时交给蓝图做表现（倒地动画 / 死亡音效 / Game Over）。C++ 只负责状态，表现留给蓝图 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat", meta = (DisplayName = "On Death"))
	void BP_OnDeath();

public:

	// ========================================================================
	//  构造函数
	// ========================================================================

	ALogSkylineCharacter();

	// ========================================================================
	//  输入 API（玩家输入与 UI 共用同一组入口）
	// ========================================================================

	/** 处理移动：Right 为左右、Forward 为前后 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** 处理视角：Yaw 为水平偏航、Pitch 为俯仰 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** 起跳 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** 停止跳跃（松开跳跃键时调用，决定最终能跳多高） */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	// ========================================================================
	//  相机访问器
	// ========================================================================

	/** 取弹簧臂组件 */
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** 取跟随相机组件 */
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	// ========================================================================
	//  ICombatAttacker 接口实现（由动画通知调用）
	// ========================================================================

	/** 在指定骨骼处做一次近战扫掠判定 */
	virtual void DoAttackTrace(FName DamageSourceBone) override;

	/** 连击检查：回看缓存窗口，决定是否跳到下一段 */
	virtual void CheckCombo() override;

	/** 蓄力攻击检查（当前未使用） */
	virtual void CheckChargedAttack() override;

	// ========================================================================
	//  ICombatDamageable 接口实现 + 引擎标准伤害入口
	// ========================================================================

	/** 受击入口之一：转发给 TakeDamage，并在成功后施加击退 */
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;

	/** 血量归零时的处理：禁用移动、收起血条、通知蓝图 */
	virtual void HandleDeath() override;

	/** 治疗（当前未实现） */
	virtual void ApplyHealing(float Healing, AActor* Healer) override;

	/** 受击预警（当前未实现） */
	virtual void NotifyDanger(const FVector& DangerLocation, AActor* DangerSource) override;

	/** 引擎标准伤害入口。ApplyDamage 和 GameplayStatics::ApplyDamage 最终都汇总到这里 */
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	// ========================================================================
	//  调试命令（只在 PIE 控制台里有效）
	// ========================================================================

	/** 【临时测试】PIE 控制台输入 DamageSelf 1 给自己扣血，用来验证 HUD。验证完删掉 */
	UFUNCTION(Exec)
	void DamageSelf(float Amount = 1.0f);

	/** 【临时调试】PIE 控制台输入 ToggleArmed 切换持剑状态。
	 *  以后接了“按 1 拔剑”的输入，直接复用这个开关即可 */
	UFUNCTION(Exec)
	void ToggleArmed();

};
