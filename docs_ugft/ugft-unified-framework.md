# UGFT 文档索引（docs_ugft）

> 说明：本目录下的 **UGFT（Unified Gauge Field Theory，统一规范场论）** 是一种**自定义/探索性**的统一规范场论叙述（围绕标架场 `e^a_μ`、自旋联络 `ω^{ab}_μ`、以及形如 `PG(1,3)=SP(1,3)⋊W^{1,3}` 的结构写法）。它与学术界常见的 **TGFT（Tensorial Group Field Theory，张量化群场论）** 并非同一脉络——后者由 Oriti、Rivasseau 等人推动，基于群流形上的张量化场。详见 `ugft-theory.md`。

## 1. 推荐阅读顺序

1. `ugft-mathematical-foundations.md`：数学与几何底座（流形/纤维丛/联络/曲率/协变导数）。
2. `ugft-general-relativity.md`：以标架/联络表述推导广义相对论关键方程。
3. `ugft-standard-model.md`：标准模型的规范群与拉氏量结构（按本文档体系叙述）。
4. `ugft-quantum-mechanics.md`：量子化、非相对论极限与若干基本结果（按本文档体系叙述）。
5. `ugft-statistical-mechanics.md`：有限温度与配分函数等（按本文档体系叙述）。
6. `ugft-compatibility-verification.md`：把上述结论按“已知实验/定律”做对照清单。

可选：
- `becgravity.md`：一个与凝聚态/量子湍流相关的原型设想。

## 2. 文件清单（每篇一句话）

- `ugft-mathematical-foundations.md`：微分几何、纤维丛与规范场的最小工具箱。
- `ugft-general-relativity.md`：从标架场构造度规、无挠条件、爱因斯坦方程与弱场极限。
- `ugft-standard-model.md`：把非引力相互作用按规范场结构组织起来的推导式写法。
- `ugft-quantum-mechanics.md`：路径积分/正则量子化的入口，以及若干经典方程的回收。
- `ugft-statistical-mechanics.md`：欧几里得化、有限温度场论与热力学量的导出。
- `ugft-compatibility-verification.md`：以“检验项/观测值/预测/兼容性”形式列核对表。
- `becgravity.md`：围绕 BEC 量子湍流触发相变与“引力子模”的工程化设想（探索性）。
- `ugft-theory.md`：术语与范围提示（避免与主流 TGFT 混淆）。

## 3. 记号与约定（仅保留最常用）

- **标架与度规**：`g_{μν} = e^a_μ e^b_ν η_{ab}`。
- **联络与曲率**：自旋联络 `ω^{ab}_μ`、曲率 `R^{ab}_{μν}`；坐标联络 `Γ^ρ_{μν}`。
- **"兼容/推导"用语**：本文档以"能在相应极限/假设下回收到熟知方程"为主线组织；并不等同于对主流文献中 TGFT（Tensorial GFT）全部结果的覆盖。
