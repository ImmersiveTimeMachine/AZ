---
name: project-protected-weapon-sets
description: "★★★ User: the M16 (автомат), pistol and unarmed clip sets are tuned and near-perfect - never break them; they live in gitignored Content/AZ/Assets; full backup 2026-09-27 at C:/UnrealEngine/Games/AZ_Backups/2026-09-27_pre-WGS."
metadata:
  node_type: memory
  type: project
  originSessionId: 3384aa9b-49dd-43e9-a26a-858cd5de9d54
  modified: 2026-09-27T05:21:05.429Z
---

User (2026-09-27): "автоматы и пистолеты, все клипы почти идеально. Мы их подбирали достаточно долго... не сломай эту
работу, подумай, чтобы иметь какой-то бэкап."

- `Content/AZ/Assets/**` is gitignored (.gitignore line `/Content/AZ/*` keeps only Blueprints): M16, Pistol, RTG
  (unarmed), GASP, Riffle_RTG, Master (RifleMega grip curves + finger fixes), RifleMega, Weapons, Characters exist
  ONLY on disk. Full copy (5852 files, 5.42 GB, editor closed) in `C:/UnrealEngine/Games/AZ_Backups/2026-09-27_pre-WGS/`
  with a README; restore only with the editor closed.
- Batch tools must never write to M16 / Pistol / RTG / GASP / Riffle_RTG, the M16/pistol profiles, `AZ_BP_Rifle`,
  `AZ_BP_Pistol` or their hero sockets. Chooser work only ADDS rows for the new weapon tag.
- The grip node is inert for weapons without grip data (M16/pistol have none) - keep it that way until an explicit
  migration card.
- After any C++ / hero-ABP phase ask the user for a PIE regression pass with M16 + pistol.
Rules also in `docs/design-briefs/weapon-grip-system-plan.md` ("Protected content"). Related:
[[project-winchester-integration-audit-2026-09-26]], [[feedback-history-rewrite-deletes-files]].
