// Copyright Artur. AZ project.
#include "NGSetup.h"

#include <fstream>

namespace ng
{
	bool FileExists(const std::string& Path)
	{
		std::ifstream F(Path, std::ios::binary);
		return static_cast<bool>(F);
	}

	namespace
	{
		X F7Of(const JValue& A)
		{
			double V[7];
			for (int K = 0; K < 7; ++K)
			{
				V[K] = A.NumAt(K);
			}
			return X::F7(V);
		}

		bool EndsWith(const std::string& S, const std::string& Suffix)
		{
			return S.size() >= Suffix.size() && S.compare(S.size() - Suffix.size(), Suffix.size(), Suffix) == 0;
		}
	}

	void Placement::Init(const HandModel& Hand, const X& InHandW, const Locals& InClip)
	{
		H = &Hand;
		HandW = InHandW;
		Clip = InClip;
		Pose Wc;
		Hand.FK(HandW, Clip, Wc);
		Pivot = Wc[Hand.Chain[Middle][1]].t;
		FwdSign = Hand.Side == 'r' ? -1.0 : 1.0;
		const std::string Meta = std::string("metacarpal_") + Hand.Side;
		std::vector<std::pair<int, V3>> All;
		for (const HandModel::FBind& B : Hand.Bind)
		{
			if (EndsWith(Hand.Bones[B.Bone], Meta) || (B.Bone == 0 && B.Off.x * FwdSign > -0.5))
			{
				All.emplace_back(B.Bone, B.Off);
			}
		}
		Palm.clear();
		for (size_t I = 0; I < All.size(); I += 3)
		{
			Palm.push_back(All[I]);
		}
	}

	X Placement::Corr(double Yaw, double Pitch, double Roll, const V3& D) const
	{
		const Q Rot = QMul(QAxis(V3(0, 0, 1), Radians(Yaw)), QMul(QAxis(V3(1, 0, 0), Radians(Pitch)), QAxis(V3(0, 1, 0), Radians(Roll))));
		const V3 T = Add(Sub(Pivot, QRot(Rot, Pivot)), D);
		return X(Rot, T);
	}

	double Placement::PalmWorst(const X& C) const
	{
		Pose W;
		H->FK(HandNew(C), Clip, W);
		double Best = 0.0;
		bool bFirst = true;
		for (const auto& BO : Palm)
		{
			const double D = H->Fld->Sample(W[BO.first].Pos(BO.second));
			if (bFirst || D < Best)
			{
				Best = D;
				bFirst = false;
			}
		}
		return Best;
	}

	bool Setup::ReadLocals(const JValue& Obj, Locals& Out, std::string& Error) const
	{
		Out.Init(Hand.Bones.size());
		for (const auto& KV : Obj.Obj)
		{
			const int B = Hand.BoneIndex(KV.first);
			if (B < 0)
			{
				Error = "unknown bone " + KV.first;
				return false;
			}
			Out.Put(B, F7Of(KV.second));
		}
		return true;
	}

	bool ReadHandDataJson(const std::string& SavedDir, char Side, HandData& Out, std::string& Error)
	{
		Out = HandData();
		try
		{
			JValue D;
			if (!LoadJsonFile(SavedDir + (Side == 'r' ? "/win_hand_live.json" : "/win_hand_live_l.json"), D, Error))
			{
				return false;
			}
			const JValue& Bones = D["bones"];
			for (size_t I = 0; I < Bones.Size(); ++I)
			{
				Out.Bones.push_back(Bones.At(I).S);
			}
			Out.Parents.assign(Out.Bones.size(), std::string());
			const JValue& Parents = D["parents"];
			for (size_t B = 1; B < Out.Bones.size(); ++B)
			{
				Out.Parents[B] = Parents[Out.Bones[B]].S;
			}
			JValue V;
			if (!LoadJsonFile(SavedDir + "/hand_" + std::string(1, Side) + "_verts.json", V, Error))
			{
				return false;
			}
			for (const auto& KV : V["ref_local_mesh"].Obj)
			{
				Out.RefLocalMesh.emplace_back(KV.first, F7Of(KV.second));
			}
			for (const auto& KV : V["bone_hand_space"].Obj)
			{
				Out.BoneHandSpace.emplace_back(KV.first, F7Of(KV.second));
			}
			const JValue& Verts = V["verts_hand_space"];
			Out.VertsHandSpace.reserve(Verts.Size());
			for (size_t I = 0; I < Verts.Size(); ++I)
			{
				const JValue& P = Verts.At(I);
				Out.VertsHandSpace.emplace_back(P.NumAt(0), P.NumAt(1), P.NumAt(2));
			}
		}
		catch (const std::exception& E)
		{
			Error = E.what();
			return false;
		}
		return true;
	}

	bool Setup::Load(const SetupConfig& InCfg, std::string& Error, const SetupInputs& Inputs)
	{
		Cfg = InCfg;
		const std::string& S = Cfg.SavedDir;
		const bool bWin = Cfg.Weapon == "winchester";
		const bool bRight = Cfg.Side == 'r';
		try
		{
			// --- field (views.py): given lattices, else the data files ---
			if (Inputs.Coarse)
			{
				CoarsePath = "(baked)";
				Fld.Coarse = *Inputs.Coarse;
			}
			else
			{
				CoarsePath = bWin ? S + "/gf_winchester.json" : S + "/fields/" + Cfg.Weapon + "_field.json";
				if (!Fld.Coarse.LoadJson(CoarsePath, Error))
				{
					return false;
				}
			}
			bFineLoaded = false;
			if (Cfg.bFine && Inputs.Fine)
			{
				FinePath = "(baked)";
				Fld.Fine = *Inputs.Fine;
				bFineLoaded = true;
			}
			else if (Cfg.bFine && !Inputs.Coarse)
			{
				FinePath = S + "/fine_field_" + (bRight ? "right" : "left") + (bWin ? "" : "_" + Cfg.Weapon) + ".json";
				if (FileExists(FinePath))
				{
					if (!Fld.Fine.LoadJson(FinePath, Error))
					{
						return false;
					}
					bFineLoaded = true;
				}
			}
			Fld.bUseFine = bFineLoaded;
			Fld.Obstacles.clear();
			if (Inputs.Obstacles)
			{
				Fld.Obstacles = *Inputs.Obstacles;
			}

			// --- hand (hand.py bone structure + skin.py): given data (the hero mesh), else the dump files ---
			HandData Own;
			const HandData* HD = Inputs.Hand;
			if (!HD)
			{
				if (!ReadHandDataJson(S, Cfg.Side, Own, Error))
				{
					return false;
				}
				HD = &Own;
			}
			Hand = HandModel();
			Hand.Side = Cfg.Side;
			Hand.Fld = &Fld;
			Hand.Bones = HD->Bones;
			Hand.Parent.assign(Hand.Bones.size(), -1);
			for (int B = 1; B < Hand.NumBones(); ++B)
			{
				Hand.Parent[B] = Hand.BoneIndex(HD->Parents[static_cast<size_t>(B)]);
				if (Hand.Parent[B] < 0 || Hand.Parent[B] >= B)
				{
					Error = "bone order: parent of " + Hand.Bones[B];
					return false;
				}
			}
			Hand.MRef.assign(Hand.Bones.size(), X());
			Hand.BCS.assign(Hand.Bones.size(), X());
			for (const auto& KV : HD->RefLocalMesh)
			{
				const int B = Hand.BoneIndex(KV.first);
				if (B < 0)
				{
					Error = "hand: unknown bone " + KV.first;
					return false;
				}
				Hand.MRef[B] = KV.second;
			}
			for (const auto& KV : HD->BoneHandSpace)
			{
				const int B = Hand.BoneIndex(KV.first);
				if (B < 0)
				{
					Error = "hand: unknown bone " + KV.first;
					return false;
				}
				Hand.BCS[B] = KV.second;
			}
			Hand.BuildSkin(HD->VertsHandSpace, Cfg.Thin);

			// --- the hold (hand.py) ---
			X HandW;
			Locals Clip;
			if (Inputs.Hold)
			{
				// sampled from the weapon's clips in the editor
				HandW = Inputs.Hold->HandInWeapon;
				Clip.Init(Hand.Bones.size());
				for (const auto& KV : Inputs.Hold->Locals)
				{
					const int B = Hand.BoneIndex(KV.first);
					if (B >= 0)
					{
						Clip.Put(B, KV.second);
					}
				}
			}
			else if (bRight && !bWin)
			{
				JValue Hold;
				if (!LoadJsonFile(S + "/" + Cfg.Weapon + "_hold_pick.json", Hold, Error))
				{
					return false;
				}
				HandW = F7Of(Hold["weapon_in_hand"]).Inv();
				if (!ReadLocals(Hold["local"], Clip, Error))
				{
					return false;
				}
			}
			else if (bRight)
			{
				JValue D;
				if (!LoadJsonFile(S + "/win_hand_live.json", D, Error))
				{
					return false;
				}
				const JValue& Frame = D["clips"]["AZ_MST_Rifle01_St_Aim_CC"]["frames"].At(0);
				HandW = F7Of(Frame["hand_in_weapon"]);
				if (!ReadLocals(Frame["local"], Clip, Error))
				{
					return false;
				}
			}
			else if (!bWin)
			{
				JValue Picks;
				if (!LoadJsonFile(S + "/" + Cfg.Weapon + "_left_picks.json", Picks, Error))
				{
					return false;
				}
				const JValue& Hold = Picks[Cfg.LeftPick];
				HandW = F7Of(Hold["hand_in_weapon"]);
				if (!ReadLocals(Hold["local"], Clip, Error))
				{
					return false;
				}
			}
			else
			{
				JValue D;
				if (!LoadJsonFile(S + "/win_hand_live_l.json", D, Error))
				{
					return false;
				}
				HandW = F7Of(D["hand_in_weapon"]);
				if (!ReadLocals(D["clip_local"], Clip, Error))
				{
					return false;
				}
			}
			for (int B = 1; B < Hand.NumBones(); ++B)
			{
				if (!Clip.Has(B))
				{
					Error = "hold locals miss " + Hand.Bones[B];
					return false;
				}
			}
			Plc.Init(Hand, HandW, Clip);
		}
		catch (const std::exception& E)
		{
			Error = E.what();
			return false;
		}
		return true;
	}
}
