// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "CombatAttacker.h"
#include "LogSkylineCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UAnimMontage;
struct FInputActionValue;


DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class ALogSkylineCharacter : public ACharacter, public ICombatAttacker
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
	TArray<FName> ComboSectionNames = { FName(TEXT("Melee01")), FName(TEXT("Melee02")), FName(TEXT("Melee03")) };
	// 兜底默认值：C++ 给一份可用的，蓝图面板可覆盖；数组的第 0 项不参与跳段（只用 [1]、[2]）


public:

	/** Constructor */
	ALogSkylineCharacter();	

protected:

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

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

};

