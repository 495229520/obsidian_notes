---
title: 模拟卷 - 线性方程组（LU、平方根法、追赶法）
tags:
  - LLM应用
  - 数值分析
  - 期末试卷
  - 模拟卷
aliases:
  - 线性方程组专项模拟卷
---

# 模拟卷 - 线性方程组（LU、平方根法、追赶法）

> [!info] 出卷范围
> 仅限 [[5.11 线性方程组]] 与 [[5.12 线性方程组平方根法、追赶法]] 两篇笔记。
>
> 覆盖考点：
> - Doolittle 分解与列主元高斯消去
> - 顺序主子式、非奇异、对角占优等常考性质
> - 平方根法 $A=LL^T$
> - 改进平方根法 $A=LDL^T$
> - 追赶法及其适用条件

> [!abstract] 试卷说明
> - 建议限时：`90 分钟`
> - 满分：`100 分`
> - 题型风格参考本目录近年期末卷，但知识点严格压缩到本专题

## 一、填空题（每空 2 分，共 20 分）

1. Doolittle 分解 $A=LU$ 存在且唯一的常用判据是：矩阵 $A$ 的各阶 `______` 均不为 0。

2. 设 $A=LU$，则求解 $AX=b$ 时，通常先解 `______`，再解 `______`。

3. 列主元高斯消去法中，每步应在当前列对角线及以下选取 `______` 的元素作为主元。

4. 若在消去过程中进行了行交换，则矩阵分解形式一般写成 `______`。

5. 对称正定矩阵的平方根分解形式为 `______`。

6. 改进平方根法中，矩阵分解形式为 `______`，其中 $L$ 是 `______` 三角阵。

7. Cholesky 分解中，对角元公式为

$$
l_{jj}=\underline{\qquad\qquad\qquad\qquad}
$$

8. 追赶法中，若三对角矩阵按下对角线、主对角线、上对角线分别记作 $a_i,b_i,c_i$，则

$$
u_i=\underline{\qquad\qquad\qquad\qquad}\qquad(i=2,\dots,n)
$$

9. 若 $A$ 为对称正定矩阵，作 $A=LDL^T$ 分解，则对角阵 $D=\operatorname{diag}(d_1,\dots,d_n)$ 中各 $d_i$ 满足 `______`。

10. 追赶法求解三对角方程组的计算量数量级为 `______`。

## 二、判断题（每题 2 分，共 10 分）

1. 只要矩阵 $A$ 非奇异，就一定能直接进行不换行的 Doolittle 分解。`( )`

2. 在 Cholesky 分解 $A=LL^T$ 中，$L$ 一定是单位下三角阵。`( )`

3. 严格对角占优矩阵可不选主元，直接做 LU 分解。`( )`

4. 追赶法本质上是三对角矩阵上的带状 LU 分解。`( )`

5. 对称矩阵一定可以使用平方根法求解线性方程组。`( )`

## 三、简答题（10 分）

比较下列四种方法的适用矩阵、分解形式与主要特点：

1. Doolittle 分解
2. 平方根法
3. 改进平方根法
4. 追赶法

要求答出：

- 各自适用的矩阵类型
- 各自的分解形式
- 各自最突出的计算特点

## 四、计算题（共 60 分）

### 1. Doolittle 分解法（12 分）

用 Doolittle 分解法解线性方程组 $AX=b$：

$$
A=
\begin{bmatrix}
1 & 2 & 3 \\
2 & 5 & 7 \\
3 & 5 & 3
\end{bmatrix},
\qquad
b=
\begin{bmatrix}
6 \\
14 \\
11
\end{bmatrix}
$$

要求：

1. 写出 $A=LU$ 中的 $L,U$
2. 先解 $LY=b$
3. 再解 $UX=Y$

### 2. 平方根法适用条件（12 分）

设

$$
A=
\begin{bmatrix}
2 & -1 & 0 \\
-1 & 2 & b \\
0 & b & 3
\end{bmatrix}
$$

问参数 $b$ 在什么范围内取值时，可用平方根法求解线性方程组 $AX=f$？说明理由。

### 3. 平方根法求解（12 分）

用平方根法解线性方程组 $AX=b$：

$$
A=
\begin{bmatrix}
4 & 2 & 1 \\
2 & 4 & 1 \\
1 & 1 & 4
\end{bmatrix},
\qquad
b=
\begin{bmatrix}
7 \\
7 \\
6
\end{bmatrix}
$$

要求：

1. 求出下三角阵 $L$
2. 先解 $LY=b$
3. 再解 $L^T X=Y$

### 4. 改进平方根法（12 分）

用改进平方根法将下列矩阵分解为 $A=LDL^T$，并求解 $AX=b$：

$$
A=
\begin{bmatrix}
4 & 2 & 2 \\
2 & 5 & 1 \\
2 & 1 & 3
\end{bmatrix},
\qquad
b=
\begin{bmatrix}
8 \\
8 \\
6
\end{bmatrix}
$$

要求：

1. 写出 $L,D$
2. 依次完成 $LY=b$、$DZ=Y$、$L^T X=Z$

### 5. 追赶法（12 分）

用追赶法解三对角方程组 $AX=d$：

$$
A=
\begin{bmatrix}
2 & -1 & 0 & 0 \\
-1 & 2 & -1 & 0 \\
0 & -1 & 2 & -1 \\
0 & 0 & -1 & 2
\end{bmatrix},
\qquad
d=
\begin{bmatrix}
1 \\
0 \\
0 \\
1
\end{bmatrix}
$$

要求写出：

1. 递推得到的 $l_i,u_i$
2. “追”的过程
3. “赶”的过程

---

> [!success]- 参考答案
>
> ## 一、填空题
>
> 1. `顺序主子式`
> 2. `LY=b`，`UX=Y`
> 3. `绝对值最大`
> 4. `PA=LU`
> 5. `A=LL^T`
> 6. `A=LDL^T`，`单位下`
> 7.
>
> $$
> l_{jj}=\sqrt{a_{jj}-\sum_{k=1}^{j-1}l_{jk}^2}
> $$
>
> 8.
>
> $$
> u_i=b_i-l_i c_{i-1}
> $$
>
> 9. `d_i>0`
> 10. `O(n)`
>
> ## 二、判断题
>
> 1. `×`
> 2. `×`
> 3. `√`
> 4. `√`
> 5. `×`
>
> ## 三、简答题参考要点
>
> - Doolittle：适用于一般非奇异矩阵；分解形式为 $A=LU$；通用，但计算量较大。
> - 平方根法：适用于对称正定矩阵；分解形式为 $A=LL^T$；只需计算一半元素，但含开方。
> - 改进平方根法：适用于对称正定矩阵；分解形式为 $A=LDL^T$；免开方，$L$ 为单位下三角阵。
> - 追赶法：适用于三对角方程组，常要求满足对角占优条件；本质是带状 LU 分解；计算量为 $O(n)$，非常省。
>
> ## 四、计算题
>
> ### 1. Doolittle 分解法
>
> 由 Doolittle 分解得
>
> $$
> L=
> \begin{bmatrix}
> 1 & 0 & 0 \\
> 2 & 1 & 0 \\
> 3 & -1 & 1
> \end{bmatrix},
> \qquad
> U=
> \begin{bmatrix}
> 1 & 2 & 3 \\
> 0 & 1 & 1 \\
> 0 & 0 & -5
> \end{bmatrix}
> $$
>
> 先解 $LY=b$：
>
> $$
> y_1=6,\qquad
> y_2=14-2\times6=2,\qquad
> y_3=11-3\times6-(-1)\times2=-5
> $$
>
> 即
>
> $$
> Y=
> \begin{bmatrix}
> 6 \\
> 2 \\
> -5
> \end{bmatrix}
> $$
>
> 再解 $UX=Y$：
>
> $$
> x_3=\frac{-5}{-5}=1,\qquad
> x_2=\frac{2-1}{1}=1,\qquad
> x_1=6-2-3=1
> $$
>
> $$
> \boxed{
> X=
> \begin{bmatrix}
> 1 \\
> 1 \\
> 1
> \end{bmatrix}}
> $$
>
> ### 2. 平方根法适用条件
>
> 平方根法要求系数矩阵对称正定。
>
> 该矩阵显然对称，只需判定其顺序主子式均大于 0：
>
> $$
> D_1=2>0
> $$
>
> $$
> D_2=
> \begin{vmatrix}
> 2 & -1 \\
> -1 & 2
> \end{vmatrix}
> =3>0
> $$
>
> $$
> D_3=
> \begin{vmatrix}
> 2 & -1 & 0 \\
> -1 & 2 & b \\
> 0 & b & 3
> \end{vmatrix}
> =9-2b^2
> $$
>
> 故
>
> $$
> 9-2b^2>0
> \iff
> b^2<\frac{9}{2}
> \iff
> -\frac{3}{\sqrt2}<b<\frac{3}{\sqrt2}
> $$
>
> $$
> \boxed{-\frac{3}{\sqrt2}<b<\frac{3}{\sqrt2}}
> $$
>
> ### 3. 平方根法求解
>
> 由 Cholesky 分解公式可得
>
> $$
> l_{11}=2,\qquad
> l_{21}=1,\qquad
> l_{31}=\frac12
> $$
>
> $$
> l_{22}=\sqrt{4-1}=\sqrt3,\qquad
> l_{32}=\frac{1-\frac12\cdot1}{\sqrt3}=\frac{\sqrt3}{6}
> $$
>
> $$
> l_{33}=\sqrt{4-\left(\frac12\right)^2-\left(\frac{\sqrt3}{6}\right)^2}
> =\frac{\sqrt{33}}{3}
> $$
>
> 因而
>
> $$
> L=
> \begin{bmatrix}
> 2 & 0 & 0 \\
> 1 & \sqrt3 & 0 \\
> \frac12 & \frac{\sqrt3}{6} & \frac{\sqrt{33}}{3}
> \end{bmatrix}
> $$
>
> 解 $LY=b$：
>
> $$
> y_1=\frac72,\qquad
> y_2=\frac{7}{2\sqrt3},\qquad
> y_3=\frac{\sqrt{33}}{3}
> $$
>
> 解 $L^T X=Y$：
>
> $$
> x_3=1,\qquad x_2=1,\qquad x_1=1
> $$
>
> $$
> \boxed{
> X=
> \begin{bmatrix}
> 1 \\
> 1 \\
> 1
> \end{bmatrix}}
> $$
>
> ### 4. 改进平方根法
>
> 设
>
> $$
> A=LDL^T,\qquad
> L=
> \begin{bmatrix}
> 1 & 0 & 0 \\
> l_{21} & 1 & 0 \\
> l_{31} & l_{32} & 1
> \end{bmatrix},
> \qquad
> D=\operatorname{diag}(d_1,d_2,d_3)
> $$
>
> 计算得
>
> $$
> d_1=4,\qquad
> l_{21}=\frac{2}{4}=\frac12,\qquad
> l_{31}=\frac{2}{4}=\frac12
> $$
>
> $$
> d_2=5-\left(\frac12\right)^2\cdot4=4
> $$
>
> $$
> l_{32}=\frac{1-\frac12\cdot4\cdot\frac12}{4}=0
> $$
>
> $$
> d_3=3-\left(\frac12\right)^2\cdot4-0^2\cdot4=2
> $$
>
> 所以
>
> $$
> L=
> \begin{bmatrix}
> 1 & 0 & 0 \\
> \frac12 & 1 & 0 \\
> \frac12 & 0 & 1
> \end{bmatrix},
> \qquad
> D=
> \begin{bmatrix}
> 4 & 0 & 0 \\
> 0 & 4 & 0 \\
> 0 & 0 & 2
> \end{bmatrix}
> $$
>
> 解 $LY=b$：
>
> $$
> y_1=8,\qquad
> y_2=8-\frac12\cdot8=4,\qquad
> y_3=6-\frac12\cdot8=2
> $$
>
> 解 $DZ=Y$：
>
> $$
> z_1=2,\qquad z_2=1,\qquad z_3=1
> $$
>
> 解 $L^T X=Z$：
>
> $$
> x_3=1,\qquad x_2=1,\qquad x_1+\frac12+\frac12=2
> $$
>
> $$
> x_1=1
> $$
>
> $$
> \boxed{
> X=
> \begin{bmatrix}
> 1 \\
> 1 \\
> 1
> \end{bmatrix}}
> $$
>
> ### 5. 追赶法
>
> 三对角元素为
>
> $$
> a_2=a_3=a_4=-1,\qquad
> b_1=b_2=b_3=b_4=2,\qquad
> c_1=c_2=c_3=-1
> $$
>
> 递推计算：
>
> $$
> u_1=2
> $$
>
> $$
> l_2=\frac{-1}{2}=-\frac12,\qquad
> u_2=2-\left(-\frac12\right)(-1)=\frac32
> $$
>
> $$
> l_3=\frac{-1}{3/2}=-\frac23,\qquad
> u_3=2-\left(-\frac23\right)(-1)=\frac43
> $$
>
> $$
> l_4=\frac{-1}{4/3}=-\frac34,\qquad
> u_4=2-\left(-\frac34\right)(-1)=\frac54
> $$
>
> “追”：
>
> $$
> y_1=1
> $$
>
> $$
> y_2=0-\left(-\frac12\right)\cdot1=\frac12
> $$
>
> $$
> y_3=0-\left(-\frac23\right)\cdot\frac12=\frac13
> $$
>
> $$
> y_4=1-\left(-\frac34\right)\cdot\frac13=\frac54
> $$
>
> “赶”：
>
> $$
> x_4=\frac{5/4}{5/4}=1
> $$
>
> $$
> x_3=\frac{\frac13-(-1)\cdot1}{4/3}=1
> $$
>
> $$
> x_2=\frac{\frac12-(-1)\cdot1}{3/2}=1
> $$
>
> $$
> x_1=\frac{1-(-1)\cdot1}{2}=1
> $$
>
> $$
> \boxed{
> X=
> \begin{bmatrix}
> 1 \\
> 1 \\
> 1 \\
> 1
> \end{bmatrix}}
