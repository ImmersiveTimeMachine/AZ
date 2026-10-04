// Copyright Artur. AZ project.
// Natural Grip solver core: the MetaHuman hand model.
// Port of Tools/wgs/natgrip/hand.py (bone structure, hold), skin.py (skin bound to bone segments), grasp.py (finger and
// thumb locals, FK, link penetration / gap, pad point), grasp2.pens and grasp3 (phalanx capsules, overlap).
//
// Rig facts the model relies on (verified on the MetaHuman rig, 2026-09-29):
//  * finger flexion = bone local -Z, MCP side angle = local +Y;
//  * angles are ABSOLUTE anatomical degrees; the mesh ref pose is already curled (REF_ABS);
//  * 4 segments per long finger: metacarpal (ring/pinky cup coupled to MCP flexion), MCP (flex + side), PIP, DIP.
#pragma once

#include "NGField.h"
#include "NGMath.h"

#include <array>
#include <string>
#include <vector>

namespace ng
{
	enum EFinger : int { Thumb = 0, Index = 1, Middle = 2, Ring = 3, Pinky = 4, NumFingers = 5 };

	/** Bone-local transforms by bone index; Set[i] = false means "use the mesh ref pose" (grasp.world's locs.get). */
	struct Locals
	{
		std::vector<X> L;
		std::vector<char> Set;

		void Init(size_t NumBones)
		{
			L.assign(NumBones, X());
			Set.assign(NumBones, 0);
		}
		void Put(int Bone, const X& T)
		{
			L[Bone] = T;
			Set[Bone] = 1;
		}
		bool Has(int Bone) const { return Set[Bone] != 0; }
		/** dict.update. */
		void Update(const Locals& Other)
		{
			for (size_t I = 0; I < L.size(); ++I)
			{
				if (Other.Set[I])
				{
					L[I] = Other.L[I];
					Set[I] = 1;
				}
			}
		}
	};

	using Pose = std::vector<X>;   // bone transforms in the solve space (weapon space), by bone index

	/** Phalanx capsule: segment A-B with radius R (cm). */
	struct Seg
	{
		V3 A, B;
		double R = 0.0;
	};

	struct HandModel
	{
		char Side = 'r';                         // 'r' trigger hand, 'l' support hand
		std::vector<std::string> Bones;          // [0] = hand_<side>, parents before children
		std::vector<int> Parent;                 // -1 for the hand
		std::vector<X> MRef;                     // mesh ref-pose local transforms
		std::vector<X> BCS;                      // bind pose in hand space ([0] = identity)
		std::array<std::vector<int>, NumFingers> Chain;   // thumb: 01,02,03; others: metacarpal,01,02,03

		struct FBind
		{
			int Bone;
			V3 Off;                              // vertex in the bone's space
		};
		std::vector<FBind> Bind;                 // every skin vertex (skin.BIND)
		std::vector<std::vector<V3>> Group;      // per bone, thinned (grasp.GROUP)

		const Field* Fld = nullptr;

		// constants (grasp.py, grasp3.py)
		double Tol = 0.05;                       // skin may touch, not pierce (cm)
		double LimMcp[2] = {-15.0, 90.0};
		double LimPip[2] = {0.0, 105.0};
		double LimDip[2] = {0.0, 80.0};
		double RefAbs[NumFingers][3] = {{0, 0, 0}, {20, 12, 4}, {24, 20, 4}, {18, 25, 5}, {12, 20, 4}};
		double Cup[NumFingers] = {0.0, 0.0, 0.0, 8.0, 16.0};
		double Rad[NumFingers][3] = {{1.3, 1.0, 0.9}, {1.0, 0.85, 0.68}, {1.0, 0.85, 0.7}, {0.93, 0.78, 0.64}, {0.84, 0.7, 0.6}};

		int NumBones() const { return static_cast<int>(Bones.size()); }
		int BoneIndex(const std::string& Name) const;
		const std::string& HandName() const { return Bones[0]; }
		int LastBone(int F) const { return Chain[F].back(); }

		/** Builds Chain / Bind / Group from Bones, Parent, MRef, BCS and the hand-space skin vertices. */
		void BuildSkin(const std::vector<V3>& VertsHandSpace, int Thin);

		// --- FK ---
		/** grasp.world: bones missing from Locs use the mesh ref pose. */
		void World(const X& Hand, const Locals& Locs, Pose& W) const;
		/** hand.fk: every bone from Full (all must be set). */
		void FK(const X& Hand, const Locals& Full, Pose& W) const;

		// --- joint parameterisation ---
		/** grasp.finger_locals (F = Index..Pinky): metacarpal cup, MCP side phi + flexion, PIP, DIP (absolute degrees). */
		void FingerLocals(int F, double CupDeg, double Phi, double A0, double A1, double A2, Locals& Out) const;
		/** grasp.thumb_locals: CMC = Base's thumb_01 rotation (+ optional extra side/flex), MCP / IP flexion relative to ref. */
		void ThumbLocals(const Locals& Base, double A1, double A2, bool bCmc, double CmcAbd, double CmcFlex, Locals& Out) const;

		// --- contact ---
		double LinkPen(const Pose& W, int Bone) const;
		double LinkGap(const Pose& W, int Bone) const;
		/** grasp2.pens for a long finger: penetration of the three phalanges. */
		void Pens(const Pose& W, int F, double Out[3]) const;
		double MaxPen(const Pose& W, int F) const;
		/** grasp.pad_point: the fingertip pad. */
		V3 PadPoint(const Pose& W, int F) const;
		/** grasp3.segs: the last three phalanx capsules. */
		void Segs(const Pose& W, int F, Seg Out[3]) const;
	};

	/** A posed hand as capsules: the three phalanges of every finger and the thumb, plus the palm (metacarpals, wrist to
	 *  the knuckles, thicker). For a second hand that lies on this one. */
	std::vector<Capsule> HandCapsules(const HandModel& H, const X& Hand, const Locals& Locs);

	/** grasp3.seg_seg: closest distance between two segments. */
	double SegSeg(const V3& P1, const V3& Q1, const V3& P2, const V3& Q2);
	/** grasp3.finger_overlap: worst capsule overlap (0 when clear by more than 2.5 mm of slack). */
	double FingerOverlap(const Seg Sa[3], const std::vector<Seg>& Others);

	/** Python's max(0.0, min(1.0, X)) including its NaN behaviour. */
	inline double Clamp01Py(double V)
	{
		const double M = V < 1.0 ? V : 1.0;
		return M > 0.0 ? M : 0.0;
	}
}
