from pathlib import Path
import json
path=Path('C:/UnrealEngine/Games/AZ/Art/Backpack2_Parts/scripts/separate_backpack.py')
exec(compile(path.read_text(),str(path),'exec'))
report=build_separated()
(ROOT/'inspection/separation_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
