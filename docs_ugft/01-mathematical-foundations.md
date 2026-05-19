# 数学基础：从微分几何到规范场论

> 本文目标：让具备**本科数学系基础**（多元微积分、线性代数、抽象代数初步、点集拓扑入门）的读者，能够从零开始把 UGFT 所用到的几何与代数工具搭起来。
>
> 阅读建议：每一节都先讲"为什么需要这个工具"，再给定义，再举例，最后说明它在 UGFT 里扮演什么角色。如果某节的形式定义看不下去，可以先抓住直觉和例子，回过头再补严格定义。

---

## 0. 阅读前提

| 你应当熟悉 | 用到的部分 |
|---|---|
| 多元微积分 | 偏导数、链式法则、梯度、雅可比矩阵 |
| 线性代数 | 向量空间、对偶空间、张量积、内积 |
| 抽象代数 | 群、子群、正规子群、商群、半直积 |
| 拓扑学 | Hausdorff 空间、同胚、可数基 |
| 微分方程 | 一阶 ODE 的几何意义（向量场） |

**不要求**预先知道：流形、纤维丛、Lie 代数、规范理论、Cartan 形式。本文从头建立。

---

## 1. 流形：把"曲面"推广到任意维

### 1.1 为什么需要

物理学的舞台不止是 $\mathbb{R}^4$ ——时空可能整体弯曲（黑洞、宇宙学），但**局部看起来**仍像平直的 $\mathbb{R}^4$ 。流形就是抓住"局部欧氏、整体可弯曲"这一性质的对象。

### 1.2 定义

**$n$ 维（光滑）流形** $M$ 是一个 Hausdorff、具有可数基的拓扑空间，且配有一族开集 $\{U_\alpha\}$ 覆盖 $M$ ，每个 $U_\alpha$ 都同胚于 $\mathbb{R}^n$ 的开集，且两个覆盖块之间的过渡映射是光滑的（ $C^\infty$ ）。

**直观**：用一堆"地图块"拼出整张地图，每张地图都是 $\mathbb{R}^n$ 的一片，相邻地图块的拼接处光滑相容。

### 1.3 例子

- $\mathbb{R}^n$ 本身——平凡流形。
- 球面 $S^2$ ——用"南北极投影"两张地图就能覆盖。
- 圆环面 $T^2 = S^1 \times S^1$ 。
- **时空** $M$ ——4 维伪黎曼流形，配备洛伦兹度规（见 §2）。

### 1.4 在 UGFT 中

时空 $M$ 是 4 维光滑流形。UGFT 不假设它整体上是 $\mathbb{R}^4$ ，但要求**每一点附近都能用坐标 $x^\mu$ 描述**（ $\mu = 0,1,2,3$ ）。

---

## 2. 切空间与切丛：曲面上的"方向"

### 2.1 为什么需要

在 $\mathbb{R}^n$ 中讨论"方向"很简单（向量就是有序数组）。但在弯曲流形上，"过 $p$ 点沿方向 $v$ 走"必须先讲清楚 $v$ 是哪个空间里的元素。**切空间** $T_p M$ 就是 $p$ 处的"局部方向空间"。

### 2.2 定义

$p \in M$ 处的**切空间** $T_p M$ 是一个 $n$ 维实向量空间，可等价地定义为：

- **几何定义**：过 $p$ 点的光滑曲线 $\gamma(t)$ 在 $t=0$ 处速度向量的集合（模等价）。
- **代数定义**：在 $p$ 点取值的导子（满足 Leibniz 法则的线性算子）。

选定局部坐标 $x^\mu$ 后， $T_p M$ 的一组自然基是

$$
\left\{ \frac{\partial}{\partial x^\mu} \right\}_{\mu=0}^{n-1}.
$$

任何切向量可写成 $v = v^\mu \frac{\partial}{\partial x^\mu}$ （Einstein 求和约定：上下相同的指标自动求和）。

### 2.3 余切空间

**余切空间** $T_p^* M = (T_p M)^*$ 是切空间的对偶（即所有线性映射 $T_p M \to \mathbb{R}$ ），其自然基记为 $\{dx^\mu\}$ ，满足

$$
dx^\mu\!\left(\frac{\partial}{\partial x^\nu}\right) = \delta^\mu_\nu.
$$

### 2.4 切丛

把所有点的切空间"打包"成一个新流形：

$$
TM \;=\; \bigsqcup_{p \in M} T_p M,
$$

称为**切丛**。它本身是 $2n$ 维光滑流形（局部上 $TM \approx U \times \mathbb{R}^n$ ）。同理有余切丛 $T^*M$ 。

### 2.5 例子

- $M = S^2$ ：每点切空间是过该点切平面； $TS^2$ 不能整体写成 $S^2 \times \mathbb{R}^2$ （"毛球定理"）。这是**非平凡丛**的标志。
- $M = \mathbb{R}^n$ ：切丛平凡， $T\mathbb{R}^n = \mathbb{R}^n \times \mathbb{R}^n$ 。

### 2.6 在 UGFT 中

每个时空点 $p$ 都有切空间 $T_p M$ 。 UGFT 的**关键创新**是同时在每点放置另一个 4 维向量空间——**局部洛伦兹标架**——并通过"标架场" $e^a_\mu$ 把两者联系起来（见 §4）。

---

## 3. 张量与度规：处理坐标变换的语言

### 3.1 张量是什么

一个 $(r,s)$ 型**张量**是多重线性映射

$$
T:\; \underbrace{T_p^* M \times \cdots \times T_p^* M}_{r \text{ 份}} \times \underbrace{T_p M \times \cdots \times T_p M}_{s \text{ 份}} \;\to\; \mathbb{R}.
$$

在坐标下写作分量形式 $T^{\mu_1 \cdots \mu_r}{}_{\nu_1 \cdots \nu_s}$ ——上指标"逆变"（co-vector slot），下指标"协变"（vector slot）。

**为什么重要**：物理量（速度、电磁场、能动量张量）都是张量；它们在坐标变换下按确定法则变换，物理结论与坐标系无关。

### 3.2 度规张量

**度规** $g$ 是 $(0,2)$ 型对称张量，决定切空间上的内积：

$$
\langle u, v \rangle = g_{\mu\nu} u^\mu v^\nu.
$$

| 类型 | 条件 | 物理对应 |
|---|---|---|
| 黎曼度规 | $g$ 正定 | 经典几何（曲面长度） |
| 伪黎曼度规 | $g$ 不定（有正负本征值） | 时空（区分时间-空间） |
| 洛伦兹度规 | 符号差 $(-,+,+,+)$ | 狭义/广义相对论 |

闵可夫斯基度规 $\eta_{\mu\nu} = \mathrm{diag}(-1,+1,+1,+1)$ 是平直时空的洛伦兹度规。

### 3.3 微分形式（速览）

$p$ 形式是反对称的 $(0,p)$ 型张量。常用工具：

- **外积** $\wedge$ ： $\alpha \wedge \beta = -\beta \wedge \alpha$ （对 1-形式）。
- **外微分** $d$ ：把 $p$ 形式映为 $p{+}1$ 形式，满足 $d^2 = 0$ 。
- **庞加莱引理**：在可缩区域上， $d\alpha = 0 \Leftrightarrow \alpha = d\beta$ 。

后面规范场的"联络"和"曲率"自然出现在 1-形式与 2-形式语言中。

### 3.4 在 UGFT 中

度规 $g_{\mu\nu}$ 不再是基本场，而是从更基本的**标架场** $e^a_\mu$ 派生：

$$
g_{\mu\nu} \;=\; e^a_\mu \, e^b_\nu \, \eta_{ab}.
$$

下一节解释为什么这样做。

---

## 4. 标架场：把局部 Lorentz 标架嫁接到时空

### 4.1 动机

在每个时空点 $p$ ，我们都可以做"自由下落实验"——此时局部物理回到**狭义相对论**，时空在 $p$ 处看起来是闵可夫斯基的。换句话说：

> 每一点 $p$ 都附带一个 4 维向量空间，称为 $p$ 处的**局部洛伦兹标架空间**，其上有固定的闵可夫斯基度规 $\eta_{ab}$ 。

要让物理写出来不依赖于具体坐标，我们需要在**时空切空间** $T_p M$ 与**局部洛伦兹标架空间**之间建立一组同构。这个同构就是标架场。

### 4.2 定义

**标架场** $e^a_\mu(x)$ 是一组 4 个 4-向量（共 16 个分量函数），其中

- $\mu$ 是时空坐标指标（对应 $T_p M$ 的基 $\partial_\mu$ ）；
- $a$ 是局部洛伦兹指标（对应内禀闵可夫斯基空间的基）。

满足正交关系

$$
e^a_\mu \, e^b_\nu \, \eta_{ab} \;=\; g_{\mu\nu}, \qquad
e^a_\mu \, e_a^\nu \;=\; \delta^\nu_\mu, \qquad
e^a_\mu \, e_b^\mu \;=\; \delta^a_b,
$$

其中 $e_a^\mu$ 是 $e^a_\mu$ 的逆，洛伦兹指标用 $\eta_{ab}$ 升降，时空指标用 $g_{\mu\nu}$ 升降。

### 4.3 类比

把每个时空点想成一栋房子， $T_p M$ 是房子的"地基坐标系"， $\eta_{ab}$ 是"标准砖块的尺寸"。标架场 $e^a_\mu$ 就是告诉你**怎样用标准砖块铺出地基**——它本身既能反映地基的弯曲（编码 $g_{\mu\nu}$ ），又始终保留"标准砖块"这一参照系。

### 4.4 为什么 UGFT 用它而不用度规

- **承载费米子**：要把 Dirac 旋量 $\psi$ 放进弯曲时空，需要在每点定义 $\gamma$ 矩阵；这只能通过局部洛伦兹标架完成（ $\eta_{ab}$ 上的 $\gamma$ 矩阵），度规 $g_{\mu\nu}$ 不够用。
- **作为规范势**：标架场可以被理解为 $W^{1,3}$ （手征平移群）的规范连接——这是 UGFT 把引力写成规范理论的关键（§8）。

---

## 5. 联络与协变导数：在弯曲空间怎么求导

### 5.1 问题

在弯曲流形上，"两个不同点的向量"分别住在不同切空间 $T_p M$ 与 $T_q M$ 里，**不能直接相减**——也就不能定义 $\partial v$ 。

需要一个额外结构：告诉你"沿曲线把 $v$ 从 $p$ 平行移到 $q$"是什么意思。这个结构就叫**联络**。

### 5.2 仿射联络

**仿射联络（Christoffel 符号）** $\Gamma^\rho_{\mu\nu}$ 让我们能对张量场求**协变导数**。对矢量场 $V^\nu$ ：

$$
\nabla_\mu V^\nu \;=\; \partial_\mu V^\nu \;+\; \Gamma^\nu_{\mu\rho}\, V^\rho.
$$

补偿项 $\Gamma^\nu_{\mu\rho} V^\rho$ 抵消了"普通偏导"对坐标变换的非张量贡献，使 $\nabla_\mu V^\nu$ 仍是 $(1,1)$ 张量。

**Levi-Civita 联络**：在度规流形上要求 $\nabla g = 0$ 且无挠时， $\Gamma$ 由度规唯一决定：

$$
\Gamma^\rho_{\mu\nu} \;=\; \tfrac{1}{2} g^{\rho\sigma} \left( \partial_\mu g_{\nu\sigma} + \partial_\nu g_{\mu\sigma} - \partial_\sigma g_{\mu\nu} \right).
$$

### 5.3 自旋联络

对于带洛伦兹指标的对象（如旋量 $\psi$ 、标架场 $e^a_\mu$ 中的 $a$ 指标），还需要另一个联络 **自旋联络** $\omega^{ab}_\mu$ ，反对称于 $a,b$ ：

$$
\omega^{ab}_\mu \;=\; -\omega^{ba}_\mu.
$$

它给出旋量的协变导数：

$$
D_\mu \psi \;=\; \partial_\mu \psi \;+\; \tfrac{1}{4} \omega^{ab}_\mu \, \gamma_{ab}\, \psi, \qquad \gamma_{ab} = \tfrac{1}{2}[\gamma_a, \gamma_b].
$$

对带洛伦兹指标的矢量 $V^a$ ：

$$
D_\mu V^a \;=\; \partial_\mu V^a \;+\; \omega^{a}{}_{b\mu}\, V^b.
$$

### 5.4 完全协变导数

把仿射联络（处理 $\mu$ 指标）与自旋联络（处理 $a$ 指标）一起作用到标架场上：

$$
D_\mu e^a_\nu \;=\; \partial_\mu e^a_\nu \;+\; \omega^{a}{}_{b\mu}\, e^b_\nu \;-\; \Gamma^\rho_{\mu\nu}\, e^a_\rho.
$$

### 5.5 例子（球面上的平行移动）

把指向北极的向量从赤道一点 $A$ 沿赤道平行移到 $B$ ，再沿经线移回到出发点；总转角不为零——这正是球面有非平凡**曲率**的标志（见 §6）。

---

## 6. 曲率与挠率：流形"弯了"多少

### 6.1 曲率张量

**黎曼曲率** 衡量"在小回路上平行移动一个向量后转了多少角度"。其分量形式：

$$
R^\rho{}_{\sigma\mu\nu} \;=\; \partial_\mu \Gamma^\rho_{\nu\sigma} - \partial_\nu \Gamma^\rho_{\mu\sigma} + \Gamma^\rho_{\mu\lambda}\, \Gamma^\lambda_{\nu\sigma} - \Gamma^\rho_{\nu\lambda}\, \Gamma^\lambda_{\mu\sigma}.
$$

等价地，作为算子的对易子：

$$
[\nabla_\mu, \nabla_\nu]\, V^\rho \;=\; R^\rho{}_{\sigma\mu\nu}\, V^\sigma.
$$

在标架表述下还有"自旋曲率"

$$
R^{ab}{}_{\mu\nu} \;=\; \partial_\mu \omega^{ab}_\nu - \partial_\nu \omega^{ab}_\mu + \omega^a{}_{c\mu}\, \omega^{cb}{}_\nu - \omega^a{}_{c\nu}\, \omega^{cb}{}_\mu,
$$

二者由标架联系： $R^\rho{}_{\sigma\mu\nu} = e^\rho_a\, e^b_\sigma\, R^{ab}{}_{\mu\nu}$ 。

### 6.2 Ricci 张量、Ricci 标量、Einstein 张量

- **Ricci 张量**： $R_{\mu\nu} = R^\rho{}_{\mu\rho\nu}$ （收缩 1-3 指标）。
- **Ricci 标量**： $R = g^{\mu\nu} R_{\mu\nu}$ 。
- **Einstein 张量**： $G_{\mu\nu} = R_{\mu\nu} - \tfrac{1}{2} g_{\mu\nu} R$ 。 满足 $\nabla^\mu G_{\mu\nu} = 0$ （Bianchi 恒等式的收缩形式）。

### 6.3 挠率

挠率衡量"平行四边形闭不上的程度"。在 UGFT 中作为标架场的"场强"：

$$
T^a{}_{\mu\nu} \;=\; D_\mu e^a_\nu - D_\nu e^a_\mu \;=\; \partial_\mu e^a_\nu - \partial_\nu e^a_\mu + \omega^a{}_{b\mu}\, e^b_\nu - \omega^a{}_{b\nu}\, e^b_\mu.
$$

经典广义相对论假设 $T^a{}_{\mu\nu} = 0$ （无挠条件），由此与自旋联络的反对称性一起唯一定下 Levi-Civita 联络。

### 6.4 例子

- 平面 $\mathbb{R}^2$ ： $R^\rho{}_{\sigma\mu\nu} = 0$ ， $T = 0$ 。
- 球面 $S^2$ （半径 $a$ ）： $R = 2/a^2 > 0$ ， $T = 0$ 。
- Einstein–Cartan 类引力理论：允许 $T \neq 0$ ，挠率耦合到费米子自旋密度。 UGFT 在底层就允许挠率非零。

---

## 7. 群与 Lie 代数：对称性的语言

### 7.1 Lie 群

**Lie 群** 是同时是群和光滑流形的对象，群运算 $(g, h) \mapsto gh^{-1}$ 光滑。例如：

| Lie 群 | 维数 | 物理意义 |
|---|---|---|
| $U(1)$ | 1 | 电磁规范变换、相位旋转 |
| $SU(2)$ | 3 | 弱同位旋 |
| $SU(3)$ | 8 | 色 SU(3) |
| $SO(3)$ | 3 | 三维空间转动 |
| $SO(1,3)$ | 6 | 时空洛伦兹群 |

### 7.2 Lie 代数

Lie 群 $G$ 单位元处的切空间记作 $\mathfrak{g} = T_e G$ ，配上李括号 $[\cdot, \cdot]$ ，称为 $G$ 的 **Lie 代数**。物理上， $\mathfrak{g}$ 的元素就是"无穷小生成元"。

选定一组基 $\{T_A\}$ ，李括号写成

$$
[T_A, T_B] \;=\; f^C{}_{AB}\, T_C,
$$

其中 $f^C{}_{AB}$ 是**结构常数**。

### 7.3 半直积

如果 $N$ 是 $G$ 的正规子群、 $H$ 是另一子群、且 $G = N H$ 、 $N \cap H = \{e\}$ ，则 $G \cong N \rtimes H$ ，称为**半直积**。 UGFT 的规范群

$$
PG(1,3) \;=\; SP(1,3) \;\rtimes\; W^{1,3}
$$

就是非齐次自旋群——把"自旋部分" $SP(1,3)$ 与"平移部分" $W^{1,3}$ 用半直积粘起来，类似 Poincaré 群 $SO(1,3) \rtimes \mathbb{R}^{1,3}$ 的结构。

### 7.4 例：电磁场是 $U(1)$ 规范理论

电磁场来自要求"波函数局部相位 $\psi \to e^{i\alpha(x)} \psi$ "不改变物理。此即 $U(1)$ 局部规范对称性；对应的规范势就是电磁四矢势 $A_\mu$ 。 UGFT 把这套思想推广到 $PG(1,3)$ 。

---

## 8. 纤维丛：让"每点带额外结构"

### 8.1 直觉

许多物理场不止是"时空上的函数"，而是"时空每点带着一个内部空间，场是这个内部空间里的元素"。比如：

- 电磁势 $A_\mu(x)$ — 每点带一个 1 维"相位空间"。
- 标架场 $e^a_\mu(x)$ — 每点带一个 4 维"内禀洛伦兹空间"。
- 旋量场 $\psi(x)$ — 每点带一个 4 维复 Dirac 空间。

**纤维丛** 就是把这种"底空间每点挂一个纤维"的几何对象正式化。

### 8.2 定义

**纤维丛** $(E, M, \pi, F, G)$ 包含：

- 全空间 $E$ （流形）；
- 底空间 $M$ （流形）；
- 投影 $\pi: E \to M$ （光滑、满射）；
- 典型纤维 $F$ （流形）；
- 结构群 $G$ （Lie 群， $G$ 作用于 $F$ ）。

满足**局部平凡性**：每点 $p \in M$ 有邻域 $U$ 使 $\pi^{-1}(U) \cong U \times F$ ；不同邻域的拼接由 $G$ 中元素给出。

### 8.3 主丛与关联丛

- **主丛** $P(M, G)$ ：纤维 $F = G$ ， $G$ 在纤维上自由右作用。规范理论的"几何主战场"。
- **关联丛** $E = P \times_G V$ ：选定 $G$ 在向量空间 $V$ 上的表示后造的丛。物质场 $\psi$ 就是关联丛的截面。
- **伴随丛** $\mathrm{Ad}(P) = P \times_{\mathrm{Ad}} \mathfrak{g}$ ：纤维是 $\mathfrak{g}$ ，规范场强 $F$ 取值于此。

### 8.4 例子

- 平凡丛 $E = M \times F$ ：拼接处都是恒等变换。
- **Möbius 带**：底空间 $S^1$ ，纤维 $[-1,1]$ ， $G = \mathbb{Z}_2$ ；走一圈纤维翻转——典型的非平凡丛。
- **磁单极**：把 Dirac 单极写成 $S^2$ 上 $U(1)$ 主丛的非平凡截面，磁荷恰对应丛的拓扑数（第一陈类）。

### 8.5 在 UGFT 中

UGFT 的核心几何对象是**时空 $M$ 上以 $PG(1,3)$ 为结构群的主丛**。 $e^a_\mu$ 与 $\omega^{ab}_\mu$ 分别是这个主丛上联络的两部分；物质场（旋量、标量、规范玻色子）住在相应的关联丛里。

---

## 9. 主丛上的联络：规范场的真面目

### 9.1 联络一形式

主丛 $P(M, G)$ 上的**联络** $\omega$ 是一个取值于 $\mathfrak{g}$ 的 1-形式

$$
\omega \;=\; \omega^A_\mu\, T_A\, dx^\mu,
$$

满足两条几何条件（垂直方向取值规则、水平分布的规范变换）。物理上， $\omega^A_\mu$ 就是**规范势**。

### 9.2 规范变换

在规范变换 $g(x) \in G$ 下，联络按

$$
\omega' \;=\; g^{-1}\, dg \;+\; g^{-1}\, \omega\, g
$$

变换。"非齐次"那一项 $g^{-1} dg$ 保证了协变导数 $D = d + \omega$ 在 $g$ 作用下变成 $D' = g^{-1} D g$ ——这就是"规范不变"的几何根源。

### 9.3 曲率二形式

联络的**曲率** $\Omega$ 是一个 $\mathfrak{g}$ -值的 2-形式：

$$
\Omega \;=\; d\omega \;+\; \omega \wedge \omega.
$$

分量形式（结构常数表示）：

$$
\Omega^A{}_{\mu\nu} \;=\; \partial_\mu \omega^A_\nu - \partial_\nu \omega^A_\mu + f^A{}_{BC}\, \omega^B_\mu\, \omega^C_\nu.
$$

物理意义：**曲率 = 规范场强**。

- $U(1)$ 主丛上， $\Omega_{\mu\nu} = F_{\mu\nu}$ 是电磁场强；
- $SU(N)$ 主丛上， $\Omega^A{}_{\mu\nu} = F^A{}_{\mu\nu}$ 是 Yang–Mills 场强；
- UGFT 的主丛上， $\Omega$ 同时含有自旋曲率 $R^{ab}{}_{\mu\nu}$ 与挠率 $T^a{}_{\mu\nu}$ 。

### 9.4 Bianchi 恒等式

由 $d^2 = 0$ ， $\Omega$ 自动满足

$$
D\, \Omega \;\equiv\; d\Omega + \omega \wedge \Omega - \Omega \wedge \omega \;=\; 0.
$$

收缩到张量形式后给出经典的 $\nabla^\mu G_{\mu\nu} = 0$ ，等价于能动量守恒。

---

## 10. UGFT 具体结构

把前 9 节工具汇总到 UGFT。

### 10.1 主丛与结构群

- 底空间： 4 维时空 $M$ 。
- 结构群： $PG(1,3) = SP(1,3) \rtimes W^{1,3}$ 。

### 10.2 两组规范势

主丛联络分解为两部分：

| 联络分量 | 取值于 | 物理对应 | 起的作用 |
|---|---|---|---|
| $\omega^{ab}_\mu$ | $\mathfrak{sp}(1,3) \cong \mathfrak{so}(1,3)$ | 自旋联络 | 旋量协变导数；曲率 $R^{ab}{}_{\mu\nu}$ |
| $e^a_\mu$ | $\mathfrak{w}^{1,3}$ | 标架场 | 度规构造；场强即挠率 $T^a{}_{\mu\nu}$ |

### 10.3 各类场的协变导数

| 场 | 协变导数 |
|---|---|
| 标量 $\varphi$ | $D_\mu \varphi = \partial_\mu \varphi$ |
| 矢量（洛伦兹） $V^a$ | $D_\mu V^a = \partial_\mu V^a + \omega^a{}_{b\mu} V^b$ |
| 矢量（坐标） $V^\nu$ | $\nabla_\mu V^\nu = \partial_\mu V^\nu + \Gamma^\nu_{\mu\rho} V^\rho$ |
| 旋量 $\psi$ | $D_\mu \psi = \partial_\mu \psi + \tfrac{1}{4} \omega^{ab}_\mu \gamma_{ab} \psi$ |
| 规范玻色子 $A^A_\mu$ | $D_\mu A^A_\nu = \partial_\mu A^A_\nu + f^A{}_{BC}\, \omega^B_\mu\, A^C_\nu$ |
| 标架 $e^a_\nu$ | $D_\mu e^a_\nu = \partial_\mu e^a_\nu + \omega^a{}_{b\mu}\, e^b_\nu - \Gamma^\rho_{\mu\nu}\, e^a_\rho$ |

### 10.4 场强与场方程

- 自旋曲率 $R^{ab}{}_{\mu\nu}$ ：自旋规范支的场强。
- 挠率 $T^a{}_{\mu\nu}$ ：手征平移规范支的场强。

对作用量

$$
S_{\mathrm{UGFT}} \;=\; \int d^4x\, e \left[ \tfrac{1}{2\kappa}\, R(e,\omega) - \tfrac{1}{4} F^A_{\mu\nu} F^{A\,\mu\nu} + \bar\psi\, i\gamma^a e_a^\mu D_\mu \psi + \mathcal{L}_{\mathrm{Higgs}} \right]
$$

分别对 $e^a_\mu$ 、 $\omega^{ab}_\mu$ 、 $A^A_\mu$ 、 $\psi$ 变分，得到耦合场方程组（详细推导见 [`02-general-relativity.md`](02-general-relativity.md) 和 [`03-standard-model.md`](03-standard-model.md) ）。

---

## 11. 量子化（速览）

### 11.1 路径积分

$$
Z \;=\; \int \mathcal{D}[e, \omega, A, \psi]\; \exp\!\left( \frac{i}{\hbar} S_{\mathrm{UGFT}} \right).
$$

困难：度规 / 标架积分测度的定义、规范固定（BRST、Faddeev–Popov）、紫外发散。

### 11.2 正则量子化

把场升格为算符，要求等时对易关系——对玻色场用对易子、费米场用反对易子。 UGFT 的可重整性目前仍是开放问题。

### 11.3 半经典极限

把 $\hbar \to 0$ ，路径积分被作用量 $S$ 的鞍点主导，回到经典 UGFT 场方程。继续做"无挠 + 经典化"极限，则归约为广义相对论。

---

## 12. 量子纠缠的 UGFT 表达（核心推导）

> 本节回应 [`07-application-becgravity.md`](07-application-becgravity.md) 中的核心猜想——量子纠缠是 UGFT 相变的"隐藏成分"——并把它**具体到公式层面**：说明纠缠熵、模哈密顿量、纠缠结构如何在 UGFT 的标架/联络变量下被表达。

### 12.1 直觉

量子纠缠刻画"子系统 $A$ 与其补集 $B$ 之间无法分离的关联"。要把它接入 UGFT 这套**几何+规范**语言，关键步骤有三：

1. 用路径积分把量子态写成 UGFT 场配置的泛函；
2. 用复制技巧 (replica trick) 把"纠缠熵"转成"配分函数比值"；
3. 在半经典极限下，配分函数由 UGFT 鞍点（即 $e^a_\mu, \omega^{ab}_\mu$ 的经典解）主导——纠缠熵自动变成**标架场的几何泛函**。

最终得到本节的核心结论：

> $$ \boxed{\; S_A \;=\; \frac{1}{4 G_N} \int_{\gamma_A} \, e^{(d-2)} \;+\; (\text{量子修正}) \;} $$
>
> 其中 $\gamma_A$ 是 UGFT 鞍点几何中以 $\partial A$ 为边界的**极小余维 2 曲面**， $e^{(d-2)}$ 是由标架 $e^a_\mu$ 诱导的体积元。

这就是 Ryu–Takayanagi 公式在 UGFT 形式下的写法——**纠缠熵 = 标架的几何**。下面给出逐步推导。

### 12.2 第一步：UGFT 态的路径积分表示

把量子态 $\lvert \Psi\rangle$ 表示成"在欧氏时间 $\tau < 0$ 的半空间上积分所有 UGFT 场"得到的泛函：

$$
\Psi[\phi_0(\vec x)] \;=\; \int_{\phi(\tau=0)=\phi_0}^{} \mathcal{D}[e, \omega, A, \psi]\; e^{-S_E[\,e, \omega, A, \psi\,]},
$$

其中 $\phi = (e, \omega, A, \psi)$ 简写所有 UGFT 场， $S_E$ 是 Wick 转动后的欧氏作用量。 这把"态"几何化成了"$\tau = 0$ 切片上的边界条件"。

### 12.3 第二步：约化密度矩阵

把空间切成两块 $A \cup B$ 。对 $B$ 区域的自由度求迹得到**约化密度矩阵**：

$$
\rho_A \;=\; \mathrm{Tr}_B\, |\Psi\rangle \langle \Psi|.
$$

在路径积分语言下， $\rho_A$ 等价于在"沿 $B$ 区域粘合上下两片欧氏切片，但 $A$ 区域留两条独立的边界"的几何上做的配分函数。

### 12.4 第三步：复制技巧

直接算 $S_A = -\mathrm{Tr}(\rho_A \log \rho_A)$ 困难。借助**复制公式**：

$$
S_A \;=\; -\lim_{n \to 1} \frac{\partial}{\partial n} \log\, \mathrm{Tr}(\rho_A^n).
$$

而 $\mathrm{Tr}(\rho_A^n)$ 等于在 $n$ 份欧氏切片沿 $A$ 区域**首尾相接成"分支覆盖"**所成几何 $M_n$ 上的配分函数：

$$
\mathrm{Tr}(\rho_A^n) \;=\; \frac{Z[M_n]}{Z[M]^n}.
$$

### 12.5 第四步：UGFT 半经典展开

在 UGFT 中， $Z[M_n]$ 由路径积分给出。半经典展开取作用量鞍点：

$$
Z[M_n] \;\approx\; \exp\!\Big(\!-S_E^{\mathrm{UGFT}}[\,\bar e_n, \bar\omega_n, \cdots]\,\Big),
$$

其中 $\bar e_n, \bar\omega_n$ 是在背景几何 $M_n$ 上 UGFT 场方程的解。

把 $-\partial_n \log Z[M_n]$ 在 $n \to 1$ 处展开（细节是 Lewkowycz–Maldacena 的"重力熵"论证），得到

$$
S_A \;=\; \frac{1}{4 G_N} \int_{\gamma_A} e^{(d-2)}.
$$

这里：

- $\gamma_A$ 是在背景 UGFT 几何中以 $\partial A$ 为边界的**极小余维 2 曲面**（Ryu–Takayanagi 曲面）；
- $e^{(d-2)} = \det(e^a_i \, e^b_j \, \eta_{ab})^{1/2}$ 是由 4 维 UGFT 标架场诱导到 $\gamma_A$ 上的体积元。

**结论**：纠缠熵被表示成**标架场 $e^a_\mu$ 的几何泛函**——它完全在 UGFT 的基本变量内部封闭。

### 12.6 模哈密顿量 = $SP(1,3)$ 生成元

约化密度矩阵的对数

$$
K_A \;=\; -\log \rho_A,
$$

称为**模哈密顿量**。对于稳态、Rindler 楔型分割（Bisognano–Wichmann 定理），它正比于一个**Lorentz 推动 (boost) 生成元**——而 boost 正是 UGFT 自旋规范支 $SP(1,3)$ 的元素。具体而言，

$$
K_A \;=\; 2\pi \int_A d\Sigma^\mu\, \xi^\nu\, T_{\mu\nu} \;+\; \text{常数},
$$

其中 $\xi^\nu$ 是保持 $\partial A$ 不动的 boost Killing 矢量， $T_{\mu\nu}$ 是 UGFT 物质场的能动量张量。

**关键观察**：模哈密顿量整体由一个 $SP(1,3) \subset PG(1,3)$ 元素生成——纠缠的"动力学"完全在 UGFT 规范代数内部。

### 12.7 纠缠 → 几何的反向耦合

上面给出"几何 → 纠缠熵"。物理上更深的反向问题是：**调控纠缠能否反过来调控几何？**

在 UGFT 框架下，对作用量做 $1/N$ 展开后，纠缠源（如 BEC 中预制的 Bell 对密度）以"反作用张量"的形式出现在标架场方程的右端：

$$
\frac{1}{\kappa}\, G^{ab}_\mu(e, \omega) \;=\; T^{ab}_\mu[\psi, A] \;+\; \underbrace{\Sigma^{ab}_\mu[\,\rho_{\mathrm{ent}}\,]}_{\text{纠缠诱导源}}.
$$

其中 $\Sigma^{ab}_\mu$ 是**纠缠流密度**——以约化密度矩阵 $\rho_{\mathrm{ent}}$ 的关联函数（如 $\langle \hat\psi(x) \hat\psi(y) \rangle$ ）做核构造的有效流。这是 [`07-application-becgravity.md`](07-application-becgravity.md) 中"纠缠手性翻转 → 反引力"猜想在场方程层面的具体写照：

> **纠缠拓扑的奇偶性 ⇔ $\Sigma^{ab}_\mu$ 的手性 ⇔ 标架场响应的吸引/排斥极性。**

### 12.8 小结

| 量 | 量子信息语言 | UGFT 几何/规范语言 |
|---|---|---|
| 态 $\|\Psi\rangle$ | 希尔伯特空间向量 | $\tau = 0$ 上场配置的边界泛函 |
| 约化密度矩阵 $\rho_A$ | $\mathrm{Tr}_B \|\Psi\rangle\langle\Psi\|$ | 沿 $B$ 粘合的路径积分 |
| 模哈密顿量 $K_A$ | $-\log \rho_A$ | $SP(1,3)$ boost 生成元的局部投影 |
| 纠缠熵 $S_A$ | $-\mathrm{Tr}(\rho_A \log \rho_A)$ | $\frac{1}{4 G_N} \int_{\gamma_A} e^{(d-2)}$ |
| 纠缠对几何反作用 | 复合算子期望值 | $\Sigma^{ab}_\mu$ 出现在标架场方程右端 |

这套对照表把"量子纠缠"完全装进了 UGFT 的标架场 $e^a_\mu$ 与自旋联络 $\omega^{ab}_\mu$ 语言里。 [`07-application-becgravity.md`](07-application-becgravity.md) 中提出的纠缠相变-引力子模耦合，可视为该对照表在凝聚态实现下的工程化体现。

---

## 13. 附录：记号速查

| 符号 | 类型 | 含义 |
|---|---|---|
| $M$ | 4 维光滑流形 | 时空 |
| $g_{\mu\nu}$ | 张量 $(0,2)$ | 时空度规 |
| $\eta_{ab}$ | $\mathrm{diag}(-1,1,1,1)$ | 局部闵可夫斯基度规 |
| $e^a_\mu$ | 标架场 | $W^{1,3}$ 规范连接 |
| $\omega^{ab}_\mu$ | 自旋联络 | $SP(1,3)$ 规范连接 |
| $\Gamma^\rho_{\mu\nu}$ | 仿射联络 | Levi-Civita（无挠时） |
| $R^\rho{}_{\sigma\mu\nu}$ | 曲率张量 | 黎曼曲率 |
| $R^{ab}{}_{\mu\nu}$ | 自旋曲率 | 标架表述 |
| $T^a{}_{\mu\nu}$ | 挠率 | 标架场的场强 |
| $\mathfrak{g}$ | Lie 代数 | $T_e G$ |
| $\rtimes$ | 半直积 | $PG = SP \rtimes W$ |
| $\wedge$ | 外积 | 形式代数 |
| $d, D, \nabla$ | 外微分 / 协变导数 | 见 §3, §5 |
| 拉丁指标 $a,b,c,\ldots$ | 局部洛伦兹 | 用 $\eta$ 升降 |
| 希腊指标 $\mu,\nu,\rho,\ldots$ | 时空坐标 | 用 $g$ 升降 |

---

> 读完本文后，建议进入 [`02-general-relativity.md`](02-general-relativity.md) 看如何从上述工具复原 Einstein 场方程；或 [`03-standard-model.md`](03-standard-model.md) 看 $SP(1,3)$ 分支如何承载电磁/弱/强相互作用。
