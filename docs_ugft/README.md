# UGFT — 统一规范场论 / Unified Gauge Field Theory

> **UGFT** 是本仓库自定义/探索性的**统一规范场论**框架：用同一个非齐次自旋规范群 $PG(1,3) = SP(1,3) \rtimes W^{1,3}$ 把引力、电磁、弱、强四种相互作用放在一起讲。
>
> ⚠️ 它与学术界另一缩写 **TGFT (Tensorial Group Field Theory, Oriti–Rivasseau 等)** **完全不是同一脉络**。详见 [`appendix-tgft-disambiguation.md`](appendix-tgft-disambiguation.md)。

---

## 🚀 从哪里开始？

**如果你只想读一篇**，就读 **[`00-overview.md`](00-overview.md)** —— 单页讲清 UGFT 是什么、核心对象、统一图景、关键公式、与主流 TGFT 的边界。

**如果你想深入推导**，按下面的顺序读。

---

## 📚 推荐阅读顺序

| # | 文件 | 内容 |
|---|---|---|
| **0** | **[`00-overview.md`](00-overview.md)** | **单页入门总览（强烈建议先读）** |
| 1 | [`01-mathematical-foundations.md`](01-mathematical-foundations.md) | 数学底座：流形、纤维丛、联络、曲率、协变导数 |
| 2 | [`02-general-relativity.md`](02-general-relativity.md) | 推导广义相对论：标架构造度规、爱因斯坦场方程、弱场极限 |
| 3 | [`03-standard-model.md`](03-standard-model.md) | 推导标准模型：$SU(3)_C \times SU(2)_L \times U(1)_Y$、对称破缺 |
| 4 | [`04-quantum-mechanics.md`](04-quantum-mechanics.md) | 推导量子力学：路径积分、非相对论极限、薛定谔/狄拉克方程 |
| 5 | [`05-statistical-mechanics.md`](05-statistical-mechanics.md) | 推导统计力学：欧氏化、配分函数、临界现象 |
| 6 | [`06-compatibility-verification.md`](06-compatibility-verification.md) | 实验兼容性核对：理论预测 vs 观测值 |

## 🔧 应用 & 附录

| 文件 | 内容 |
|---|---|
| [`07-application-becgravity.md`](07-application-becgravity.md) | **工程应用**：BEC 量子湍流 + 量子纠缠拓扑 → 引力子模 / 反引力（含新猜想） |
| [`appendix-tgft-disambiguation.md`](appendix-tgft-disambiguation.md) | **附录**：与主流 TGFT (Tensorial Group Field Theory) 的边界划分 |

---

## 📐 记号约定

| 符号 | 含义 |
|---|---|
| $e^a_\mu$ | 标架场（tetrad/vierbein） |
| $\omega^{ab}_\mu$ | 自旋联络（spin connection） |
| $g_{\mu\nu} = e^a_\mu e^b_\nu \eta_{ab}$ | 时空度规由标架构造 |
| $\eta_{ab} = \mathrm{diag}(-1,1,1,1)$ | 闵可夫斯基度规（mostly-plus 约定） |
| $R^{ab}_{\mu\nu}$ | 自旋曲率（标架表述下的黎曼曲率） |
| $\Gamma^\rho_{\mu\nu}$ | 坐标联络（仿射联络） |
| $PG(1,3) = SP(1,3) \rtimes W^{1,3}$ | UGFT 的非齐次自旋规范群 |
| 拉丁指标 $a,b,\dots$ | 局部洛伦兹标架指标 |
| 希腊指标 $\mu,\nu,\dots$ | 时空坐标指标 |

---

## ⚠️ 阅读须知

- 本目录把"推导/兼容"理解为**在相应极限或假设下能回收到已知方程**，而不是对所有现代文献结果的覆盖。
- 全部内容属探索性叙述，不构成对主流物理教材的替代。
- 公式渲染需要 GitHub 原生数学支持（`$...$` 和 `$$...$$`）。如果你 fork 到其他 Markdown 渲染器，可能需要调整。
