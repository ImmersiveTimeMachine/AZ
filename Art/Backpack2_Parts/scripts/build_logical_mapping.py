import json
from pathlib import Path

root=Path('C:/UnrealEngine/Games/AZ/Art/Backpack2_Parts')
top=json.loads((root/'inspection/source_topology.json').read_text())
accessories={
    'BP2_Knife':[17,121,161,29,70],
    'BP2_Axe':[11,133,150,167,172],
    'BP2_Bottle':[28,43,95,96,97,98,138,175,176,177],
    'BP2_Bottle_Holder':[21,132,173],
    'BP2_Rope':[9,93,94,110,146],
    'BP2_Front_Pouch':[30,32,56,71,99,100,141,178,179],
}
used=[c for group in accessories.values() for c in group]
assert len(used)==len(set(used))
components={'BP2_Backpack':[c for c in range(len(top['parts'])) if c not in used],**accessories}
parts={name:sorted(f for c in group for f in top['parts'][c]['faces']) for name,group in components.items()}
flat=[f for group in parts.values() for f in group]
assert sorted(flat)==list(range(14612))
manifest={'schema_version':1,'source_object':top['object'],'parts':parts,'components':components,
          'notes':{'BP2_Backpack':'Main bag, straps, harness and remaining fasteners.',
                   'BP2_Knife':'Wooden handle, blue wrap and metal blade/guard combined. Source blade tip is truncated/open; preserved as modeled.',
                   'BP2_Bottle_Holder':'Carrier cloth separated from the bottle.',
                   'BP2_Front_Pouch':'Pouch includes flap, strap and button.'}}
(root/'inspection/logical_parts.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
print(json.dumps({name:len(faces) for name,faces in parts.items()}))
