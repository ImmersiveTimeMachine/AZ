from pathlib import Path
diagnostic_scene='BP2_Knife_Inspection'
diagnostic_image='knife_closeup.png'
diagnostic_groups=[]
for name,ids in [('Complete knife',[17,121,161,29,70]),('Lower section',[29,70]),('Upper section',[17,121,161])]:
    for angle in [-45,45,135]:
        diagnostic_groups.append({'name':f'{name} {angle} degrees','component_ids':ids,'rotation_deg':angle})
path=Path('C:/UnrealEngine/Games/AZ/Art/Backpack2_Parts/scripts/inspect_textured_parts.py')
exec(compile(path.read_text(),str(path),'exec'))
