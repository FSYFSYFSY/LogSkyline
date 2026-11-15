# LogSkyline 项目交接笔记 / AI Handoff

> **给接手的 AI：请直接读取本文件作为上下文起点**，不需要翻历史对话。
> 更细的逐日排障日志在 `.workbuddy/memory/2026-11-08.md`、`2026-11-14.md`、`2026-11-15.md`。
> 最后更新：2026-11-15 16:16 (GMT+8)

## 0. 用户与协作方式（先读这段）

- 用户是 **C++ 与 Unreal Engine 双初学者**，通过 UE 5.8 第三人称模板学游戏开发，目标是单人 demo（已放弃多人方向）。
- 偏好：**一次只给一步**、先讲思路再动手、用具体执行故事和生活化类比，**避免堆术语**；讲抽象概念后配一个 5 分钟能看到结果的实验。
- 不要一次性大改代码；改动前说明每行作用，改完给可验证的反馈点。用**简体中文**回答。
- 用户会反复追问「为什么」，需要把历史原因与技术原因分开讲清楚，不要只给结论。

## 1. 项目与环境

| 项 | 值 |
|---|---|
| 项目根 | `D:\LogSkyline\LogSkyline` |
| 引擎 | `D:\UE_5.8`，**手动安装的 Installed Build**（注册表未登记 5.8） |
| 模板 | UE 5.8 Third Person，含 `Variant_Combat` / `Variant_Platforming` / `Variant_SideScrolling` |
| uproject | `LogSkyline.uproject`，`EngineAssociation` 已被 UE 改成 `"5.8"`（未提交改动） |
| Git | `FSYFSYFSY/LogSkyline`，分支 `FSY_tut`，HEAD `aca568d`；`origin` 官方、`mirror` 为 ghproxy 部分克隆源 |
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

- `FSY_tut` 比 main 多两个提交，实现了蹲下与攻击：
  - `02724ae` 攻击：C++ 加 `AttackAction` / `AttackMontage` 两个 `UPROPERTY`；`SetupPlayerInputComponent` 里 `BindAction(AttackAction, Started, &DoAttack)`（`.cpp:77`）；`DoAttack()` → `Montage_Play`。
  - `aca568d` 蹲下：C++ 构造函数开 `bCanCrouch`、`CrouchedHalfHeight=40`、`MaxWalkSpeedCrouched=150`；蓝图 `IA_Crouch` 事件节点 → `Crouch()/UnCrouch()`。
- 资产：`Content/Myplayer/` 有 `BP_MyplayerCharacter`、`BP_MyplayerGameMode`、`IA_Crouch`、`AM_MyComboAttack`。后者与模板 `Variant_Combat/Anims/AM_ComboAttack` 几乎一致（同 `MM_Attack_01/02/03` + `SK_Mannequin`），内含 `AnimNotify_DoAttackTrace` 与 `AnimNotify_CheckCombo`；`IMC_Default` 已加 `IA_Crouch` + `IA_ComboAttack`。
- 蓝图 `Touch Input for First Person Movement` 是 `EdGraphNode_Comment` 注释框，内容为**移动端触控编排**（`BPI_TouchInterface_C` 虚拟摇杆）：`Touch Jump Start/End`、`CallFunc_DoMove_*`/`DoLook_*`、`DoJumpStart/End`、`Crouch/UnCrouch`。蓝图里唯一的 EnhancedInput 事件节点是 `InpActEvt_IA_Crouch_...`；**PC 键鼠 5 个输入全绑在 C++**。
- 自建练习文件 `Source/LogSkyline/DummyCharacter.h/.cpp`（未跟踪），与主线独立，暂不用管。

## 4. 教学进度（接手从这里继续）

- ✅ **第 1 步完成**：`BP_MyplayerCharacter` 的 `AttackMontage` 已从 `AM_ComboAttack` 换成 `AM_MyComboAttack`（uasset 有改动）。
- ✅ **第 2 步已改并编译成功（16:16）**：
  - `LogSkylineCharacter.h`：`DoAttack()` 加 `UFUNCTION(BlueprintCallable, Category="Combat")`；加普通成员 `bool bIsAttacking = false;`
  - `LogSkylineCharacter.cpp` 的 `DoAttack()`：状态守卫 → `IsValid(AttackMontage)`（带 Warning 日志）→ `IsValid(AnimInstance)` → `Montage_Play` 返回 `Duration`，`<=0` 直接返回 → `FOnMontageEnded::BindLambda` + `Montage_SetEndDelegate` 播完/打断时复位状态（不用 Tick、不用定时器）。
  - 依赖已核：`Animation/AnimInstance.h`（`FOnMontageEnded` 定义处）、`LogSkyline.h`（`LogLogSkyline`）。
  - **DLL = 16:16 / 796160 B**（旧 00:17 / 793600 B），PDB 69 MB；`grep -a` 已确认含 `Combat|Input`、`Combat|Animation`、`BP_OnAttackStarted`、`On Attack Started`、`DoAttack`、`AttackAction`、`AttackMontage` → 反射结构已进引擎。UBT `Result: Succeeded`。
  - 编译途中两个坑（详见 §2）：16:03 VS F7 被 Live Coding 互斥锁拦下（UHT 跑了但没链接）；16:14 命令行编译被 UBA 权限问题搞坏 DLL/PDB/lib，**清理 0 字节产物后加 `-NoUBA` 重编成功**。
  - 待用户验证：蓝图事件图表右键搜到 `Do Attack`；PIE 连按攻击不重播；清空 `AttackMontage` 后按攻击 Output Log 出现 `Dummy: AttackMontage 未指定`。
  - 还原注意：`git checkout -- Source/LogSkyline/LogSkylineCharacter.h Source/LogSkyline/LogSkylineCharacter.cpp` 会**同时撤销第 2 步与 13:54 结构改造**（两者都未提交）；只想撤结构改造需手动删除新增行，**撤完必须重新编译**（`-NoUBA` 增量编译只需数秒）。
- ✅ **13:54 结构改造第 1 步（C++ 侧一次改完，已随 16:16 编译生效）**：① `AttackAction` → `UPROPERTY(EditDefaultsOnly, Category="Combat|Input")`、`AttackMontage` → `Category="Combat|Animation"`（收紧为类默认值可配）；② 新增 `UFUNCTION(BlueprintImplementableEvent, Category="Combat", meta=(DisplayName="On Attack Started")) void BP_OnAttackStarted();`（**C++ 只触发，禁止在 .cpp 写函数体**，UHT 生成 thunk）；③ `DoAttack()` 在 `bIsAttacking = true` 后调用它，结束委托逻辑顺延为第 5 步。
  - 意图：补齐「表现反馈」层与「编辑器可配」命名规范，让 C++ 侧结构一次性到位，只编译/重启一次。**此钩子非功能必需**（用户 16:10 明确觉得绕，已解释可随时删除，删掉不影响第 3 步）。
  - 后续步骤：**第 2 步** 重开 `.uproject` 后四验证点（Combat 分类、右键有 `Do Attack`、蓝图「添加事件 → On Attack Started」、接 Print String 后 PIE 出现文字）；**第 3 步** 蓝图加 `Touch Attack` 额外触发源 → `Do Attack`、把 Print String 换成音效/特效（不需再编译 C++）。
- ⏸ **第 3 步**：让攻击打得到人。`ALogSkylineCharacter` 实现 `ICombatAttacker`（`, public ICombatAttacker` + 三个纯虚函数），`DoAttackTrace` 做球形检测 → `Cast<ICombatDamageable>` → `ApplyDamage`；蒙太奇已自带通知，不用新建。范例：`Variant_Combat/CombatCharacter.cpp`、`AnimNotify_DoAttackTrace.cpp`、`CombatDummy.h`。靶子可用 `Lvl_Combat` 的假人。
- ⏸ **第 4 步（以后）**：`FGameplayTag` 管攻击中/可连击状态；多人则蒙太奇走服务器 RPC / GAS。

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
- 接口概念（用户经「点名 / 能力登记卡」类比理解）：`ICombatAttacker` 是登记表；`Cast` 是判断门槛，接口还提供**统一函数名与参数**作为调用入口 + 纯虚函数保证有实现；外部「写」接口，「跑」的是对象自带虚函数表里对应类的函数体。`AnimNotify_DoAttackTrace.cpp` → `Cast<ICombatAttacker>(MeshComp->GetOwner())` → `DoAttackTrace()`；配对 `ICombatDamageable` → `ACombatDummy::ApplyDamage`。**`ALogSkylineCharacter` 尚未实现接口**，蒙太奇里两类通知目前静默失效。

## 6. 工作区状态与还原

- 常见 `git status`：`M LogSkyline.uproject`、`M Content/Myplayer/BP_MyplayerCharacter.uasset`、`M Source/LogSkyline/LogSkylineCharacter.h/.cpp`；未跟踪 `.claudiaideconfig`、`DummyCharacter.h/.cpp`。
- 原 `.sln` / `.slnx` 备份在 `%TEMP%\LogSkyline.sln.bak-20261114` / `.slnx.bak-20261114`；去掉 `-NoPrograms` 重跑 UBT 可还原完整方案。
- 本文件是**本地未跟踪文件**：`git pull` 不会动它；但 `git clean -fdx` 会删。若希望另一台机器也拿到，需要 `git add` + `commit` + `push`。

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
- **「为什么要绕一个 BP_OnAttackStarted」（16:10 教学，用户明确觉得绕）**：这一步**不是功能需求**，是主动补的未来准备；删掉 `.cpp:177` 与 `.h:84-86` 声明，攻击/动画/防连按行为完全不变。外部仍然是**直接调 `DoAttack()`**，钩子不在调用链上，而是主链内部向蓝图广播的**旁支**（方向 C++ → 蓝图），蓝图未实现时是 no-op，所以现在这一嗓子没有任何可见效果。价值只在将来加音效/特效时：蓝图「添加事件 → On Attack Started」接一下即可（编译蓝图，秒级），否则要改 C++ → 关编辑器全量编译 → 重开，且把具体音效资源焊死在代码里（C++ 不该知道播哪个音效）。**用户若只关注功能与第 3 步，可以现在删掉它**；趁 13:55 改动尚未编译成功，删掉不会多一次全量编译。
- **「为什么音效走钩子、动画不走」（16:26 教学）**：答案不是「不能复用」——C++ 里并排写一行 `PlaySound` 完全能编译运行；差别在于 `Montage_Play` 那条链是**双向且参与状态机**的：`Duration` 返回值判断播放成败（`.cpp:168-172`）、`Montage_SetEndDelegate` 解锁 `bIsAttacking`（`.cpp:180-185`）、蒙太奇时间轴上的 `AnimNotify_DoAttackTrace` / `AnimNotify_CheckCombo` 驱动第 3 步命中与连击 → 蒙太奇是**攻击的计时器和逻辑时间轴**，不只是表现。而 `BP_OnAttackStarted` 是**单向广播、无返回值**，把蒙太奇挪过去 C++ 就拿不到 Duration、挂不上结束委托，状态永远不解锁。判断口诀：**「它会不会影响判定、时长、状态？」会 → C++ 内部直接调；不会 → 广播给蓝图**。钩子也不是「音效专用接口」，而是「一次真实攻击开始了」这个事实的广播，音效/特效/震屏/UI/成就都是消费者。反过来，纯装饰不参与判定的动画（披风、拖尾）用钩子在蓝图播更合适——分界是「参与规则 vs 纯表现」，不是「动画 vs 音效」。
- **Montage 的心智模型（16:46 教学，用户自述「之前可能理解错了」）**：`UAnimMontage` **不是动画**，是「分镜脚本 + 时间轴」；动画数据是 `AnimSequence`（`MM_Attack_01/02/03`）。四层结构：`SlotAnimTracks` / `DefaultSlot`（插播通道，叠加在 AnimBP 状态机输出之上，播完自动退回，状态机无需为攻击加状态）、`AnimSegments`（胶片顺序 / `AnimStartTime` / `AnimPlayRate`）、`CompositeSections` + `NextSectionName`（可跳章节，模板 `CheckCombo()` 用 `Montage_JumpToSection(ComboSectionNames[ComboCount], ...)`，输入窗口 0.45s）、`AnimNotifyTracks`（时间轴打点：`AnimNotify_DoAttackTrace` → `Cast<ICombatAttacker>` → `DoAttackTrace`；`AnimNotify_CheckCombo` → `CheckCombo`）；另有 `BlendIn/Out`。资产绑定 `SK_Mannequin` 骨架。**意义 = 把「时间」变成可编程对象**：代码 `Montage_Play` 主动启动并拿回 `MontageLength`，`Montage_SetEndDelegate` 拿结束/打断通知，Section 让代码跳段，Notify 让动画在指定帧反过来叫代码。所以换 Montage 不只是换画面，连攻击时长、命中帧、连击窗口、结束时机一起换。常见误解：① 以为是一段动画 ② 以为由 AnimBP 状态机决定播放 ③ 以为是纯表现、与逻辑无关 ④ 以为播完代码不用管。**本项目落差**：`AM_MyComboAttack` 是完整答案（三段 + Section + 两类 Notify），但 `DoAttack()` 只 `Montage_Play` 从头播到尾，Section 用不上；角色未实现 `ICombatAttacker`，Notify 触发也静默失败 → 第 3 步要补。
- 排查口诀：`grep -rn "UFUNCTION(Blueprint" Source/LogSkyline/LogSkylineCharacter.h` 看哪些函数允许蓝图调用；`grep -rn "BindAction" Source/` 看键鼠触发源；蓝图 `Touch Input` 注释框看触控触发源。
- **事件图表里混了两类节点（13:49 核实）**：`Touch Input for First Person Movement` 注释框**内** = 触控/移动端（`BPI_TouchInterface_C` 虚拟摇杆、`Touch Jump Start/End`、调用 `DoMove`/`DoLook`/`DoJumpStart`），PC 上没有虚拟摇杆 UI 时不会触发；注释框**外**唯一的 EnhancedInput 事件节点 `InpActEvt_IA_Crouch_K2Node_EnhancedInputActionEvent` = **标准 Enhanced Input，PC 端也走这里**（`IA_Crouch` 在 `IMC_Default` 有键位映射）。C++ 里**没有** `CrouchAction` 属性、也没有 `BindAction(CrouchAction,...)`，所以蹲下是唯一完全靠蓝图事件节点驱动的 PC 输入；其余 5 个键鼠输入（Jump/Move/Look/MouseLook/Attack）都在 C++ `SetupPlayerInputComponent`。PIE 用键盘时实际走「C++ 那 5 个 + 蓝图 `IA_Crouch` 事件」，两套入口最终汇到同一批 C++ 函数。
