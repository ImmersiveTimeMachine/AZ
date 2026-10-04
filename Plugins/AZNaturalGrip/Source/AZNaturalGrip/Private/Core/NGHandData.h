// Copyright Artur. AZ project.
// Natural Grip solver core: the raw hand inputs, whichever way they were read (the Python dump JSON files or the hero
// mesh asset). Same content as Saved/wgs/win_hand_live[_l].json (bones, parents) + hand_<s>_verts.json (mesh ref
// locals, bind pose in hand space, hand-space skin vertices rounded to 1e-3 like dump_hand_verts.py).
#pragma once

#include "NGMath.h"

#include <string>
#include <utility>
#include <vector>

namespace ng
{
	struct HandData
	{
		std::vector<std::string> Bones;                          // [0] = hand_<side>, parents before children
		std::vector<std::string> Parents;                        // per bone ([0] unused)
		std::vector<std::pair<std::string, X>> RefLocalMesh;     // finger bones' mesh ref-pose locals
		std::vector<std::pair<std::string, X>> BoneHandSpace;    // finger bones' bind pose in hand space
		std::vector<V3> VertsHandSpace;                          // hand skin vertices in hand space

		/** The FK bone order the solver uses: hand, then per finger (thumb, index, middle, ring, pinky) its chain. */
		static std::vector<std::string> SolverBones(char Side);
	};

	/** The clip hold: where the weapon's clips put the hand on the weapon (a medoid frame), sampled from the clips in the
	 *  editor (or read from <key>_hold_pick.json / <key>_left_picks.json). */
	struct HoldData
	{
		X HandInWeapon;                                          // the hand in weapon space
		std::vector<std::pair<std::string, X>> Locals;           // the finger bones' locals in that frame
		bool bHasElbows = false;
		V3 ElbowAim, ElbowRelaxed;                               // lowerarm in weapon space (support hand: wrist bend)
		std::string AimClip, RelaxedClip;                        // where the picks came from (log)
		double AimTime = 0.0, RelaxedTime = 0.0;
	};

	inline std::vector<std::string> HandData::SolverBones(char Side)
	{
		const std::string S(1, Side);
		std::vector<std::string> Out = {"hand_" + S};
		static const char* Names[] = {"thumb", "index", "middle", "ring", "pinky"};
		for (int F = 0; F < 5; ++F)
		{
			if (F != 0)
			{
				Out.push_back(std::string(Names[F]) + "_metacarpal_" + S);
			}
			for (int K = 1; K <= 3; ++K)
			{
				Out.push_back(std::string(Names[F]) + "_0" + std::to_string(K) + "_" + S);
			}
		}
		return Out;
	}
}
