// 练习角色 Dummy —— 第一步：只建立角色这个"挂载点"
// 攻击动作（动画蒙太奇 + 输入绑定）在第二步接入，这里先留好接口。

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Animation/AnimMontage.h"
#include "DummyCharacter.generated.h"

UCLASS()
class LOGSKYLINE_API ADummyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ADummyCharacter();

protected:
	virtual void BeginPlay() override;

public:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// 攻击用的动画蒙太奇：在蓝图子类里指定（例如 AM_ComboAttack）
	// 有 UPROPERTY 才会被编辑器看到、被序列化、被 GC 正确追踪
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dummy|Combat")
	UAnimMontage* AttackMontage;

	// 攻击入口：蓝图或输入绑定都可以调用它
	UFUNCTION(BlueprintCallable, Category = "Dummy|Combat")
	void Attack();
};
