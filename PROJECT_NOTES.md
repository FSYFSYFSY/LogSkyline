# LogSkyline 项目交接笔记 / AI Handoff

> **给接手的 AI：请直接读取本文件作为上下文起点**，不需要翻历史对话。
> 更细的逐日排障日志在 `.workbuddy/memory/2026-11-08.md`、`2026-11-14.md`、`2026-11-15.md`。
> 最后更新：2026-11-17 19:40 (GMT+8)

## 0. 用户与协作方式（先读这段）

- 用户是 **C++ 与 Unreal Engine 双初学者**，通过 UE 5.8 第三人称模板学游戏开发，目标是单人 demo（已放弃多人方向）。
- 偏好：**一次只给一步**、先讲思路再动手、用具体执行故事和生活化类比，**避免堆术语**；讲抽象概念后配一个 5 分钟能看到结果的实验。
- 不要一次性大改代码；改动前说明每行作用，改完给可验证的反馈点。用**简体中文**回答。
- 用户会反复追问「为什么」，需要把历史原因与技术原因分开讲清楚，不要只给结论。

## 1. 项目与环境

| 项 | 值 |
|---|---|
| 项目根 | `E:\UEProjects\LogSkyline`（2026-11-17 核实；本文早期记录里的 `D:\LogSkyline\LogSkyline` 已失效，命令请按 E 盘路径读） |
| 引擎 | `E:\UE_5.8`，**手动安装的 Installed Build**（注册表未登记 5.8） |
| 模板 | UE 5.8 Third Person，含 `Variant_Combat` / `Variant_Platforming` / `Variant_SideScrolling` |
| uproject | `LogSkyline.uproject`，`EngineAssociation` 已被 UE 改成 `"5.8"`（已随 `07ec343` 提交） |
| Git | `FSYFSYFSY/LogSkyline`，分支 `FSY_tut`（HEAD 以 `git log -1` 为准；`07ec343` 为攻击重构 + 文档首次入库）；`origin` fetch = 官方 HTTPS、push = SSH；`mirror` = ghproxy 镜像，且为 `FSY_tut` 上游 |
| IDE | VS2026 Community (18.0) |

## 2. 环境坑与速查命令

- **没有** `GenerateProjectFiles.bat`，Windows 右键也没有 "Generate Visual Studio project files"（引擎手动安装、未注册 shell 扩展）。刷新项目文件用：
  ```
  "D:\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" -projectfiles -project="D:\LogSkyline\LogSkyline\LogSkyline.uproject" -game -NoPrograms -progress -NoMutex
  ```
  - `-NoMutex`：VS 开着时 UBT 会报 `ConflictingInstance`，必须加。
  - **`-NoPrograms` 关键**：不加会生成 57 个引擎 C# 项目（共 67 条），VS 打开弹「未能正确加载解决方案中的一个或多个项目」。源码依据 `ProjectFileGenerator.cs`：`-Game` 只关 `bIncludeEnginePrograms`，只有 `-NoPrograms` 同时关掉 `bIncludeDotNetPrograms`。
- 根目录有**两个 .sln**：`LogSkyline.sln`（游戏方案，用这个，2 个 vcxproj）/ `Automation_LogSkyline.sln`（53 个纯引擎 C# 工具，别点）。`Lvl_ThirdPerson` 的 World Settings 覆写 GameMode，覆盖 `DefaultEngine.ini` 的全局值。
- **绝不要用「文件 → 打开 → 文件夹」打开 UE 项目**：会触发 VS2026 Unreal Workspace 缺陷，点任意文件弹「路径的形式不合法」。根因 `UEWorkspaceService..ctor` 对空 `uprojectFilePath` 调 `Path.GetFullPath`；`ConfigurationAndPlatformDropdown` 每次刷新重抛。修法 = 回到 `.sln` 模式。看 VSCode 式目录树请用 VSCode 打开文件夹。
- **反射结构变更必须全量编译**：新增/改动 `.h` 里的 `UCLASS`/`UPROPERTY`/`UFUNCTION`、改类布局 → Live Coding 不支持，需**关 UE 编辑器 → 全量编译 → 重开**。只改 `.cpp` 函数体可用 Live Coding。
- 编译命令：
  ```
  "D:\UE_5.8\Engine\Build\BatchFiles\Build.bat" LogSkylineEditor Win64 Development -Project="D:\LogSkyline\LogSkyline\LogSkyline.uproject" -WaitMutex
  ```
- **排查「属性搜不到 / 节点不出现」先查 DLL**：比对 `Binaries/Win64/UnrealEditor-LogSkyline.dll` 与源码时间戳，并 `grep -a "AttackMontage" <dll>` 确认反射名已进引擎。历史上多次踩这个坑。
- **UBA（Unreal Build Accelerator）权限故障（16:14 实例）**：编译输出大量 `SetFileInformationByHandle (FileDispositionInfo) failed ... (Access is denied)`，伴随 `LNK1136`（.lib 无效）/ `LNK1201`（写 PDB 失败），**且会把 `Binaries/Win64/UnrealEditor-LogSkyline.dll` 删掉、pdb/lib 变成 0 字节**（此时项目无 DLL，编辑器打不开）。根因是 `C:\ProgramData\Epic\UnrealBuildAccelerator` 缓存目录权限异常，与 C++ 代码无关（两个 .cpp 其实都编译通过了）。修法：删除 0 字节的 `UnrealEditor-LogSkyline.pdb` 与 `Intermediate/Build/Win64/x64/UnrealEditor/Development/LogSkyline/UnrealEditor-LogSkyline.lib|.exp`，再用 `Build.bat LogSkylineEditor Win64 Development -Project=... -WaitMutex -NoUBA` 重编（`BuildConfiguration.cs:54` 定义了 `-NoUBA`），增量只需数秒。以后本项目编译建议固定带 `-NoUBA`。
- **VS 里 F7 报 `BuildException: Unable to build while Live Coding is active`（16:03 实例）**：UE 编辑器开着时 Live Coding 持有 `Global\LiveCoding_...` 互斥锁，UBT 直接拒绝全量构建；此时 UHT 可能已跑完（`*.gen.cpp` 已含新 Category / 新事件），但**不链接、DLL 时间戳不变**，编辑器仍加载旧反射数据 → 表现为「分类/节点没变，像没改一样」。**必须完全退出编辑器**（`UnrealEditor.exe` 与 `LiveCodingConsole.exe` 进程都消失）再编译；`-WaitMutex`、重启编辑器、Live Coding 热更都不能替代（反射结构变更本就不支持热更）。UBT 日志在 `%LOCALAPPDATA%\UnrealBuildTool\Log.txt`，关键词 `Live Coding is active` / `Result: Failed`。

## 3. 当前代码状态

- `FSY_tut` 相对 main（`dffd59c`）的主要代码提交（文档维护提交略；完整列表用 `git log --oneline`）：
  - `02724ae` 攻击：C++ 加 `AttackAction` / `AttackMontage` 两个 `UPROPERTY`；`SetupPlayerInputComponent` 里 `BindAction(AttackAction, Started, &DoAttack)`（`.cpp:77`）；`DoAttack()` → `Montage_Play`。
  - `aca568d` 蹲下：C++ 构造函数开 `bCanCrouch`、`CrouchedHalfHeight=40`、`MaxWalkSpeedCrouched=150`；蓝图 `IA_Crouch` 事件节点 → `Crouch()/UnCrouch()`。
  - `07ec343` 攻击逻辑重构：`bIsAttacking` 状态守卫与蒙太奇结束委托、`DoAttack()` 加 `BlueprintCallable`、`BP_OnAttackStarted` 钩子；`DummyCharacter` 与 `PROJECT_NOTES.md` 首次入库。
  - `d49d82e` 三段连击（2026-11-16，已提交并推送）：`LogSkylineCharacter.h/.cpp` 补齐 `ICombatAttacker` 实现与连击跳段，详见 §4「三段连击」节。要点：加 `CachedAttackInputTime` / `ComboInputCacheTimeTolerance` / `ComboCount` / `ComboSectionNames` 四个成员；类继承 `ICombatAttacker` + 三个 `override`（`DoAttackTrace` / `CheckChargedAttack` 留空体）；`CheckCombo()` 在 0.45s 窗口内 `++ComboCount` 并 `Montage_JumpToSection(Melee02/03)`。资产 `AM_MyComboAttack` 未改，三段 Section 与两类 Notify 一直在。已编译 + PIE 验证通过。
  - `044f51f` 连击参数兜底（2026-11-17，已提交推送）：`ComboSectionNames` 保留类内默认值作兜底、`DoAttack()` 增加「数组不足 2 项」自检 Warning。详见 §4「11-17 升级」小节。
  - `d0ff00a` 资产侧修复（2026-11-17，已提交推送）：`ABP_Unarmed` 的 Control Rig `Alpha ← NOT IsSlotActive("DefaultSlot")`（修攻击时下半身不动）；`BP_MyplayerCharacter` 填 `ComboSectionNames`。
  - `2f2f2a6` 分工改造（2026-11-17，已提交）：新增 `BP_OnComboStep(int32)` 蓝图钩子并接入 `CheckCombo()`；蓝图首次实现 `BP_OnAttackStarted` 与 `On Combo Step` 两个事件。详见 §4「11-17 分工改造」小节。已编译（DLL 19:30）+ PIE 验证通过。
  - `d9f055e` 打击判定（2026-11-20，已提交）：`DoAttackTrace()` 球扫 + `Cast<ICombatDamageable>` + `ApplyDamage` + 按 Actor 去重；`.h` 新增 5 个 `Combat|Melee` 参数。详见 §4「第 3 步」。
- 资产：`Content/Myplayer/` 有 `BP_MyplayerCharacter`、`BP_MyplayerGameMode`、`IA_Crouch`、`AM_MyComboAttack`。后者与模板 `Variant_Combat/Anims/AM_ComboAttack` 几乎一致（同 `MM_Attack_01/02/03` + `SK_Mannequin`），内含 `AnimNotify_DoAttackTrace` 与 `AnimNotify_CheckCombo`；`IMC_Default` 已加 `IA_Crouch` + `IA_ComboAttack`。
- 蓝图 `Touch Input for First Person Movement` 是 `EdGraphNode_Comment` 注释框，内容为**移动端触控编排**（`BPI_TouchInterface_C` 虚拟摇杆）：`Touch Jump Start/End`、`CallFunc_DoMove_*`/`DoLook_*`、`DoJumpStart/End`、`Crouch/UnCrouch`。蓝图里唯一的 EnhancedInput 事件节点是 `InpActEvt_IA_Crouch_...`；**PC 键鼠 5 个输入全绑在 C++**。
- 自建练习文件 `Source/LogSkyline/DummyCharacter.h/.cpp`（已随 `07ec343` 纳入版本管理），与主线独立，暂不用管。

## 4. 教学进度（接手从这里继续）

- ✅ **第 1 步完成**：`BP_MyplayerCharacter` 的 `AttackMontage` 已从 `AM_ComboAttack` 换成 `AM_MyComboAttack`（uasset 有改动）。
- ✅ **第 2 步已改并编译成功（16:16）**：
  - `LogSkylineCharacter.h`：`DoAttack()` 加 `UFUNCTION(BlueprintCallable, Category="Combat")`；加普通成员 `bool bIsAttacking = false;`
  - `LogSkylineCharacter.cpp` 的 `DoAttack()`：状态守卫 → `IsValid(AttackMontage)`（带 Warning 日志）→ `IsValid(AnimInstance)` → `Montage_Play` 返回 `Duration`，`<=0` 直接返回 → `FOnMontageEnded::BindLambda` + `Montage_SetEndDelegate` 播完/打断时复位状态（不用 Tick、不用定时器）。
  - 依赖已核：`Animation/AnimInstance.h`（`FOnMontageEnded` 定义处）、`LogSkyline.h`（`LogLogSkyline`）。
  - **DLL = 16:16 / 796160 B**（旧 00:17 / 793600 B），PDB 69 MB；`grep -a` 已确认含 `Combat|Input`、`Combat|Animation`、`BP_OnAttackStarted`、`On Attack Started`、`DoAttack`、`AttackAction`、`AttackMontage` → 反射结构已进引擎。UBT `Result: Succeeded`。
  - 编译途中两个坑（详见 §2）：16:03 VS F7 被 Live Coding 互斥锁拦下（UHT 跑了但没链接）；16:14 命令行编译被 UBA 权限问题搞坏 DLL/PDB/lib，**清理 0 字节产物后加 `-NoUBA` 重编成功**。
  - 待用户验证：蓝图事件图表右键搜到 `Do Attack`；PIE 连按攻击不重播；清空 `AttackMontage` 后按攻击 Output Log 出现 `Dummy: AttackMontage 未指定`。
  - 还原注意：第 2 步与 13:54 结构改造已随 `07ec343` 提交，`git checkout -- Source/LogSkyline/LogSkylineCharacter.h Source/LogSkyline/LogSkylineCharacter.cpp` 现在只能回到**该提交的版本**（仍含改造），**不再能撤销改造**。若要取回改造前的代码：`git checkout aca568d -- Source/LogSkyline/LogSkylineCharacter.h Source/LogSkyline/LogSkylineCharacter.cpp`（会形成未提交改动，需自行决定是否提交）；**改完必须重新编译**（`-NoUBA` 增量编译只需数秒）。
- ✅ **13:54 结构改造第 1 步（C++ 侧一次改完，已随 16:16 编译生效）**：① `AttackAction` → `UPROPERTY(EditDefaultsOnly, Category="Combat|Input")`、`AttackMontage` → `Category="Combat|Animation"`（收紧为类默认值可配）；② 新增 `UFUNCTION(BlueprintImplementableEvent, Category="Combat", meta=(DisplayName="On Attack Started")) void BP_OnAttackStarted();`（**C++ 只触发，禁止在 .cpp 写函数体**，UHT 生成 thunk）；③ `DoAttack()` 在 `bIsAttacking = true` 后调用它，结束委托逻辑顺延为第 5 步。
  - 意图：补齐「表现反馈」层与「编辑器可配」命名规范，让 C++ 侧结构一次性到位，只编译/重启一次。**此钩子非功能必需**（用户 16:10 明确觉得绕，已解释可随时删除，删掉不影响第 3 步）。
  - 后续步骤：**第 2 步** 重开 `.uproject` 后四验证点（Combat 分类、右键有 `Do Attack`、蓝图「添加事件 → On Attack Started」、接 Print String 后 PIE 出现文字）；**第 3 步** 蓝图加 `Touch Attack` 额外触发源 → `Do Attack`、把 Print String 换成音效/特效（不需再编译 C++）。
### 三段连击（2026-11-16 完成并验证，本次待提交）

- ✅ **整条链已跑通**：左键 → `IMC_Default` → `IA_ComboAttack` → `BindAction` → `DoAttack()`（守卫中记按键时间）→ 蒙太奇 `Check Combo String` 通知 → `Cast<ICombatAttacker>` → `CheckCombo()` → `Montage_JumpToSection(Melee02/03)`。
- 起因：`AM_MyComboAttack` 从初始提交就含完整三段（`MM_Attack_01/02/03`、Section `Melee01/02/03`、`NextSectionName`、两类 Notify），但 `DoAttack()` 只 `Montage_Play` 从头播到尾、角色未登记接口 → 通知 `Cast` 认不出人、静默失效。资产一个字节没改，全补在 C++。
- 分三步（每步单独验证，先「接通」后「跳段」）：
  1. 守卫 `if (bIsAttacking)` 的 `return` 前加 `CachedAttackInputTime = GetWorld()->GetTimeSeconds();`（拦重播不变，只是记一笔；时间戳而非 bool，因为步骤 3 要判「多久前」）。
  2. 登记接口：`.h` 加 `#include "CombatAttacker.h"`（须在 `generated.h` 前）、类声明 `, public ICombatAttacker`、三个 `override`（少一个则类仍抽象、蓝图子类编译不过）；`.cpp` 三实现中两个留空体，`CheckCombo()` 先只打日志验证「叫得到人」。
  3. `CheckCombo()` 完整实现：五道判断（`bIsAttacking` → 窗口 `now - CachedAttackInputTime <= 0.45f` → 消费 `=0` → `++ComboCount` → `ComboCount < 3`）→ 跳段。`ComboCount = 0` 只在 `DoAttack()` 全新攻击成功播蒙太奇时归零（攻击中再按走 return 不归零）。
- **验证证据**：DLL 19:58 比源码新；`Saved/Logs/LogSkyline.log:2311-2318` 有 4 组 `Combo step 1 -> Melee02` / `Combo step 2 -> Melee03` 交替（约 0.57s 间隔）；无 `JumpToSectionName failed` 警告。
- ⚠️ **踩坑记录**：三行成员第一次写在 `.h` 的**类外**（`DECLARE_LOG_CATEGORY_EXTERN` 上方）= 全局变量，单角色 PIE 仍能跑（只有 `.cpp` include 该 `.h`、未限定名解析到全局），但语义错误（全游戏共享一份 + 将来第二个 `.cpp` include 会重复定义）。已挪回类内 `CachedAttackInputTime` 下方。**属角色的状态必须写在类内。**
- 关键机制：`Montage_JumpToSection` 是**当场拨同一蒙太奇实例的播放指针**（`AnimInstance.cpp:3006` → `JumpToSectionName` → `AnimMontage.cpp:1824 SetPosition`），不排队、不换资产、结束委托不提前触发、`bIsAttacking` 全程 true。故三段 = 一次播放中途拨两次指针。`NextSectionName` 才是「这段播完去哪」，两者不同。段落名写错**会打 `LogAnimMontage` Warning**（非完全静默）；真正静默的是 `AnimNotify_CheckCombo` 里 `Cast` 失败。
- 分工答疑（用户问「标准项目是否该放蓝图」）：模板 `CombatCharacter.h` 里资产引用 / `ComboSectionNames` / 0.45s 阈值都是 `UPROPERTY(EditAnywhere)` 在蓝图配，`ComboCount` / 各类 flag 是普通成员。本项目当时为学习阶段刻意硬编码（少一次反射变更、不碰蓝图），功能等价；**2026-11-17 已按此升级为蓝图可配，见下节**。口诀：**策划要调 / 要换资源 → 蓝图；规则 / 逐帧状态 → C++**。

### 11-17 升级：连击参数搬到蓝图 + 默认值兜底

- **改动（已编译，DLL 19:09 晚于源码 17:57）**：
  - `.h`：`ComboInputCacheTimeTolerance` → `UPROPERTY(EditAnywhere, Category="Combat|Combo", meta=(ClampMin=0, ClampMax=5, Units="s"))`，默认 0.45。
  - `.h`：`ComboSectionNames` → `UPROPERTY(EditAnywhere, Category="Combat|Combo")`，**保留**类内初始化器 `= { FName(TEXT("Melee01")), FName(TEXT("Melee02")), FName(TEXT("Melee03")) }`。
  - `.cpp` `DoAttack()`（`bIsAttacking = true; ComboCount = 0;` 之后）：加配置自检，`ComboSectionNames.Num() < 2` 时打 `UE_LOG(..., Warning, "[Combo] ComboSectionNames 只有 %d 项 ...")`，避免漏配后静默退化成单段。
- **为什么要保留兜底**：`EditAnywhere` 只负责「让蓝图能改」，不负责「没配时还能用」。上一版把默认值删成空数组后，一旦蓝图不填或填错，连招就静默降级为「只打第一段」，编译和运行都不报错。标准写法是 **C++ 给可用默认值 + 蓝图可覆盖**（同 `ComboInputCacheTimeTolerance = 0.45f`）。
- **⚠️ 蓝图覆盖优先（通用坑）**：属性一旦在蓝图里被保存过，蓝图 CDO 就有自己的一份值，**C++ 默认值改它不生效，包括「空数组」这个值**。要让兜底生效必须在 Details 面板点该属性右侧的 **Reset to Default**（黄色回退箭头）。
- **三种配置对照（11-17 实机验证）**：

  | 蓝图里的值 | `Num()` | `ComboCount(1) < Num()` | `JumpToSection` | 表现 |
  |---|---|---|---|---|
  | 空数组 | 0 | false | 不调用 | 只打第一段 |
  | 名字错（实测 `Melee0`） | 3 | true | `failed` + Warning | 只打第一段 |
  | `Melee01/02/03` | 3 | true | 成功 | 正常三段 |

- **历史误解澄清（重要）**：用户曾反馈「Class Defaults 里明明是空的，却能正常 A 三段」。真因是 `d49d82e` 版本里 `ComboSectionNames` **不是 `UPROPERTY`**，只是普通 C++ 成员，所以蓝图面板上**根本没有这一项**，用户看到的「空」等于「面板上不存在」；而那三个名字一直写在类内初始化器里，运行时 `Num()` 恒为 3。改成 `EditAnywhere` 并清空默认值之后，「空」才第一次等于真的空。**排查此类问题不要只看面板，先查 `git show <commit>:Source/.../LogSkylineCharacter.h` 确认声明方式。**
- **验证证据**：`Saved/Logs/LogSkyline.log` 2026-11-17 09:31（UTC，即 17:31）连续 4 组 `LogLogSkyline: Combo step 1 -> Melee0` + `LogAnimMontage: Warning: JumpToSectionName Melee0 ... failed` → 证明「错名只打 Warning、不停播，行为退化为单段」，与源码 `AnimMontage.cpp:1816-1837`（找不到 Section 时仅 `UE_LOGF(Warning)` 后 `return false`）一致。
- **编译与验证（11-17 19:09 完成）**：完全退出编辑器（含 `LiveCodingConsole.exe`）→ VS2026 F7 → DLL 19:09（源码 17:57、obj 19:09:09）。用 `grep` 在 DLL 里搜到宽字符 `[Combo] ComboSectionNames` 与 `缺少后续 Section`（`TEXT()` 在 Windows 下是 UTF-16，须按 `utf-16-le` 搜），证明新代码进了二进制而非仅改时间戳。PIE 打三下输出 `Combo step 1 -> Melee02` / `Combo step 2 -> Melee03`，无警告。注意 `UHT/*.generated.h` 时间戳未变属正常——只改默认值初始化器与函数体、未增删反射属性时 UHT 不重跑。

### 11-17 分工改造：把「表现」交给蓝图

- **起因**：用户问「标准项目里有没有该放蓝图的部分」。先做职责审计，结论是规则层不用动，缺的是表现层的出口。
- **审计表**（`LogSkylineCharacter.h/.cpp` 现状）：

  | 类别 | 内容 | 位置 | 归属 |
  |---|---|---|---|
  | 规则 / 逐帧状态 | `bIsAttacking` 守卫、`IsValid` 资源守卫、`Montage_Play` 返回值判断、`EndDelegate` 解锁 | `.cpp:149-193` | 必须 C++ |
  | 规则 / 逐帧状态 | `CheckCombo()` 的 0.45s 窗口、`++ComboCount`、跳段 | `.cpp:201-222` | 必须 C++ |
  | 参数 / 资源 | `AttackMontage`、`AttackAction`、`ComboInputCacheTimeTolerance`、`ComboSectionNames` | `.h:55-71` | 已在蓝图侧 |
  | 表现钩子 | `BP_OnAttackStarted()`、`BP_OnComboStep(int32)` | `.h:99,102` | C++ 触发，蓝图实现 |
  | 手感数值 | `MaxWalkSpeed`、`JumpZVelocity`、`AirControl`、`BrakingDeceleration*`、`CrouchedHalfHeight`、`MaxWalkSpeedCrouched`、`RotationRate`、`TargetArmLength` | `.cpp:31-48` | **仍硬编码，未动**（见末条） |

- **「搬不搬」的判据**：不要用「数值型就放蓝图」，用**缺失时是否静默失效**。`ComboSectionNames` 空 → 连招静默退化 → 必须 C++ 兜底；`MaxWalkSpeed=0` → 角色不动、一眼可见 → 兜底价值低。
- **关键认知**：构造函数里设的值**本身就是 CDO 默认值**，蓝图面板一直能覆盖它（`.cpp:33-34` 引擎原注释：`For faster iteration times these variables ... can be tweaked in the Character Blueprint instead of recompiling`）。所以「搬到蓝图」常常不需要改代码。`EditAnywhere` 只是多开「关卡实例」这一层入口，**不改变** `C++ CDO → 蓝图 CDO → 关卡实例` 的优先顺序。
- `EditDefaultsOnly` 与 `EditAnywhere` 对**本项目角色蓝图无实践差别**：角色实例由 PIE 运行时 spawn，编辑时只有 Class Defaults 一个入口；差别只体现在关卡里手动摆放的 Actor（如 `BP_CombatDummy`）。

**钩子 1 · `BP_OnAttackStarted`**（`07ec343` 已声明，11-17 才首次在蓝图实现）

- C++ 在 `.cpp:185` 调用；蓝图 `BP_MyplayerCharacter` 事件图加 `On Attack Started` → `Print String`。
- 声明后长期**空转**：C++ 每次攻击都调用，蓝图无对应节点，既不报错也不崩。这是 `BlueprintImplementableEvent` 的设计预期，不是遗漏。

**钩子 2 · `BP_OnComboStep(int32 ComboStep)`**（11-17 新增）

- `.h`：`UFUNCTION(BlueprintImplementableEvent, Category="Combat", meta=(DisplayName="On Combo Step")) void BP_OnComboStep(int32 ComboStep);` —— **只写声明，不写函数体**。
- `.cpp`：`CheckCombo()` 中放在 `Montage_JumpToSection` **之后**调用 `BP_OnComboStep(ComboCount);`。放之后是因为只有真跳成功才该出表现。
- 蓝图侧：`On Combo Step` 的 `ComboStep`（Int）→ `To String (Integer)` → `Print String`。**Int 不能直连 `In String`**，UE 不自动转换。
- **参数是数组下标语义，不是「第几段」**：`Melee01` 不触发；跳到 `Melee02` 传 **1**；跳到 `Melee03` 传 **2**。
- UHT 生成的 thunk 解释了「蓝图不实现也不崩」：

  ```cpp
  void ALogSkylineCharacter::BP_OnComboStep(int32 ComboStep)
  {
      LogSkylineCharacter_eventBP_OnComboStep_Parms Parms;
      Parms.ComboStep = ComboStep;
      UFunction* Func = FindFunctionChecked(NAME_ALogSkylineCharacter_BP_OnComboStep);
      ProcessEvent(Func, &Parms);   // 蓝图没接这个事件 = 没人接收，直接返回
  }
  ```

- **顺序铁律**：`.h` 声明 → 编译（UHT 重跑 + 链接）→ 蓝图里才出现该事件。顺序反了会一直搜不到，是「节点不出现」类问题最常见的原因。
- **验证证据**：`UFUNCTION` 属反射变更，Live Coding 做不了。实测 `.h` 19:27 → `LogSkylineCharacter.gen.cpp` 19:29（UHT 重跑）→ DLL 19:30，DLL 内命中反射名 `BP_OnComboStep` / `ComboStep`。蓝图 19:38 含 `OnComboStep`×2 + `K2Node_Event` + `ToString` + `PrintString`；日志 19:38:18 / 19:38:21 / 19:38:25 三轮完整跳段，无警告。
- **未做**：构造函数那批手感数值没清理。真要搬需先确认「删掉代码后蓝图 CDO 会回落到引擎默认值」——`ACharacter` 的 `MaxWalkSpeed` 引擎默认是 **600**，与当前 500 不同，直接删会改手感。

- ✅ **第 3 步（打击，2026-11-20 完成，提交 `d9f055e`）**：让攻击打得到人。
  - 实现：`LogSkylineCharacter.cpp:198` 起 `DoAttackTrace(FName DamageSourceBone)` 球扫——`GetMesh()->GetSocketLocation` 取起点、沿 `GetActorForwardVector() * MeleeTraceDistance` 延伸、`SweepMultiByObjectType`（`ECC_Pawn` + `ECC_WorldDynamic`，`AddIgnoredActor(this)`）→ 按 Actor 去重 `TSet<AActor*> AlreadyHit` → `Cast<ICombatDamageable>` → `ApplyDamage(MeleeDamage, this, Hit.ImpactPoint, Impulse)`，冲量 `ImpactNormal * -Knockback + UpVector * Launch`。
  - 参数（`.h`，`Category="Combat|Melee"`，全部 `EditAnywhere`）：`MeleeTraceDistance=75` / `MeleeTraceRadius=75` / `MeleeDamage=1` / `MeleeKnockbackImpulse=250` / `MeleeLaunchImpulse=300`，取自 `Variant_Combat/CombatCharacter.h:108-132`。
  - 去重是模板没有的：同一 Actor 多个碰撞体会返回多条命中，不去重则冲量叠加、一刀多份伤害。
  - 验证：日志 `打击: 扫到 BP_CombatDummy_C_...` 9 次 / 没扫到 3 次；木桩被推得晃动，屏幕出现蓝色火花。
- **木桩的两条「非教学」行为（已解释，非 bug）**：
  - **打击特效不是用户加的**：`ACombatDummy::ApplyDamage` 调 `BP_OnDummyDamaged`，模板 `BP_CombatDummy` 已实现该事件并 `SpawnSystemAtLocation(/Game/Variant_Combat/VFX/NS_Damage)`。换任何 `ICombatDamageable` 实现者都各自负责自己的受击表现。
  - **木桩是「吊」着的不是「站」着的**：`CombatDummy.cpp` 构造函数 `Dummy->SetSimulatePhysics(true)` + `PhysicsConstraint->SetConstrainedComponents(BasePlate, None, Dummy, None)`；引擎约束默认 **Linear 三轴 Locked / Angular 三轴 Free**（`ConstraintInstance.cpp:359-370`）＝倒立摆。模板蓝图把角向改成 `Limited` 才能晃而不倒。摆进关卡时若 `Dummy` 与地面穿插，第一帧解算会把它顶歪；位置被锁死所以冲量只转成角速度 → 一直旋转。修法：抬高 Z 5~10、角向限位 15°、`Angular Damping` 调 3。
- ⏭ **第 4 步（进行中，2026-11-20）**：让伤害「有后果」。`ICombatDamageable` 四个函数目前三个是空 stub（`HandleDeath` / `ApplyHealing` / `NotifyDanger`），木桩永远打不死。
  - 4-1 加血量：`ACombatDummy` 加 `MaxHP`(默认 5，对齐 `ACombatCharacter.h:78`) / `CurrentHP`，**在 `BeginPlay()` 里初始化 `CurrentHP = MaxHP`**（不可放构造函数，否则蓝图改 `MaxHP` 后 `CurrentHP` 仍是旧值）；`ApplyDamage` 扣血 + 日志。验收：打五下，HP 4/5 → 0/5。
  - 4-2 `HandleDeath()`：断约束 / 让木桩倒地（模板 `ACombatCharacter::HandleDeath` 可参照：禁移动、开 ragdoll、隐藏血条、定时重生）。
  - 4-3（可选）血条 UI：模板 `Content/Variant_Combat/UI/UI_LifeBar` + C++ 类 `UCombatLifeBar`（`SetLifePercentage`），`ACombatCharacter` 用 `UWidgetComponent* LifeBar` 挂在角色上。
  - 注意：改 `ACombatDummy` 基类会同时影响 `Lvl_Combat` 里的同款木桩（共用 `BP_CombatDummy`）。
- ⏸ **第 5 步（以后）**：`FGameplayTag` 管攻击中/可连击状态；多人则蒙太奇走服务器 RPC / GAS。

## 5. 架构约定（用户已确认理解）

- **蓝图是 C++ 类的子类，共享同一套反射数据**，不是两套体系。三种可见性：`UPROPERTY` → 类默认值出现属性；`UFUNCTION(BlueprintCallable)` → 节点列表出现调用节点；函数体 / `BindAction` → **不生成节点但运行时生效**。
- 「暴露」有三类对象：**编辑器**（`UPROPERTY(EditDefaultsOnly)` 填资源数值）、**蓝图逻辑**（`UFUNCTION`）、**其他 C++ 系统**（`UINTERFACE` 纯虚函数），第四类是 **C++ 内部无宏**。
- 分工口诀：每帧跑 / 规则一致性 / 要复制 → C++ 内部；换资源调数值 → `UPROPERTY`；蓝图主动发起 → `BlueprintCallable`；C++ 触发蓝图表现 → `BlueprintImplementableEvent`。
- 精确表述：**触发源（外在时机：触控 / UI / 剧情）用蓝图编排，能力 + 内在规则（连击窗口 / 冷却 / 状态 / 网络）用 C++ 实现**。输入回调是离散事件，蓝图绑输入完全合法；「每帧逻辑必须 C++」只管 Tick。
- **13:52 判断原则（收束）**：「事件触发逻辑」不能一刀切。外在触发源（触控 / UI / 剧情 / 调试）→ 蓝图；内在触发规则（键鼠输入绑定、状态守卫、连击窗口 0.45s、输入缓存、冷却、打断判定）→ C++；动画时间轴事件（`AnimNotify` 命中帧）→ C++（蓝图图表接不了）；表现反馈（特效 / 音效 / UI / 震屏）→ 蓝图 `BlueprintImplementableEvent`。公式：**触发源可以多个、分散两层；规则只能一份、在 C++**。三堵墙：① 连击窗口/冷却若每帧跑，Blueprint VM 约 10x 开销且帧率波动影响判定 ② 蓝图直接调 `Montage_Play` 会绕过 `bIsAttacking` ③ 多人时输入/状态需 CMC/ASC 复制，蓝图本地触发不同步。成熟做法（Lyra）：C++ 定义能力+规则并暴露 `BlueprintCallable` 单一入口，蓝图负责额外触发源与表现，状态机/冷却/复制永远 C++。判断口诀：「换了设计师要不要改？」要 → 蓝图；「换了帧率/网络还要不要一样？」要 → C++。
- **暴露入口，不暴露内部步骤**：若把 `Montage_Play` 暴露给蓝图，会绕过 `bIsAttacking` 守卫。
- 输入绑定现状：模板基线把 5 个输入绑在 C++，蹲下是 `aca568d` 后加的蓝图例外。两者都合法，不是设计错误；真要统一应把蹲下并进 C++，**现阶段不急**。
- 攻击推荐接法：保留 C++ 键盘 `BindAction(AttackAction,...)` 不动，蓝图另加 `Touch Attack` / UI 按钮 → `Do Attack`（第 2 步的 `BlueprintCallable` 已铺路），两条路径汇到同一函数且不双重触发。若把键盘攻击也搬进蓝图，**必须删 C++ 第 77 行**避免双重触发。
- **不要为了「让蓝图图表看见攻击」而搬逻辑**：`BindAction` / 函数体不生成节点是正常设计，不是缺陷；搬过去只会让 5 个键鼠输入里唯独攻击在蓝图，更不一致。三方案：A 保留 C++ 键盘绑定 + 蓝图加额外触发源（推荐）；B 删 C++ 第 77 行、蓝图放 `IA_ComboAttack` 事件节点（可行但不推荐）；C 不改。怕找不到逻辑就留线索：蓝图注释框写明「键盘攻击由 C++ `SetupPlayerInputComponent` 绑定，见 `LogSkylineCharacter.cpp:77`」，并查本笔记 §3/§5 或 `grep -rn "BindAction" Source/`。
- 接口概念（用户经「点名 / 能力登记卡」类比理解）：`ICombatAttacker` 是登记表；`Cast` 是判断门槛，接口还提供**统一函数名与参数**作为调用入口 + 纯虚函数保证有实现；外部「写」接口，「跑」的是对象自带虚函数表里对应类的函数体。`AnimNotify_DoAttackTrace.cpp` → `Cast<ICombatAttacker>(MeshComp->GetOwner())` → `DoAttackTrace()`；配对 `ICombatDamageable` → `ACombatDummy::ApplyDamage`。**`ALogSkylineCharacter` 已实现 `ICombatAttacker`（2026-11-16）**：`CheckCombo` 通知已生效并驱动三段连击；`DoAttackTrace` 通知也能叫到角色，但其实现目前是空函数体（打击待做，见 §4）。

## 6. 工作区状态与还原

- 常见 `git status`（`07ec343` 推送后）：工作区干净，仅 `?? .claudiaideconfig`（VS 插件的本地配置，含本机私有路径，刻意不入库）。
- 原 `.sln` / `.slnx` 备份在 `%TEMP%\LogSkyline.sln.bak-20261114` / `.slnx.bak-20261114`；去掉 `-NoPrograms` 重跑 UBT 可还原完整方案。
- 本文件自 `07ec343` 起**已被 Git 跟踪并推送到公开仓库**：`git pull` 会更新它，`git clean -fdx` 不会删除被跟踪文件；另一台机器 clone 后即可读到。仓库为 public，内容（本机路径、学习偏好、排障细节）对外可见，且已留在该提交的历史中。

## 7. 给接手会话的操作提示

1. 先读本文件；需要排障细节再读 `.workbuddy/memory/` 下对应日期的日志。
2. 动代码前先按 §2 确认 DLL 是否编译；很多「属性搜不到 / 节点不出现」都源于此。
3. 一次只推进一小步，给可见验证点；用户偏好先理解再动手。

## 8. 常见误解速查

- **「蓝图图表里有节点」≠「实现写在蓝图」**：`CallFunc_DoMove_*` / `DoJumpStart` / `Crouch` 都是蓝图**调用 C++ 函数**的节点。分三层：能力实现（C++）、触发源（C++ `BindAction` 或蓝图触控/UI）、桥（`UFUNCTION(BlueprintCallable)` 让蓝图能生成调用节点）。实现从来都在 C++，无例外。
- **为什么 Move/Look/Jump 在蓝图有节点而 Attack 没有**：模板为**移动端触控**（`BPI_TouchInterface`）需要第二条触发路径，且这几个函数模板自带 `BlueprintCallable`；`DoAttack()` 原本**没有** `BlueprintCallable`（第 2 步才补），且 `02724ae` 只做了 C++ 键盘绑定、没做触控攻击按钮。
- 对照：`DoMove/DoLook/DoJump` = C++ 实现 + 模板自带宏 + 蓝图触控节点；`DoAttack` 第 2 步前 = C++ 实现 + 无宏 + 不可能有节点，第 2 步后 = C++ 实现 + 有宏 + 可加节点（尚未放）；`Crouch/UnCrouch` = 引擎 C++ API + 引擎自带宏 + `IA_Crouch` 节点。
- 结论：`BlueprintCallable` 决定「蓝图能不能调」，**不决定「实现在哪」**；差别只是触发源数量（Move/Look/Jump 两个：键鼠 + 触控；Attack 目前一个：键鼠）。
- **`BindAction` 是什么（14:26 教学）**：登记「某个 IA 触发时调用哪个函数」的一根线，本身不执行动作。四参数 = IA 资产 / `ETriggerEvent`（`Started` 刚按下、`Triggered` 持续给值、`Completed` 松开）/ 目标对象 / 函数指针；项目 `.cpp:77` = `BindAction(AttackAction, Started, this, &DoAttack)`，**登记时 C++ 就知道调谁，运行时只是查表分发**。只在 `SetupPlayerInputComponent` 登记**一次**，之后每次按键由引擎自动分发，不是每帧、也不是每次按键都跑。Enhanced Input 三层：IA（是什么动作，Content 资产）→ IMC（哪个键触发，Content 资产）→ BindAction（触发后调哪个函数，C++）。蓝图 `IA_Crouch` 事件节点本质同一根线，只是画成节点。不写它则资产与函数互不相连，按键无反应。速查：`grep -n "BindAction" Source/LogSkyline/LogSkylineCharacter.cpp` 看全部 6 条登记。
- **「BP_OnAttackStarted 不写 .cpp 实现」不是漏写（16:04 教学）**：`BlueprintImplementableEvent` 的函数体归蓝图，UHT 在 `*.gen.cpp` 自动生成 thunk（运行时查蓝图有无实现该事件）；若再手写 `void ALogSkylineCharacter::BP_OnAttackStarted(){}` 会与 thunk 重复定义 → 编译失败。蓝图未实现时是 no-op，故可「先加钩子、后接线」，中间项目照跑。方向对照：`BlueprintCallable`（`DoAttack`）= C++ 写函数体、蓝图调；`BlueprintImplementableEvent` = 蓝图写函数体、C++ 调；`BlueprintNativeEvent` = C++ 写 `_Implementation` 默认实现、蓝图可覆盖。`DoAttack()` 第 146–186 行的动画逻辑（守卫 / `Montage_Play` 第 168 行 / 结束委托）一行未删，只插入第 177 行一次触发。
- **「为什么要绕一个 BP_OnAttackStarted」（16:10 教学，用户明确觉得绕）**：这一步**不是功能需求**，是主动补的未来准备；删掉 `.cpp:177` 与 `.h:84-86` 声明，攻击/动画/防连按行为完全不变。外部仍然是**直接调 `DoAttack()`**，钩子不在调用链上，而是主链内部向蓝图广播的**旁支**（方向 C++ → 蓝图），蓝图未实现时是 no-op，所以现在这一嗓子没有任何可见效果。价值只在将来加音效/特效时：蓝图「添加事件 → On Attack Started」接一下即可（编译蓝图，秒级），否则要改 C++ → 关编辑器全量编译 → 重开，且把具体音效资源焊死在代码里（C++ 不该知道播哪个音效）。**用户若只关注功能与第 3 步，可以现在删掉它**；趁 13:55 改动尚未编译成功，删掉不会多一次全量编译。（**11-17 更新：该钩子最终保留，并已在蓝图实现为 `On Attack Started → Print String`，见 §4「11-17 分工改造」；当前行号为 `.h:97-99` 与 `.cpp:185`，不再是本条写的 `.h:84-86` / `.cpp:177`。**）
- **「为什么音效走钩子、动画不走」（16:26 教学）**：答案不是「不能复用」——C++ 里并排写一行 `PlaySound` 完全能编译运行；差别在于 `Montage_Play` 那条链是**双向且参与状态机**的：`Duration` 返回值判断播放成败（`.cpp:168-172`）、`Montage_SetEndDelegate` 解锁 `bIsAttacking`（`.cpp:180-185`）、蒙太奇时间轴上的 `AnimNotify_DoAttackTrace` / `AnimNotify_CheckCombo` 驱动第 3 步命中与连击 → 蒙太奇是**攻击的计时器和逻辑时间轴**，不只是表现。而 `BP_OnAttackStarted` 是**单向广播、无返回值**，把蒙太奇挪过去 C++ 就拿不到 Duration、挂不上结束委托，状态永远不解锁。判断口诀：**「它会不会影响判定、时长、状态？」会 → C++ 内部直接调；不会 → 广播给蓝图**。钩子也不是「音效专用接口」，而是「一次真实攻击开始了」这个事实的广播，音效/特效/震屏/UI/成就都是消费者。反过来，纯装饰不参与判定的动画（披风、拖尾）用钩子在蓝图播更合适——分界是「参与规则 vs 纯表现」，不是「动画 vs 音效」。
- **Montage 的心智模型（16:46 教学，用户自述「之前可能理解错了」）**：`UAnimMontage` **不是动画**，是「分镜脚本 + 时间轴」；动画数据是 `AnimSequence`（`MM_Attack_01/02/03`）。四层结构：`SlotAnimTracks` / `DefaultSlot`（插播通道，叠加在 AnimBP 状态机输出之上，播完自动退回，状态机无需为攻击加状态）、`AnimSegments`（胶片顺序 / `AnimStartTime` / `AnimPlayRate`）、`CompositeSections` + `NextSectionName`（可跳章节，模板 `CheckCombo()` 用 `Montage_JumpToSection(ComboSectionNames[ComboCount], ...)`，输入窗口 0.45s）、`AnimNotifyTracks`（时间轴打点：`AnimNotify_DoAttackTrace` → `Cast<ICombatAttacker>` → `DoAttackTrace`；`AnimNotify_CheckCombo` → `CheckCombo`）；另有 `BlendIn/Out`。资产绑定 `SK_Mannequin` 骨架。**意义 = 把「时间」变成可编程对象**：代码 `Montage_Play` 主动启动并拿回 `MontageLength`，`Montage_SetEndDelegate` 拿结束/打断通知，Section 让代码跳段，Notify 让动画在指定帧反过来叫代码。所以换 Montage 不只是换画面，连攻击时长、命中帧、连击窗口、结束时机一起换。常见误解：① 以为是一段动画 ② 以为由 AnimBP 状态机决定播放 ③ 以为是纯表现、与逻辑无关 ④ 以为播完代码不用管。**本项目现状（2026-11-16 更新）**：`AM_MyComboAttack` 的完整答案已被 C++ 用上——角色已登记 `ICombatAttacker`、`CheckCombo` 通知驱动 `Montage_JumpToSection` 跳 `Melee02/03`，三段连击验证通过；`DoAttackTrace` 通知也能叫到角色但实现为空（打击待做，见 §4）。
- 排查口诀：`grep -rn "UFUNCTION(Blueprint" Source/LogSkyline/LogSkylineCharacter.h` 看哪些函数允许蓝图调用；`grep -rn "BindAction" Source/` 看键鼠触发源；蓝图 `Touch Input` 注释框看触控触发源。
- **事件图表里混了两类节点（13:49 核实）**：`Touch Input for First Person Movement` 注释框**内** = 触控/移动端（`BPI_TouchInterface_C` 虚拟摇杆、`Touch Jump Start/End`、调用 `DoMove`/`DoLook`/`DoJumpStart`），PC 上没有虚拟摇杆 UI 时不会触发；注释框**外**唯一的 EnhancedInput 事件节点 `InpActEvt_IA_Crouch_K2Node_EnhancedInputActionEvent` = **标准 Enhanced Input，PC 端也走这里**（`IA_Crouch` 在 `IMC_Default` 有键位映射）。C++ 里**没有** `CrouchAction` 属性、也没有 `BindAction(CrouchAction,...)`，所以蹲下是唯一完全靠蓝图事件节点驱动的 PC 输入；其余 5 个键鼠输入（Jump/Move/Look/MouseLook/Attack）都在 C++ `SetupPlayerInputComponent`。PIE 用键盘时实际走「C++ 那 5 个 + 蓝图 `IA_Crouch` 事件」，两套入口最终汇到同一批 C++ 函数。
- **「Class Defaults 里是空的，为什么还能连三段」（11-17 澄清）**：先分清两件事——**面板上有没有这一项** vs **这一项的值是不是空**。`d49d82e` 时期 `ComboSectionNames` 没有 `UPROPERTY`，蓝图面板上不存在该属性，默认值写在 C++ 类内初始化器里照样生效；2026-11-17 改成 `EditAnywhere` 后它才出现在面板，此时「空」才等于真的空。判断顺序：① 声明有没有 `UPROPERTY`（`grep -n ComboSectionNames Source/LogSkyline/LogSkylineCharacter.h`）② 类内初始化器有没有默认值 ③ 蓝图 CDO 有没有覆盖 ④ 最后才看运行时日志。其中 ③ 的规则是 **蓝图 CDO > C++ 类默认值**：蓝图保存过就存了自己一份，之后 C++ 怎么改默认值都不跟随，包括「空数组」这个值；要恢复继承得点属性行的 Reset to Default（黄色回退箭头）。反过来看同样成立——C++ 里「看起来是空」也不代表运行时是空，这就是 11-17 那次误判的根因。
- **「`BlueprintImplementableEvent` 声明了但蓝图一直没实现，会不会崩」（11-17 用户主动提问）**：不会。UHT 为它生成的 thunk 是 `FindFunctionChecked` + `ProcessEvent`，蓝图事件图里没有对应节点就等于没有接收者，`ProcessEvent` 直接返回，不报错不崩溃。**没有实现 ≠ 遗漏**，这类钩子天生可选（`BP_OnAttackStarted` 就这样空转了三天才在 11-17 接上）。反过来，C++ 侧**必须**只写声明、不写函数体，写成 `{ }` 会编译报错；漏写 `UFUNCTION` 宏则链接失败（缺 thunk 定义）。
- **「`EditDefaultsOnly` 和 `EditAnywhere` 到底差在哪」（11-17 教学）**：差别只在**关卡里摆着的实例**的 Details 面板里会不会出现这一项。`EditDefaultsOnly` 只在 Class Defaults 可改；`EditAnywhere` 两处都可改；`EditInstanceOnly` 只在实例可改；`Visible*` 系列是对应 `Edit*` 的只读版。**对 `BP_MyplayerCharacter` 这种运行时 spawn 的角色无实践差别**——编辑时只有 Class Defaults 一个入口。真正能看出区别的是关卡里手摆的 Actor（`BP_CombatDummy` 在 `Lvl_Combat` 里摆了多个，逐个可调）。用哪个取决于「要不要允许某个实例偏离类型默认」：`AttackMontage` 用 `EditDefaultsOnly`（一套动作属于角色类型，不允许实例换），`ComboInputCacheTimeTolerance` 用 `EditAnywhere`（手感参数，未来可能让不同变体有不同窗口）。
