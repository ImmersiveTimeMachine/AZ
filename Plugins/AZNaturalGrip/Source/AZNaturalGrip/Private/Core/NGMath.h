// Copyright Artur. AZ project.
// Natural Grip solver core: vectors, quaternions and rigid transforms (double precision, no engine dependency).
// Port of Tools/wgs/natgrip/ql.py. The formulas are copied operation for operation so the C++ results match the Python
// solver bit for bit wherever the CRT math does (parity tests in Tools/ngtest).
#pragma once

#include <cmath>
#include <cstddef>

namespace ng
{
	constexpr double kPi = 3.141592653589793;
	constexpr double kDegToRad = kPi / 180.0;      // CPython math.radians: x * (pi / 180.0)
	constexpr double kRadToDeg = 180.0 / kPi;      // CPython math.degrees: x * (180.0 / pi)

	inline double Radians(double Deg) { return Deg * kDegToRad; }
	inline double Degrees(double Rad) { return Rad * kRadToDeg; }

	/** Python's builtin sum() over floats: CPython >= 3.12 uses Neumaier compensated summation (start value int 0). */
	inline double PyFloatSum(const double* V, size_t Count)
	{
		if (Count == 0)
		{
			return 0.0;
		}
		double Result = 0.0 + V[0];
		double Comp = 0.0;
		for (size_t I = 1; I < Count; ++I)
		{
			const double Item = V[I];
			const double T = Result + Item;
			if (std::fabs(Result) >= std::fabs(Item))
			{
				Comp += (Result - T) + Item;
			}
			else
			{
				Comp += (Item - T) + Result;
			}
			Result = T;
		}
		if (Comp != 0.0 && std::isfinite(Comp))
		{
			Result += Comp;
		}
		return Result;
	}

	struct V3
	{
		double x = 0.0, y = 0.0, z = 0.0;
		V3() = default;
		V3(double X, double Y, double Z) : x(X), y(Y), z(Z) {}
		double operator[](int K) const { return K == 0 ? x : (K == 1 ? y : z); }
		double& operator[](int K) { return K == 0 ? x : (K == 1 ? y : z); }
	};

	inline V3 Add(const V3& A, const V3& B) { return V3(A.x + B.x, A.y + B.y, A.z + B.z); }
	inline V3 Sub(const V3& A, const V3& B) { return V3(A.x - B.x, A.y - B.y, A.z - B.z); }
	inline V3 Mul(const V3& A, double S) { return V3(A.x * S, A.y * S, A.z * S); }
	inline double Dot(const V3& A, const V3& B) { return A.x * B.x + A.y * B.y + A.z * B.z; }
	inline V3 Cross(const V3& A, const V3& B) { return V3(A.y * B.z - A.z * B.y, A.z * B.x - A.x * B.z, A.x * B.y - A.y * B.x); }
	inline double Length(const V3& A) { return std::sqrt(Dot(A, A)); }
	inline V3 Norm(const V3& A)
	{
		const double L = Length(A);
		return L > 1e-12 ? Mul(A, 1.0 / L) : V3(0.0, 0.0, 0.0);
	}

	struct Q
	{
		double x = 0.0, y = 0.0, z = 0.0, w = 1.0;
		Q() = default;
		Q(double X, double Y, double Z, double W) : x(X), y(Y), z(Z), w(W) {}
	};

	inline Q QMul(const Q& A, const Q& B)
	{
		return Q(A.w * B.x + A.x * B.w + A.y * B.z - A.z * B.y,
		         A.w * B.y - A.x * B.z + A.y * B.w + A.z * B.x,
		         A.w * B.z + A.x * B.y - A.y * B.x + A.z * B.w,
		         A.w * B.w - A.x * B.x - A.y * B.y - A.z * B.z);
	}
	inline Q QInv(const Q& A) { return Q(-A.x, -A.y, -A.z, A.w); }
	inline V3 QRot(const Q& A, const V3& V)
	{
		const double Tx = 2 * (A.y * V.z - A.z * V.y), Ty = 2 * (A.z * V.x - A.x * V.z), Tz = 2 * (A.x * V.y - A.y * V.x);
		return V3(V.x + A.w * Tx + (A.y * Tz - A.z * Ty),
		          V.y + A.w * Ty + (A.z * Tx - A.x * Tz),
		          V.z + A.w * Tz + (A.x * Ty - A.y * Tx));
	}
	/** Rotation of Ang radians about a unit Axis. */
	inline Q QAxis(const V3& Axis, double Ang)
	{
		const double S = std::sin(Ang / 2);
		return Q(Axis.x * S, Axis.y * S, Axis.z * S, std::cos(Ang / 2));
	}
	inline Q QNorm(const Q& A)
	{
		const double Sq[4] = {A.x * A.x, A.y * A.y, A.z * A.z, A.w * A.w};   // ql.qnorm: sqrt(sum(c * c for c in q))
		const double N = std::sqrt(PyFloatSum(Sq, 4));
		return Q(A.x / N, A.y / N, A.z / N, A.w / N);
	}
	/** Rotation of Deg degrees about a unit Axis (grasp.Q). */
	inline Q QDeg(const V3& Axis, double Deg) { return QAxis(Axis, Radians(Deg)); }

	/** Rigid transform with UE FTransform semantics (no scale): A * B = apply A, then B. */
	struct X
	{
		Q q;
		V3 t;
		X() = default;
		X(const Q& InQ, const V3& InT) : q(InQ), t(InT) {}

		/** [tx, ty, tz, qx, qy, qz, qw] (the dump scripts' f7 layout). */
		static X F7(const double* A) { return X(Q(A[3], A[4], A[5], A[6]), V3(A[0], A[1], A[2])); }

		V3 Pos(const V3& V) const { return Add(QRot(q, V), t); }
		V3 Vec(const V3& V) const { return QRot(q, V); }
		V3 IPos(const V3& V) const { return QRot(QInv(q), Sub(V, t)); }
		V3 IVec(const V3& V) const { return QRot(QInv(q), V); }
		X Inv() const
		{
			const Q Qi = QInv(q);
			return X(Qi, Mul(QRot(Qi, t), -1.0));
		}
	};

	/** Child * Parent (ql.T.__mul__): the child's local transform composed into the parent's space. */
	inline X operator*(const X& Child, const X& Parent)
	{
		return X(QNorm(QMul(Parent.q, Child.q)), Add(QRot(Parent.q, Child.t), Parent.t));
	}
}
