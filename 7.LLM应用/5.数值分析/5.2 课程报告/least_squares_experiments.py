# -*- coding: utf-8 -*-
"""
课程报告《最小二乘法及其应用》配套数值实验
实验一：一元线性拟合（模拟房价-面积数据）
实验二：不同次数多项式拟合比较（含法方程条件数分析）
实验三：高次插值与低次最小二乘拟合的对比（含噪声数据）
实验四：指数模型的线性化最小二乘拟合（模拟细菌生长数据）
实验五：勒让德正交基与幂基的法方程条件数对比

核心算法（法方程构造、列主元高斯消元、牛顿插值）均为手写实现，
numpy 仅用于数组运算与条件数计算，matplotlib 用于绘图。
"""
import json
import math
from pathlib import Path

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import font_manager

# ---------- 中文字体 ----------
_available = {f.name for f in font_manager.fontManager.ttflist}
for _cand in ["PingFang SC", "Hiragino Sans GB", "Arial Unicode MS",
              "Heiti TC", "STHeiti", "Songti SC"]:
    if _cand in _available:
        plt.rcParams["font.sans-serif"] = [_cand]
        break
plt.rcParams["axes.unicode_minus"] = False

HERE = Path(__file__).resolve().parent
FIG = HERE / "figures"
FIG.mkdir(exist_ok=True)
rng = np.random.default_rng(42)
results = {}


# ---------- 核心算法（手写实现） ----------
def gauss_solve(A, b):
    """列主元高斯消元法求解 Ax = b"""
    A = np.array(A, dtype=float, copy=True)
    b = np.array(b, dtype=float, copy=True)
    n = b.size
    for k in range(n - 1):
        p = k + int(np.argmax(np.abs(A[k:, k])))          # 选列主元
        if abs(A[p, k]) < 1e-300:
            raise ZeroDivisionError("主元过小，方程组接近奇异")
        if p != k:
            A[[k, p]] = A[[p, k]]
            b[[k, p]] = b[[p, k]]
        for i in range(k + 1, n):                          # 消元
            m = A[i, k] / A[k, k]
            A[i, k:] -= m * A[k, k:]
            b[i] -= m * b[k]
    x = np.zeros(n)
    for i in range(n - 1, -1, -1):                         # 回代
        x[i] = (b[i] - A[i, i + 1:] @ x[i + 1:]) / A[i, i]
    return x


def normal_equations(x, y, m):
    """构造 m 次多项式拟合的法方程 G a = d"""
    s = [np.sum(x ** p) for p in range(2 * m + 1)]
    G = np.array([[s[j + k] for k in range(m + 1)] for j in range(m + 1)])
    d = np.array([np.sum(y * x ** j) for j in range(m + 1)])
    return G, d


def ls_polyfit(x, y, m):
    """最小二乘多项式拟合：返回系数 a[0..m] 与法方程系数矩阵 G"""
    G, d = normal_equations(x, y, m)
    a = gauss_solve(G, d)
    return a, G


def polyval(a, x):
    """多项式求值 p(x) = a0 + a1 x + ... + am x^m"""
    r = np.zeros_like(np.asarray(x, dtype=float))
    for c in reversed(a):
        r = r * x + c
    return r


def newton_coeffs(xs, ys):
    """牛顿插值：差商表对角线元素"""
    c = np.array(ys, dtype=float, copy=True)
    for j in range(1, len(xs)):
        c[j:] = (c[j:] - c[j - 1:-1]) / (xs[j:] - xs[:-j])
    return c


def newton_eval(c, xs, x):
    """牛顿插值多项式求值（秦九韶形式）"""
    r = np.full_like(np.asarray(x, dtype=float), c[-1])
    for k in range(len(c) - 2, -1, -1):
        r = r * (x - xs[k]) + c[k]
    return r


def fit_metrics(y, y_hat):
    res = y - y_hat
    mse = float(np.mean(res ** 2))
    ss_tot = float(np.sum((y - np.mean(y)) ** 2))
    r2 = 1.0 - float(np.sum(res ** 2)) / ss_tot
    return mse, math.sqrt(mse), r2


# ---------- 实验一：一元线性拟合（模拟房价数据） ----------
area = np.array([55, 62, 70, 78, 85, 90, 98, 105, 112, 120,
                 128, 135, 142, 150, 158], dtype=float)
price = np.round(0.82 * area + 12 + rng.normal(0, 7, area.size), 1)

a1, G1 = ls_polyfit(area, price, 1)
pred1 = polyval(a1, area)
mse1, rmse1, r2_1 = fit_metrics(price, pred1)

results["exp1"] = {
    "area": area.tolist(),
    "price": price.tolist(),
    "normal_matrix": G1.tolist(),
    "rhs": normal_equations(area, price, 1)[1].tolist(),
    "coef": [round(v, 4) for v in a1],
    "mse": round(mse1, 3),
    "rmse": round(rmse1, 3),
    "r2": round(r2_1, 4),
}

plt.figure(figsize=(7, 4.5))
plt.scatter(area, price, c="#d62728", zorder=3, label="样本点（模拟数据）")
xs = np.linspace(50, 165, 200)
plt.plot(xs, polyval(a1, xs), "b-",
         label=f"拟合直线 y = {a1[0]:.3f} + {a1[1]:.3f}x")
for xi, yi, pi in zip(area, price, pred1):
    plt.plot([xi, xi], [yi, pi], "gray", lw=0.8, alpha=0.7)
plt.xlabel("面积 (m²)")
plt.ylabel("价格 (万元)")
plt.title("实验一：房价-面积数据的最小二乘线性拟合")
plt.legend()
plt.grid(alpha=0.3)
plt.tight_layout()
plt.savefig(FIG / "fig1_linear_fit.png", dpi=200)
plt.close()

# ---------- 实验二：不同次数多项式拟合比较 ----------
def f2(x):
    return np.sin(2 * x) + 0.5 * x

x2 = np.linspace(0, 3, 30)
y2 = f2(x2) + rng.normal(0, 0.15, x2.size)
dense2 = np.linspace(0, 3, 600)

degree_stats = []
fits = {}
for m in range(1, 9):
    a, G = ls_polyfit(x2, y2, m)
    fits[m] = a
    train_mse = float(np.mean((y2 - polyval(a, x2)) ** 2))
    true_rmse = float(np.sqrt(np.mean((f2(dense2) - polyval(a, dense2)) ** 2)))
    degree_stats.append({
        "degree": m,
        "train_mse": round(train_mse, 5),
        "true_rmse": round(true_rmse, 5),
        "cond_G": float(f"{np.linalg.cond(G):.3e}"),
    })
results["exp2"] = {"noise_sigma": 0.15, "n_samples": 30,
                   "stats": degree_stats}

fig, axes = plt.subplots(2, 2, figsize=(10, 7), sharex=True, sharey=True)
for ax, m in zip(axes.ravel(), [1, 3, 5, 8]):
    ax.scatter(x2, y2, s=14, c="#7f7f7f", alpha=0.8, label="含噪样本")
    ax.plot(dense2, f2(dense2), "g--", lw=1.2, label="真实函数")
    ax.plot(dense2, polyval(fits[m], dense2), "b-", lw=1.5,
            label=f"{m} 次拟合")
    st = next(s for s in degree_stats if s["degree"] == m)
    ax.set_title(f"m = {m}（训练 MSE = {st['train_mse']:.4f}）")
    ax.grid(alpha=0.3)
    ax.legend(fontsize=8)
fig.suptitle("实验二：不同次数多项式的最小二乘拟合效果")
fig.tight_layout()
fig.savefig(FIG / "fig2_poly_degrees.png", dpi=200)
plt.close(fig)

plt.figure(figsize=(7, 4.5))
ds = [s["degree"] for s in degree_stats]
plt.semilogy(ds, [s["train_mse"] for s in degree_stats],
             "o-", label="训练均方误差 MSE")
plt.semilogy(ds, [s["true_rmse"] ** 2 for s in degree_stats],
             "s--", label="对真实函数的均方误差")
plt.xlabel("多项式次数 m")
plt.ylabel("误差（对数坐标）")
plt.title("实验二：拟合误差随多项式次数的变化")
plt.legend()
plt.grid(alpha=0.3, which="both")
plt.tight_layout()
plt.savefig(FIG / "fig2b_error_vs_degree.png", dpi=200)
plt.close()

# ---------- 实验三：高次插值 vs 低次最小二乘 ----------
def f3(x):
    return np.sin(x)

x3 = np.linspace(0, 2 * np.pi, 13)          # 13 个等距节点
y3 = f3(x3) + rng.normal(0, 0.08, x3.size)  # 加入观测噪声
dense3 = np.linspace(0, 2 * np.pi, 800)

c_newton = newton_coeffs(x3, y3)            # 12 次牛顿插值
interp_dense = newton_eval(c_newton, x3, dense3)

a3, G3 = ls_polyfit(x3, y3, 5)              # 5 次最小二乘拟合
ls_dense = polyval(a3, dense3)

truth = f3(dense3)
results["exp3"] = {
    "n_nodes": 13, "noise_sigma": 0.08,
    "interp_degree": 12, "ls_degree": 5,
    "interp_rmse": round(float(np.sqrt(np.mean((interp_dense - truth) ** 2))), 4),
    "interp_maxerr": round(float(np.max(np.abs(interp_dense - truth))), 4),
    "ls_rmse": round(float(np.sqrt(np.mean((ls_dense - truth) ** 2))), 4),
    "ls_maxerr": round(float(np.max(np.abs(ls_dense - truth))), 4),
    "ls_coef": [round(v, 5) for v in a3],
}

plt.figure(figsize=(8, 5))
plt.plot(dense3, truth, "g--", lw=1.2, label="真实函数 sin(x)")
plt.scatter(x3, y3, c="#d62728", zorder=3, label="含噪观测点")
plt.plot(dense3, interp_dense, "-", c="#ff7f0e", lw=1.4, label="12 次牛顿插值")
plt.plot(dense3, ls_dense, "b-", lw=1.6, label="5 次最小二乘拟合")
plt.xlabel("x")
plt.ylabel("y")
plt.title("实验三：高次插值与低次最小二乘拟合的对比")
plt.legend()
plt.grid(alpha=0.3)
plt.tight_layout()
plt.savefig(FIG / "fig3_interp_vs_ls.png", dpi=200)
plt.close()

# ---------- 实验四：指数模型的线性化最小二乘拟合 ----------
# 模型 y = a * exp(b x)，两边取对数化为 ln y = ln a + b x 的线性拟合
t4 = np.arange(0, 16, dtype=float)                    # 0~15 小时，16 个观测
y4 = np.round(3.6 * np.exp(0.24 * t4)
              * np.exp(rng.normal(0, 0.04, t4.size)), 2)

z4 = np.log(y4)
coef4, G4 = ls_polyfit(t4, z4, 1)                     # 对 ln y 作线性拟合
a4_fit = math.exp(coef4[0])
b4_fit = coef4[1]
exp_pred = a4_fit * np.exp(b4_fit * t4)
exp_rmse = float(np.sqrt(np.mean((exp_pred - y4) ** 2)))

p4, _ = ls_polyfit(t4, y4, 2)                         # 对照：二次多项式直接拟合
poly_pred = polyval(p4, t4)
poly_rmse = float(np.sqrt(np.mean((poly_pred - y4) ** 2)))

t_extra = 18.0                                        # 外推 3 小时
true_extra = 3.6 * math.exp(0.24 * t_extra)
exp_extra = a4_fit * math.exp(b4_fit * t_extra)
poly_extra = float(polyval(p4, t_extra))

results["exp4"] = {
    "t": t4.tolist(), "y": y4.tolist(),
    "true_a": 3.6, "true_b": 0.24, "noise_sigma": 0.04,
    "fit_ln_a": round(coef4[0], 4), "fit_a": round(a4_fit, 4),
    "fit_b": round(b4_fit, 4),
    "exp_rmse": round(exp_rmse, 3), "poly2_rmse": round(poly_rmse, 3),
    "poly2_coef": [round(v, 4) for v in p4],
    "extrapolation_t": t_extra,
    "true_extra": round(true_extra, 2),
    "exp_extra": round(exp_extra, 2),
    "exp_extra_relerr": round(abs(exp_extra - true_extra) / true_extra, 4),
    "poly_extra": round(poly_extra, 2),
    "poly_extra_relerr": round(abs(poly_extra - true_extra) / true_extra, 4),
}

fig, (axL, axR) = plt.subplots(1, 2, figsize=(11, 4.5))
dense4 = np.linspace(0, 18.5, 400)
axL.scatter(t4, y4, c="#d62728", zorder=3, label="观测数据")
axL.plot(dense4, a4_fit * np.exp(b4_fit * dense4), "b-", lw=1.6,
         label=f"指数拟合 y = {a4_fit:.3f}·exp({b4_fit:.4f}t)")
axL.plot(dense4, polyval(p4, dense4), "-", c="#ff7f0e", lw=1.4,
         label="二次多项式拟合")
axL.plot(dense4, 3.6 * np.exp(0.24 * dense4), "g--", lw=1.0, label="真实模型")
axL.axvspan(15, 18.5, color="gray", alpha=0.12)
axL.annotate("外推区", xy=(16.5, 20), fontsize=9, color="gray")
axL.set_xlabel("时间 t (小时)")
axL.set_ylabel("菌落数量 (千个)")
axL.set_title("线性坐标：拟合与外推对比")
axL.legend(fontsize=8)
axL.grid(alpha=0.3)
axR.scatter(t4, y4, c="#d62728", zorder=3, label="观测数据")
axR.plot(dense4, a4_fit * np.exp(b4_fit * dense4), "b-", lw=1.6, label="指数拟合")
axR.set_yscale("log")
axR.set_xlabel("时间 t (小时)")
axR.set_ylabel("菌落数量（对数坐标）")
axR.set_title("对数坐标：指数模型化为直线")
axR.legend(fontsize=8)
axR.grid(alpha=0.3, which="both")
fig.suptitle("实验四：指数生长模型的线性化最小二乘拟合")
fig.tight_layout()
fig.savefig(FIG / "fig4_exp_model.png", dpi=200)
plt.close(fig)

# ---------- 实验五：勒让德正交基与幂基的条件数对比 ----------
def legendre_basis(x, m):
    """按三项递推公式生成 P_0(x) ... P_m(x)，返回 n x (m+1) 矩阵"""
    x = np.asarray(x, dtype=float)
    B = np.zeros((x.size, m + 1))
    B[:, 0] = 1.0
    if m >= 1:
        B[:, 1] = x
    for k in range(1, m):
        B[:, k + 1] = ((2 * k + 1) * x * B[:, k] - k * B[:, k - 1]) / (k + 1)
    return B


def ls_fit_basis(x, y, m, basis):
    """在给定基底下作最小二乘拟合，返回系数与法方程系数矩阵"""
    B = basis(x, m)
    G = B.T @ B
    d = B.T @ y
    c = gauss_solve(G, d)
    return c, G


def runge(x):
    return 1.0 / (1.0 + 25.0 * x ** 2)


x5 = np.linspace(-1, 1, 81)                 # 81 个等距采样点（无噪声）
y5 = runge(x5)
dense5 = np.linspace(-1, 1, 1001)
truth5 = runge(dense5)

power_basis = lambda x, m: np.vander(np.asarray(x, float), m + 1,
                                     increasing=True)
basis_stats = []
curves = {}
for m in range(2, 25, 2):
    c_pow, G_pow = ls_fit_basis(x5, y5, m, power_basis)
    fit_pow = power_basis(dense5, m) @ c_pow
    c_leg, G_leg = ls_fit_basis(x5, y5, m, legendre_basis)
    fit_leg = legendre_basis(dense5, m) @ c_leg
    basis_stats.append({
        "degree": m,
        "cond_power": float(f"{np.linalg.cond(G_pow):.3e}"),
        "cond_legendre": float(f"{np.linalg.cond(G_leg):.3e}"),
        "rmse_power": float(f"{np.sqrt(np.mean((fit_pow - truth5) ** 2)):.3e}"),
        "rmse_legendre": float(f"{np.sqrt(np.mean((fit_leg - truth5) ** 2)):.3e}"),
    })
    curves[m] = (fit_pow, fit_leg)
# 系数可信度检验：24 次幂基下，法方程解与对设计矩阵直接作 SVD 最小二乘解的偏差
m_show = 24
A24 = power_basis(x5, m_show)
c24_gauss, _ = ls_fit_basis(x5, y5, m_show, power_basis)
c24_svd = np.linalg.lstsq(A24, y5, rcond=None)[0]
coef_relerr = float(np.linalg.norm(c24_gauss - c24_svd)
                    / np.linalg.norm(c24_svd))
results["exp5"] = {"n_samples": 81, "stats": basis_stats,
                   "coef_relerr_power_deg24": float(f"{coef_relerr:.3e}")}

fig, (axC, axE) = plt.subplots(1, 2, figsize=(11, 4.5))
ds5 = [s["degree"] for s in basis_stats]
axC.semilogy(ds5, [s["cond_power"] for s in basis_stats],
             "o-", label="幂基法方程")
axC.semilogy(ds5, [s["cond_legendre"] for s in basis_stats],
             "s--", label="勒让德基法方程")
axC.axhline(1e16, color="r", ls=":", lw=1, label="双精度精度极限（约 1e16）")
axC.set_xlabel("多项式次数 m")
axC.set_ylabel("法方程条件数（对数坐标）")
axC.set_title("(a) 条件数随次数的增长")
axC.legend(fontsize=8)
axC.grid(alpha=0.3, which="both")
axE.semilogy(ds5, [s["rmse_power"] for s in basis_stats],
             "o-", label="幂基")
axE.semilogy(ds5, [s["rmse_legendre"] for s in basis_stats],
             "s--", label="勒让德基")
axE.set_xlabel("多项式次数 m")
axE.set_ylabel("对真实函数的 RMSE（对数坐标）")
axE.set_title("(b) 拟合精度随次数的变化")
axE.legend(fontsize=8)
axE.grid(alpha=0.3, which="both")
fig.suptitle("实验五：幂基与勒让德基的条件数与拟合精度对比")
fig.tight_layout()
fig.savefig(FIG / "fig5_cond_compare.png", dpi=200)
plt.close(fig)

plt.figure(figsize=(8, 4.5))
plt.plot(dense5, curves[m_show][0] - truth5, "-", c="#ff7f0e", lw=1.2,
         label=f"幂基 {m_show} 次拟合误差")
plt.plot(dense5, curves[m_show][1] - truth5, "b-", lw=1.2,
         label=f"勒让德基 {m_show} 次拟合误差")
plt.axhline(0, color="gray", lw=0.8)
plt.xlabel("x")
plt.ylabel("拟合值减真实值")
plt.title(f"实验五：{m_show} 次拟合的误差曲线对比")
plt.legend(fontsize=9)
plt.grid(alpha=0.3)
plt.tight_layout()
plt.savefig(FIG / "fig6_basis_curves.png", dpi=200)
plt.close()

# ---------- 输出结果 ----------
(HERE / "results.json").write_text(
    json.dumps(results, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(results, ensure_ascii=False, indent=2))
