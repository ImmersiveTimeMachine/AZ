"""Three editable CHALK inspection concepts. Uses existing approved art; no Unreal changes."""
from pathlib import Path
import ast, json, traceback
from gi.repository import Gimp, Gio, Gegl

PROJECT = Path('C:/UnrealEngine/Games/AZ')
helpers = PROJECT/'Tools/unified_ui_mockups_gimp.py'
tree = ast.parse(helpers.read_text(encoding='utf-8'))
tree.body = [n for n in tree.body if not isinstance(n,ast.Try)]
exec(compile(tree,str(helpers),'exec'),globals())
OUT = PROJECT/'UI Design/CHALK_Inspection_Concepts_v01'
OUT.mkdir(parents=True,exist_ok=True)
THEME = dict(THEMES[1], text='#EEEAE0',muted='#C6CABB',edge='#AAB39E')
WHITE='#EEEAE0'; INK='#282E29'; MUTED='#596355'


def photo(d,x,y,w):
    h=w*.79
    d.rect('Photograph / soft contact shadow',x+10,y+16,w,h,'#090E0C',36)
    d.rect('Photograph / paper',x,y,w,h,'#E9E1CA')
    d.outline('Photograph / edge',x,y,w,h,'#B9AD93',1)
    d.image(SCENE,'Photo image / approved project concept art',x+22,y+22,w-44,(w-44)*.56)
    d.text('Saint-Laurent',x+28,y+h-74,26,INK,'Roboto Regular')
    d.text('2009',x+w-94,y+h-67,19,MUTED)
    return h


def key(d,label,caption,x,y,width=34):
    d.rect('Keycap '+label,x,y,width,32,'#20281F',94)
    d.outline('Square keycap '+label,x,y,width,32,WHITE,1)
    d.center(label,x+width/2,y+5,17,WHITE)
    d.text(caption,x+width+12,y+4,20,WHITE)


def screen(number,title,subtitle):
    image=Gimp.Image.new(1920,1080,Gimp.ImageBaseType.RGB)
    base=group(image,'01 / Environment')
    d=Draw(image,base,THEME)
    d.image(SCENE,'Approved Montreal environment',0,0,1920,1080)
    ui=group(image,'03 / Editable interface')
    d=Draw(image,ui,THEME)
    d.text('CHALK',58,38,30,WHITE,'Roboto Bold')
    d.text(number+' / '+title,58,84,21,WHITE,'Roboto Bold')
    d.text(subtitle,58,115,18,'#CDD0C5')
    d.right('КОНЦЕПТ • НЕ ИГРОВОЙ СКРИНШОТ',1858,42,15,'#CDD0C5')
    # Only a narrow footer receives contrast; the scene is never blacked out.
    d.rect('Footer / local contrast',42,967,1836,73,'#111A16',73)
    key(d,'ЛКМ','Вращать',68,987,49)
    key(d,'↕','Приблизить',322,987,34)
    key(d,'Space','Перевернуть',596,987,72)
    key(d,'F','Прочитать',951,987)
    key(d,'E','Взять',1238,987)
    key(d,'Esc','Вернуть',1490,987,52)
    return image,base


def save(image,name):
    png_export(image,OUT/(name+'.png'))
    assert Gimp.file_save(Gimp.RunMode.NONINTERACTIVE,image,Gio.File.new_for_path(str(OUT/(name+'.xcf'))))
    stats=counts(image)
    image.delete()
    return {'name':name,'layers':stats}


def main():
    Gimp.context_push()
    Gimp.context_set_antialias(True);Gimp.context_set_feather(False)
    report=[]
    img,base=screen('01','ПРЕДМЕТ ПЕРЕД КАМЕРОЙ','Окружение остаётся читаемым; интерфейс почти не вмешивается.')
    prop=group(img,'02 / Inspection object');d=Draw(img,prop,THEME)
    photo(d,1000,245,570)
    d.text('ФОТОГРАФИЯ',1000,735,22,WHITE,'Roboto Bold')
    d.text('Знакомый двор. На обороте осталась подпись.',1000,776,20,WHITE)
    report.append(save(img,'01_Context'))

    img,base=screen('02','КАМЕРА ПРИБЛИЖАЕТСЯ','Крупный план без отдельного тёмного экрана.')
    # A tighter crop represents the camera moving closer, not a global dimming effect.
    for layer in base.get_children():
        layer.scale(2496,1404,False);layer.set_offsets(-480,-190)
    prop=group(img,'02 / Inspection object');d=Draw(img,prop,THEME)
    photo(d,665,170,760)
    d.text('ФОТОГРАФИЯ',665,815,22,WHITE,'Roboto Bold')
    d.text('Переверните, чтобы рассмотреть обратную сторону.',665,855,20,WHITE)
    report.append(save(img,'02_Closeup'))

    img,base=screen('03','ПРЕДМЕТ И ПОЛЕВЫЕ ЗАПИСИ','Чтение открывается рядом; фотография и мир остаются видны.')
    prop=group(img,'02 / Inspection object');d=Draw(img,prop,THEME)
    photo(d,360,240,565)
    paper=group(img,'04 / Reading panel');d=Draw(img,paper,THEME)
    d.rect('Reading / local shadow',1090,200,680,660,'#111A16',24)
    d.rect('Reading / paper',1076,188,680,660,'#DDD6C4')
    d.line('Reading / left rule',1124,239,1124,794,'#949B88',2)
    d.text('НА ОБОРОТЕ',1160,236,18,MUTED,'Roboto Bold')
    d.text('Saint-Laurent, 2009',1160,282,32,INK,'Roboto Bold')
    d.text('«Наш двор. До того,\nкак улицы опустели».',1160,373,27,INK)
    d.text('Подпись на фотографии',1160,498,19,MUTED)
    d.line('Reading / separator',1160,557,1705,557,'#949B88',1)
    d.text('Фотография',1160,594,22,INK,'Roboto Bold')
    d.text('Можно осмотреть с обеих сторон\nили вернуть на место.',1160,638,22,INK)
    d.text('Текст — пример для макета',1160,784,16,MUTED)
    report.append(save(img,'03_Read'))
    (OUT/'manifest.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
    Gimp.context_pop()
    print('INSPECTION_CONCEPTS_SAVED count=3')


try:
    main()
except Exception:
    (OUT/'error.txt').write_text(traceback.format_exc(),encoding='utf-8')
    raise
