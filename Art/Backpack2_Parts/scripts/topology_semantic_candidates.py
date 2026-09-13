import json
from pathlib import Path
p=Path(r'C:/UnrealEngine/Games/AZ/Art/Backpack2_Parts/inspection')
sets={s['id']:s for s in json.loads((p/'coincident_component_sets.json').read_text())['sets']}
choices=[
 ('Axe',[11,133,167],'high','Whole axe head, haft and grip. All weights Axe1=1.'),
 ('Knife',[17,29],'high','Wrapped wooden handle plus metal blade/guard, confirmed using native textured multi-angle renders. The source blade tip is truncated and open (five boundary edges); finish the tip before exposed handheld use. Preliminary sheath classification was corrected.'),
 ('Bottle',[28,98],'high','Bottle body and cap; holder excluded.'),
 ('Bottle_Holder',[21],'high','Wraparound bottle carrier on bag side.'),
 ('Cord_Loop',[9],'high','Double loop cord/rope. Same seam-connected geometry has mixed Backpack2/Axe1 deformation weights.'),
 ('Front_Utility_Pouch',[30,100,32],'medium-high','Front pouch shell, internal flap/band and round clasp. Optional separate accessory; may remain attached to backpack.')
]
records=[];used=set()
for name,ss,conf,desc in choices:
 cs=sorted(c for s in ss for c in sets[s]['components']); used.update(cs)
 records.append({'name':name,'seam_set_ids':ss,'component_ids':cs,'vertex_count':sum(sets[s]['vertex_count'] for s in ss),'face_count':sum(sets[s]['face_count'] for s in ss),'confidence':conf,'description':desc})
remain=sorted(set(range(195))-used)
top=json.loads((p/'source_topology.json').read_text())['parts']
records.insert(0,{'name':'Backpack_WithHarnessAndFasteners','component_ids':remain,'vertex_count':sum(top[c]['vertex_count'] for c in remain),'face_count':sum(top[c]['face_count'] for c in remain),'confidence':'high','description':'Main bag, shoulder and lower harness, dangling straps, buckles and remaining hardware. Optional bottle holder and pouch may be grouped back with this object.'})
result={'status':'read-only candidate segmentation; parent must visually confirm before use','source_object':'SKM_SurvivalMan_backpack2','components_are':'zero-based IDs from source_topology.json, NOT polygon or vertex IDs','groups':records}
assert sum(r['vertex_count'] for r in records)==9866
assert sum(r['face_count'] for r in records)==14612
(p/'semantic_component_candidates.json').write_text(json.dumps(result,indent=2))
print(json.dumps([{k:v for k,v in r.items() if k not in ('description','component_ids')} for r in records]))
