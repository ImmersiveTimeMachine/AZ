# @Description: Apply Field Notes overlay styling to exact existing owned HUD/Quick/compass/quest templates.
"""Import-inert and file-plan-first. Root executes:

    plan()                        # offline baseline/schema validation; no editor calls
    apply_asset(asset_path)       # one backed-up owned package per call, Idle only
    # Native compile and explicit save outside Python, in plan()['compile_order'].
    verify_asset(asset_path)      # exact requested values + preserved tree/slots
    gap_recipe()                  # explicit remaining state/category wiring, not claimed implemented

    # Additive stages after the first pass has been compiled/saved/verified:
    apply_quick_frame(Entry_or_Fists_asset)  # requires imported T_FN_QuickFrame
    prepare_equipped_notch()      # native compile Entry outside Python
    wire_equipped_notch()         # native compile/save Entry outside Python
    prepare_side_profiles()       # native compile QuestModule outside Python
    configure_side_profiles()
    wire_marker_categories()     # native compile/save QuestModule outside Python
    verify_followups()            # read-only structural/style receipt, not runtime proof

No widget creation/removal, graph writes, compile/save/PIE, source-pack changes,
input edits, inventory defaults, current mode/destination logic or marker-key
changes occur in this style stage. Unknown widgets remain untouched. All desired
keys are checked against both the fresh baseline and the current native schema.
The full live before/after property diff must be contained by the exact patch.
"""
from __future__ import annotations

import copy
import importlib.util
import json
import shutil
from pathlib import Path

ROOT = Path('C:/UnrealEngine/Games/AZ')
_spec = importlib.util.spec_from_file_location('fn_overlay_styles', ROOT / 'Tools/field_notes_styles_setup.py')
S = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(S)
BASELINE = S.OUT / 'Baseline/baseline'
OUT = S.OUT / 'Overlay'
HUD = '/Game/AZ/Blueprints/Menu/HUD/WBP_AZ_GameHUD'
NAV = '/Game/AZ/Blueprints/Menu/HUD/Navigation'
QUICK = '/Game/AZ/Blueprints/Menu/HUD/QuickSelect'
QUEST = NAV + '/Quests'
ART = S.DEST + '/Art'
OWNER_KEY, OWNER = 'AZ.FieldNotes.OverlayAuthor', 'field_notes_overlay_setup:v1'
ASSETS = (HUD, NAV + '/WBP_AZ_CompassModule', NAV + '/WBP_AZ_Compass', NAV + '/WBP_AZ_CompassMarker',
          NAV + '/WBP_AZ_WorldMarker', QUEST + '/WBP_AZ_QuestModule', QUEST + '/WBP_AZ_Mission',
          QUEST + '/WBP_AZ_Task') + tuple(QUICK + '/' + name for name in (
              'WBP_AZ_QuickSelect', 'WBP_AZ_QuickSelectEntry', 'WBP_AZ_QuickSelectFists',
              'WBP_AZ_QuickSelectItemDetails', 'WBP_AZ_QuickSelectNameLeaf', 'WBP_AZ_QuickSelectFocusDetails',
              'WBP_AZ_QuickSelectFocusNameLeaf', 'WBP_AZ_QuickSelectDescriptionLeaf', 'WBP_AZ_QuickSelectImageLeaf'))


def read_baseline(package):
    S.require(package in ASSETS, 'Outside overlay allowlist: ' + package)
    manifest = json.loads((BASELINE / 'manifest.json').read_text(encoding='utf-8'))
    S.require(manifest['complete'] and len(manifest['assets']) >= 45, 'Complete reviewed baseline is required.')
    S.require(package in manifest['assets'], 'Owned overlay package missing from baseline: ' + package)
    return json.loads((BASELINE / manifest['assets'][package]['receipt']).read_text(encoding='utf-8'))


def merge(before, patch):
    result = copy.deepcopy(before)
    for key, value in patch.items():
        result[key] = merge(result[key], value) if isinstance(value, dict) and isinstance(result.get(key), dict) else copy.deepcopy(value)
    return result


def font(px, weight='Regular'):
    spec = S.tokens()['fonts'][weight]
    return {'fontObject': S.ref(spec['asset']), 'fontMaterial': 'None', 'typefaceFontName': spec['typeface'],
            'size': px * .75, 'outlineSettings': {'outlineSize': 0, 'outlineMaterial': 'None'}}


def rgba(role, alpha=1.0):
    return S.linear('Overlay', role, alpha)


def custom(hex_color, alpha=1.0):
    values = [int(hex_color[i:i+2],16) / 255.0 for i in (0,2,4)]
    values = [v/12.92 if v <= .04045 else ((v+.055)/1.055)**2.4 for v in values]
    return dict(zip(('r','g','b','a'), values + [alpha]))


def manual_text(px, weight='Regular', role='text'):
    return {'use Manual Font Info': True, 'font Info': font(px, weight), 'color And Opacity': rgba(role)}


def marker_profile(kind, world=False):
    """Partial style patch: original scale/distance/ping/visibility/arrow policy is preserved."""
    name = {'story':'Story', 'side':'Side', 'personal':'Personal'}[kind]
    result = {'marker Icon': S.ref(ART + '/T_FN_' + name)}
    if world:
        result.update({'marker Icon Color': rgba(kind), 'marker Name Font Info': manual_text(19,'Bold'),
                       'distance Text Font Info': manual_text(16)})
    else:
        result.update({'marker Color': rgba(kind), 'highlight Color': rgba('text'),
                       'distance Text Info': manual_text(19)})
    return result


def plan(write=True):
    """Offline only. The baseline is data; its contents never execute code."""
    snapshots = {p: read_baseline(p) for p in ASSETS}
    patches = {}

    def patch(package, name, values):
        snapshot = snapshots[package]
        if name == '@CDO':
            target = snapshot['cdo']
        else:
            found = [row['widget'] for row in snapshot['templates']
                     if row['row']['widgetName'] == name and 'widget' in row]
            S.require(len(found) == 1, 'Expected one actual template: ' + package + ':' + name)
            target = found[0]
        S._validate_patch(target['schema'], values, package + ':' + name)
        key = (package,name)
        patches[key] = merge(patches.get(key,{}), values)

    def text(package, name, px, weight='Regular', role='text'):
        patch(package,name,{'font':font(px,weight),'colorAndOpacity':S.slate(rgba(role)),
                            'shadowColorAndOpacity':{'r':0,'g':0,'b':0,'a':0},'shadowOffset':{'x':0,'y':0}})

    def tint(package, name, role='text', alpha=1.0):
        patch(package,name,{'colorAndOpacity':rgba(role,alpha)})

    S.require(snapshots[HUD]['tree']['info']['widgetCount'] == 37, 'HUD baseline changed: inspect, do not remove extra widgets.')
    root = QUICK + '/WBP_AZ_QuickSelect'
    S.require(snapshots[root]['tree']['info']['widgetCount'] == 33, 'Quick root baseline changed: inspect, do not rebuild it.')
    patch(HUD,'@CDO',{'healthFillColor':rgba('text'),'criticalHealthFillColor':rgba('danger')})
    for name, px, weight, role in (
        ('WeaponNameText',28,'Bold','text'), ('AmmoRoundsText',57,'Bold','text'),
        ('AmmoCapacityText',27,'Bold','muted'), ('SpareMagazinesText',22,'Bold','text'),
        ('FireModeText',14,'Bold','muted'), ('LowHealthText',19,'Regular','danger'),
        ('ModeNameText',27,'Bold','text'), ('PickupText',19,'Regular','text'), ('InfoText',18,'Regular','muted')):
        text(HUD,name,px,weight,role)
    for name in ('WeaponIcon','MagazineIcon','HealthIcon','FightModeIcon','ExploreModeIcon',
                 'HitTopLeft','HitTopRight','HitBottomLeft','HitBottomRight'):
        tint(HUD,name)
    patch(HUD,'CoreContrast',{'colorAndOpacity':custom('07100E',.60)})
    patch(HUD,'HealthContrast',{'colorAndOpacity':custom('07100E',.60)})
    patch(HUD,'HealthProgressBar',{'fillColorCurrent':rgba('text'),'backgroundTint':rgba('edge',.60)})

    for name, px, weight, role in (
        ('HeaderText',16,'Bold','muted'),('StatusText',15,'Regular','muted'),('HintText',15,'Regular','muted'),
        ('FocusNameText',30,'Bold','text'),('FocusDescriptionText',18,'Regular','muted'),('ModeText',15,'Bold','muted')):
        text(root,name,px,weight,role)
    patch(root,'SceneDim',{'colorAndOpacity':custom('06100C',.18)})
    for name in ('ArrowUp','ArrowDown','ArrowLeft','ArrowRight'):
        tint(root,name,'muted')
    for leaf_name in ('WBP_AZ_QuickSelectEntry','WBP_AZ_QuickSelectFists'):
        package = QUICK + '/' + leaf_name
        names = {row['row']['widgetName'] for row in snapshots[package]['templates'] if 'widget' in row}
        patch(package,'@CDO',{'idleColor':rgba('edge',.70),'hoveredColor':rgba('text'),
                             'editingColor':rgba('text'),'equippedColor':rgba('text',.95)})
        # Preserve the existing dynamic BrushColor/opacity contract until the separately
        # authored transparent frame is available. RoundedBox outline RGB ignores the
        # UBorder tint; with transparent fill, its transparency option cannot reproduce
        # the native assignment pulse (DrawElementTypes.cpp:307-316).
        patch(package,'HighlightBorder',{'brushColor':rgba('edge',.70)})
        tint(package,'CardFill','panel',.78)
        for name in ('Icon','ExploreActionIcon'):
            if name in names: tint(package,name)
        for name, px, weight, role in (
            ('CategoryText',14,'Bold','muted'), ('NameText',15,'Bold','text'), ('KeyText',15,'Bold','muted'),
            ('StateText',10,'Regular','muted'), ('AmmoText',16,'Bold','text'), ('EmptyMarkText',16,'Regular','muted')):
            if name in names: text(package,name,px,weight,role)

    # Actual fragment-assimilation leaves, not only the root's hidden fallback text.
    for name, px, weight, role in (
        ('WBP_AZ_QuickSelectNameLeaf',15,'Bold','text'), ('WBP_AZ_QuickSelectFocusNameLeaf',30,'Bold','text'),
        ('WBP_AZ_QuickSelectDescriptionLeaf',18,'Regular','muted')):
        package = QUICK + '/' + name
        S.require(snapshots[package]['cdo']['values']['textStyle'] == 'None', 'Leaf now has a runtime style override; inspect before replacing fonts.')
        text(package,'Text_LeafText',px,weight,role)
    tint(QUICK + '/WBP_AZ_QuickSelectImageLeaf','Image_Icon')
    # Composite parents retain their references and neutral aggregate tint; child assets above own the style.

    compass = NAV + '/WBP_AZ_CompassModule'
    common = {'baseColor1':rgba('text',.70),'baseColor2':rgba('story'), 'fontColor1':rgba('text'),
              'fontColor2':rgba('muted'),'bodyFont':font(19),'bodyFont2':font(19,'Bold'),
              'headlineFont':font(24,'Bold'),'contrastColor1':custom('07100E',.58)}
    patch(compass,'@CDO',merge(common,{'contrastColor2':custom('07100E',.60), 'microFont':font(19),
        'microFont2':font(19),'compass_Texture':S.ref(ART+'/T_FN_CompassStrip'),
        'compass_Tint':rgba('text'),'pointerTint':rgba('text'),
        'compass_BackgroundColor':custom('07100E'),'compass_BackgroundOpacity':.58}))
    # Existing sizes, rotations, strip draw offsets, masks, precision and pointer resources are untouched.
    tint(NAV+'/WBP_AZ_Compass','Pointer')
    patch(NAV+'/WBP_AZ_Compass','Background',{'colorAndOpacity':custom('07100E',.58)})
    text(NAV+'/WBP_AZ_CompassMarker','DistanceValue',19)
    for name, px, weight in (('MarkerNameText',19,'Bold'),('MarkerNameDistanceText',16,'Regular'),('DistanceText',16,'Regular')):
        text(NAV+'/WBP_AZ_WorldMarker',name,px,weight)
    tint(NAV+'/WBP_AZ_WorldMarker','ArrowIcon')

    module = QUEST + '/WBP_AZ_QuestModule'
    task_info = {}
    for suffix, texture, role in (('Active','Active','text'),('Checked','Complete','muted'),('Failed','Failed','danger')):
        task_info['icon '+suffix] = {'icon Texture':S.ref(ART+'/T_FN_'+texture), 'iconColor':rgba(role), 'highlightColor':rgba('text')}
        task_info['text '+suffix] = manual_text(20,'Regular',role)
    task_info['text Empty'] = manual_text(20,'Regular','muted')
    task_info['icon Empty'] = {'iconColor':rgba('muted'),'highlightColor':rgba('text')}
    patch(module,'@CDO',merge(common,{'headlineFont':font(26,'Bold'),
        'missionNotificationInfo':{'headlineTextInfo':manual_text(26,'Bold'),
                                   'backgroundColor':custom('07100E'),'backgroundOpacity':.58},
        'missionTasksInfo':task_info,
        'questCompassInfo':marker_profile('story'), 'questWorldInfo':marker_profile('story',True),
        'personalCompassInfo':marker_profile('personal'), 'personalWorldInfo':marker_profile('personal',True)}))
    text(module,'CampaignStatusText',18,'Regular','muted')
    text(QUEST+'/WBP_AZ_Mission','MissionNameText',26,'Bold')
    patch(QUEST+'/WBP_AZ_Mission','Background',{'colorAndOpacity':custom('07100E',.58)})
    text(QUEST+'/WBP_AZ_Task','TaskText',20)

    operations = [{'asset':package,'target':name,'patch':value} for (package,name),value in patches.items()]
    for row in operations:
        forbidden = ('visibility','renderOpacity','renderTransform','layoutData','text','fragmentTag',
                     'markerPrecision','compass_Direction','markerY_Translation','pointerY_Translation')
        S.require(not(set(row['patch']) & set(forbidden)), 'Non-style top-level field slipped into overlay patch.')
    order = [p for p in ASSETS if '/QuickSelect/' in p and p != root] + [root, NAV+'/WBP_AZ_CompassMarker',
             NAV+'/WBP_AZ_WorldMarker', NAV+'/WBP_AZ_Compass', NAV+'/WBP_AZ_CompassModule',
             QUEST+'/WBP_AZ_Task',QUEST+'/WBP_AZ_Mission',module,HUD]
    result = {'token_sha256':S.digest(S.TOKENS),'baseline':str(BASELINE),'assets':list(ASSETS),
              'operations':operations,'compile_order':list(dict.fromkeys(order)),
              'geometry_or_tree_changes':False,'equipped_notch_pending':True,'side_category_event_pending':True,
              'border_only_frame_pending':True,
              'critical_overlay_contrast_pending_runtime_review':True}
    return S.write(OUT/'plan.json',result) if write else result


def snapshot(package):
    bp,cdo = S.defaults(package)
    tree = S.native_tool('UMGToolSet.UMGToolSet.GetWidgets',widgetBlueprint=S.ref(package))
    objects = {'@CDO':S.read_object(cdo)}
    slots = {}
    for row in tree['widgets']:
        if isinstance(row.get('widget'),dict): objects[row['widgetName']] = S.read_object(S.resolve(row['widget']))
        if isinstance(row.get('slot'),dict): slots[row['widgetName']] = S.read_object(S.resolve(row['slot']))
    return {'asset':package,'tree':tree,'objects':objects,'slots':slots}


def delta(before,after,prefix=''):
    if isinstance(before,dict) and isinstance(after,dict):
        result=[]
        for key in set(before)|set(after):
            name=prefix+'.'+key if prefix else key
            if key not in before or key not in after: result.append(name)
            else: result.extend(delta(before[key],after[key],name))
        return result
    return [] if S._contains(before,after) and S._contains(after,before) else [prefix]


def leaves(value,prefix=''):
    result=[]
    for key,item in value.items():
        name=prefix+'.'+key if prefix else key
        result.extend(leaves(item,name) if isinstance(item,dict) else [name])
    return result


def apply_asset(package):
    S.require(package in ASSETS,'Outside exact overlay asset scope.')
    S.idle()
    desired=plan(write=False)
    operations=[row for row in desired['operations'] if row['asset']==package]
    before=snapshot(package)
    baseline=read_baseline(package)
    S.require(before['tree']==baseline['tree'],'Widget tree differs from reviewed baseline; refresh/review rather than rebuilding: '+package)
    for row in operations:
        S._validate_patch(before['objects'][row['target']]['schema'],row['patch'],package+':'+row['target'])
        for path in S._references(row['patch']):
            if path.startswith('/Game/') or path.startswith('/Engine/'):
                S.resolve(path)  # Fonts and imported art must really be loaded/resolvable before any write.
    folder=OUT/'BeforeWrites'/S.stamp()/package.rsplit('/',1)[-1]
    S.write(folder/'live-before.json',before)
    disk=S.package_file(package)
    S.require(disk.is_file(),'Baseline package is not saved: '+package)
    shutil.copy2(disk,folder/disk.name)
    changed=False
    for row in operations:
        record=before['objects'][row['target']]
        if S._contains(record['values'],row['patch']): continue
        obj=S.resolve(record['object'])
        S.require(obj.get_package().get_name()==package,'Refusing to modify a referenced/source-pack object: '+record['object'])
        S.require(S.ue().ToolsetLibrary.set_object_properties(obj,json.dumps(row['patch'])),
                  'Native style write failed. Inspect '+str(folder)+' before any retry.')
        changed=True
    after=snapshot(package)
    S.require(before['tree']==after['tree'] and before['slots']==after['slots'], 'Overlay styling changed hierarchy or layout slots.')
    allowed={row['target']:leaves(row['patch']) for row in operations}
    for name,old in before['objects'].items():
        differences=delta(old['values'],after['objects'][name]['values'])
        unknown=[path for path in differences if not any(path==a or path.startswith(a+'.') or a.startswith(path+'.') for a in allowed.get(name,[]))]
        S.require(not unknown,'Unexpected property changes on '+name+': '+str(unknown))
    for row in operations:
        S.require(S._contains(after['objects'][row['target']]['values'],row['patch']), 'Style read-back differs: '+row['target'])
    if changed:
        bp,_=S.defaults(package)
        S.ue().EditorAssetLibrary.set_metadata_tag(bp,OWNER_KEY,OWNER)
        S.ue().EditorAssetLibrary.set_metadata_tag(bp,S.HASH_KEY,desired['token_sha256'])
    return S.write(OUT/(package.rsplit('/',1)[-1]+'-applied.json'),
        {'asset':package,'changed':changed,'backup':str(folder),'tree_and_slots_preserved':True,
         'widget_count':after['tree']['info']['widgetCount'],'operations':len(operations),
         'native_compile_then_save_after_return':[package] if changed else [],'runtime_verified':False})


def verify_asset(package):
    S.require(package in ASSETS,'Outside overlay scope.')
    current=snapshot(package); baseline=read_baseline(package)
    S.require(current['tree']==baseline['tree'],'Widget tree differs from baseline.')
    for row in plan(write=False)['operations']:
        if row['asset']==package:
            S.require(S._contains(current['objects'][row['target']]['values'],row['patch']), 'Value mismatch: '+row['target'])
    # Slots, transforms, dimensions, visibility and non-style state compare with the read-back
    # from immediately before this asset's application; earlier module changes are not reverted.
    receipt=OUT/(package.rsplit('/',1)[-1]+'-applied.json')
    S.require(receipt.is_file(),'Apply receipt missing.')
    saved=json.loads(receipt.read_text(encoding='utf-8'))
    before=json.loads((Path(saved['backup'])/'live-before.json').read_text(encoding='utf-8'))
    S.require(current['slots']==before['slots'],'Slot geometry changed after application/compile.')
    return S.write(OUT/(package.rsplit('/',1)[-1]+'-verified.json'),
                   {'asset':package,'widget_count':current['tree']['info']['widgetCount'],'style_readback':True,
                    'tree_and_slots_preserved':True,'runtime_verified':False})


def gap_recipe(write=True):
    """Exact follow-on wiring contract; does not create nodes/widgets or claim it has run."""
    result={
      'style_stage_scope':'Only the property plan above is executable in this file. The following small owned-BP stages remain explicit.',
      'border_only_frame':{
        'assets':[QUICK+'/WBP_AZ_QuickSelectEntry',QUICK+'/WBP_AZ_QuickSelectFists'],
        'new_art_required':ART+'/T_FN_QuickFrame',
        'art_contract':'Native GIMP 104x72 PNG: RGB white everywhere, alpha255 only on a two-pixel rectangular perimeter, alpha0 interior; no baked fill/text/icon. Preserve existing actual cell dimensions.',
        'import_contract':'Owned UI texture, sRGB on, UI compression, no mipmaps, clamp both axes; inspect alpha and exact dimensions before assignment.',
        'stage_order':['Back up both owned packages and read their current HighlightBorder native schema.',
                       'Set only HighlightBorder.background drawAs=Image, resourceObject=T_FN_QuickFrame, tintColor=opaque white; retain brushColor driven by native state and retain its current placement.',
                       'Do not set RoundedBox or a fixed outline colour. Keep CardFill as the separately rendered backing.',
                       'Native compile/save outside Python, then verify native BrushColor still changes focus and only the frame alpha breathes during assignment.'],
        'engine_evidence':'SlateCore/Private/Rendering/DrawElementTypes.cpp:307-316 assigns RoundedBox outline RGB directly from OutlineSettings.Color; bUseBrushTransparency replaces its alpha with InTint.A, which is zero for transparent fill.',
        'pending_until':'Imported frame assigned and user-authorized runtime review confirms border-only pulse; this style-only pass preserves the original brush.'},
      'equipped_notch':{
        'asset':QUICK+'/WBP_AZ_QuickSelectEntry', 'add_only':'EquippedNotch / UImage under existing EntryCanvas',
        'slot':{'position':[44,69],'size':[16,3],'anchors':[0,0],'alignment':[0,0],'zOrder':4},
        'initial_visibility':'Collapsed', 'color':rgba('text'),
        'event':'Native BlueprintImplementableEvent OnEntryViewChanged(View)',
        'condition':'View.SlotIndex != 0 AND View.bEquipped; do not conflate View.bHovered, bReady, bEditing or bPending with equipped.',
        'effect':'Set only EquippedNotch visibility HitTestInvisible/Collapsed. Existing native border opacity pulse remains unchanged.',
        'after_verified':'Set the Entry CDO equippedColor to idle edge colour so steady equipment uses the notch, while hovered/assigning frame remains neutral bright.',
        'preserve':'If an event body exists, splice a Sequence ahead of its existing continuation; never replace it. No notch on slot0 mode BP.',
        'note':'Also mirror the notch to any independently authored item Entry subclass only if its current tree/class requires it; never regenerate eight slots.'},
      'category_upsert':{
        'asset':QUEST+'/WBP_AZ_QuestModule', 'event':'RuntimeMarkerUpsert(MarkerObject,Label,bPersonal)',
        'existing_profiles':['QuestCompassInfo','QuestWorldInfo','PersonalCompassInfo','PersonalWorldInfo'],
        'add_profiles':['SideCompassInfo : S_CompassMarkerInfo_H','SideWorldInfo : S_WorldMarkerInfo_H'],
        'side_profile_patches':{'SideCompassInfo':marker_profile('side'),'SideWorldInfo':marker_profile('side',True)},
        'stage_order':['Add only two typed variables; native compile outside Python.',
                       'Copy Quest profile values to Side profiles, then apply these style-only side patches; preserve scale/distance/arrow/ping/visibility fields.',
                       'Inspect actual RuntimeMarkerUpsert execution pins and locate existing bPersonal branch before changing links.',
                       'Leave personal branch unchanged. On quest branch, resolve BoundQuestMap.GetQuestProgress(), then GetTrackedQuestId(), then FindQuestDefinition(id), with object-valid guards.',
                       'Read Definition.Category once on this upsert event; EAZ_QuestCategory::Story selects existing Quest profiles, Side selects new Side profiles.',
                       'Both assign existing SelectedCompassInfo/SelectedWorldInfo locals and rejoin the existing bridge calls.'],
        'preserve':['bRuntimeBound guard','bridge_guard/local owner','MarkerObject validity','PublishedMarkerKeys AddUnique',
                    'original MarkerObject key','_world_info_with_label injection','both bridge upsert calls','all remove/shutdown callbacks'],
        'forbidden':['Tick lookup','global player/widget scan','widget-owned quest progress','RemoveAllMarkers','rewiring lifetime delegates'],
        'invalid_context':'Do not guess a category. Guard invalid progress/definition and leave normal removal/replay ownership intact; verify initialization and replay before accepting this stage.',
        'native_helpers':'Tools/quest_runtime_bindings.py RuntimeGraph, _world_info_with_label and _wire_upsert document the current chain; do not rerun its whole wire() to replace existing graphs.'},
      'critical_health':'The style stage retains current critical threshold and LOW HEALTH/NO HEALTH binding. Approved dark-red Overlay danger contrast needs user-run bright/dark checks; do not change thresholds or invent another health source.',
      'current_vs_destination':'GameHUD ModeNameText/FightModeIcon/ExploreModeIcon keep native current-mode rendering; Fists mode card keeps ExploreActionIcon/native inverse slot0 logic. Text samples are not assigned.',
      'runtime_gates':['HUD widget count37 and quick root33 preserved by style stage.',
                       'Eight existing104x72 cells, all slot/key assignments and actual fragment-assimilation leaves unchanged.',
                       'After notch/category stages, explicitly account only for the one new notch widget/two profile fields and their event nodes.',
                       'Compare focus+equipped, assignment pulse, Story/Side/Personal compass/world markers, HUD recreation and separate personal marker removal. No claim until user-authorized runtime review.']}
    return S.write(OUT/'remaining-state-category-recipe.json',result) if write else result


# Additive, separately compiled follow-up stages. Run style apply/verify first.
FOLLOW_KEY = 'AZ.FieldNotes.OverlayStage.'


def runtime_support():
    spec = importlib.util.spec_from_file_location('fn_runtime_support', ROOT/'Tools/quest_runtime_bindings.py')
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def graph_readback(bp, graph_name):
    editor = S.ue().BlueprintGraphEditor.get_graph_editor_by_name(bp,graph_name)
    if editor is None: return []
    lib = S.ue().BlueprintEditorLibrary
    result = []
    for node in editor.list_all_nodes():
        pins = []
        for output, values in ((False,lib.list_input_pins(node)),(True,lib.list_output_pins(node))):
            for pin in values:
                pins.append({'name':str(pin.get_pin_name()),'output':output,
                             'links':[{'node':other.get_owning_node().get_path_name(),'pin':str(other.get_pin_name())}
                                      for other in pin.list_connected_pins()]})
        result.append({'node':node.get_path_name(),'class':node.get_class().get_name(),
                       'title':str(lib.get_node_title(node)),'pins':pins})
    return result


def preserve_existing_graph(before,after,allowed_pins):
    """All old nodes/pins/links survive except the exact spliced connections."""
    indexed = {row['node']:row for row in after}
    for old in before:
        S.require(old['node'] in indexed,'An existing node was removed: '+old['node'])
        new = indexed[old['node']]
        S.require(old['class']==new['class'],'An existing node changed class.')
        oldpins = {(p['name'],p['output']):p for p in old['pins']}
        newpins = {(p['name'],p['output']):p for p in new['pins']}
        S.require(oldpins.keys()==newpins.keys(),'An existing pin was added/removed.')
        for key,pin in oldpins.items():
            if (old['node'],key[0],key[1]) not in allowed_pins:
                S.require(sorted(pin['links'],key=str)==sorted(newpins[key]['links'],key=str),
                          'Unexpected old link changed: '+old['node']+'.'+key[0])


def begin_followup(package, stage, graphs=()):
    S.require(package in ASSETS,'Follow-up package outside owned scope.')
    S.idle()
    bp,cdo = S.defaults(package)
    marker = S.ue().EditorAssetLibrary.get_metadata_tag(bp,FOLLOW_KEY+stage)
    S.require(marker in ('','v1'),'Interrupted '+stage+'; inspect backup and partial graph before retry.')
    if marker == 'v1': return bp,cdo,None
    folder = OUT/'BeforeWrites'/S.stamp()/(package.rsplit('/',1)[-1]+'-'+stage)
    S.write(folder/'live-before.json',snapshot(package))
    S.write(folder/'graphs-before.json',{name:graph_readback(bp,name) for name in graphs})
    shutil.copy2(S.package_file(package),folder/S.package_file(package).name)
    # Latch before first edit. Failure deliberately remains inspectable, not auto-retried.
    S.ue().EditorAssetLibrary.set_metadata_tag(bp,FOLLOW_KEY+stage,'writing')
    return bp,cdo,folder


def end_followup(bp,stage,folder,**values):
    S.ue().EditorAssetLibrary.set_metadata_tag(bp,FOLLOW_KEY+stage,'v1')
    return S.write(OUT/(bp.get_name()+'-'+stage+'.json'),
                   {'asset':bp.get_path_name(),'stage':stage,'backup':str(folder),
                    'native_compile_then_save_required':True,'runtime_verified':False,**values})


def patch_object(obj, patch):
    record = S.read_object(obj)
    S._validate_patch(record['schema'],patch,obj.get_path_name())
    S.require(obj.get_package().get_name() in ASSETS,'Object outside owned overlay packages.')
    S.require(S.ue().ToolsetLibrary.set_object_properties(obj,json.dumps(patch)),'Native property write failed.')
    S.require(S._contains(S.read_object(obj)['values'],patch),'Native property readback failed.')


def apply_quick_frame(package):
    """After importing T_FN_QuickFrame. Existing native colour/opacity drives the frame."""
    S.require(package in (QUICK+'/WBP_AZ_QuickSelectEntry',QUICK+'/WBP_AZ_QuickSelectFists'),'Wrong frame target.')
    texture = S.resolve(S.ref(ART+'/T_FN_QuickFrame'))
    S.require(isinstance(texture,S.ue().Texture2D),'Quick frame must be a Texture2D.')
    S.require(texture.blueprint_get_size_x()==104 and texture.blueprint_get_size_y()==72,'Quick frame dimensions changed.')
    bp,_,folder = begin_followup(package,'Frame')
    if folder is None: return {'already_applied':True}
    before = snapshot(package)
    obj = S.resolve(before['objects']['HighlightBorder']['object'])
    patch_object(obj,{'background':{'drawAs':'Image','resourceObject':S.ref(ART+'/T_FN_QuickFrame'),
                                   'resourceName':'None','tintColor':S.slate({'r':1,'g':1,'b':1,'a':1})}})
    after = snapshot(package)
    S.require(before['tree']==after['tree'] and before['slots']==after['slots'],'Frame stage changed geometry.')
    return end_followup(bp,'Frame',folder,native_colour_and_pulse_preserved=True)


def prepare_equipped_notch():
    """Adds one passive image to the item card only; compile externally before wiring."""
    package = QUICK+'/WBP_AZ_QuickSelectEntry'
    S.idle(); bp,_ = S.defaults(package)
    old = snapshot(package)
    S.require('EquippedNotch' not in old['objects'] or
              S.ue().EditorAssetLibrary.get_metadata_tag(bp,FOLLOW_KEY+'NotchTree')=='v1',
              'Unowned/partial EquippedNotch already exists.')
    bp,_,folder = begin_followup(package,'NotchTree')
    if folder is None: return {'already_prepared':True}
    q = runtime_support()
    root = old['objects']['EntryCanvas']['object']
    child = q.tool('UMGToolSet.UMGToolSet.AddWidget',widgetBlueprint=S.ref(package),
                   parentWidget={'refPath':root},widgetClass={'refPath':'/Script/UMG.Image'},widgetDisplayName='EquippedNotch')
    patch_object(S.resolve(child['widget']),{'visibility':'Collapsed','colorAndOpacity':rgba('text')})
    patch_object(S.resolve(child['slot']),{'layoutData':{'offsets':{'left':44,'top':69,'right':16,'bottom':3},
                 'anchors':{'minimum':{'x':0,'y':0},'maximum':{'x':0,'y':0}},'alignment':{'x':0,'y':0}},
                 'bAutoSize':False,'zOrder':4})
    q.tool('UMGToolSet.UMGToolSet.ToggleWidgetAsVariable',widgetBlueprint=S.ref(package),widget=child['widget'],bIsVariable=True)
    now = snapshot(package)
    S.require(set(now['objects'])-set(old['objects'])=={'EquippedNotch'},'Unexpected added widgets.')
    S.require(all(now['objects'][n]==v for n,v in old['objects'].items() if n!='@CDO'), 'Existing widget changed during notch addition.')
    S.require(all(now['slots'][n]==v for n,v in old['slots'].items()), 'Existing slot changed during notch addition.')
    return end_followup(bp,'NotchTree',folder,added_widgets=['EquippedNotch'],next='Native compile, then wire_equipped_notch().')


class AppendGraph:
    """Adds nodes to one existing graph; never clears it or recreates an entry."""
    def __init__(self,bp,name):
        self.bp,self.name = bp,name
        self.q = runtime_support(); self.b = self.q.support()
        self.editor = S.ue().BlueprintGraphEditor.get_graph_editor_by_name(bp,name)
        S.require(self.editor is not None,'Missing existing graph '+name)

    def call(this,owner,name,**values):
        # Concrete operators avoid the promotable-operator spawner hang recorded
        # in quest_mission_presentation.py. No generic math palette nodes.
        if owner=='/Script/Engine.KismetMathLibrary':
            before = {n.get_path_name() for n in this.editor.list_all_nodes()}
            cls = S.ue().load_class(None,owner)
            guid = S.ue().AZ_BlueprintNodeUtils.add_function_call_node(
                this.bp.get_package().get_name(),this.name,cls,name,0,0)
            nodes = [n for n in this.editor.list_all_nodes() if n.get_path_name() not in before]
            S.require(bool(guid) and len(nodes)==1 and nodes[0].get_class().get_name()=='K2Node_CallFunction','Concrete math spawn failed.')
            node = nodes[0]
        else:
            node = this.editor.add_call_function_node(this.b.function_path(owner,name))
        S.require(node is not None,'Missing function '+owner+':'+name)
        for key,value in values.items(): this.b.connect(value,this.b.inp(node,key))
        return node

    def get(self,name,local=False,owner=None,target=None):
        node = self.editor.add_get_local_variable_node(name) if local else self.editor.add_get_member_variable_node(name,owner or '')
        S.require(node is not None,'Missing getter '+name)
        if target is not None: self.b.connect(target,self.b.inp(node,'self'))
        return self.b.out(node,name)

    def set_local(self,name,execute,value):
        node = self.editor.add_set_local_variable_node(name)
        S.require(node is not None,'Missing existing local '+name)
        self.b.connect(value,self.b.inp(node,name)); self.b.connect(execute,self.b.inp(node,'execute'))
        return self.b.out(node,'then')

    def branch(self,execute,condition):
        node = self.editor.add_branch_node()
        self.b.connect(condition,self.b.inp(node,'Condition'))
        if execute is not None: self.b.connect(execute,self.b.inp(node,'execute'))
        return node,self.b.out(node,'then'),self.b.out(node,'else')

    def valid(self,execute,obj):
        condition = self.b.out(self.call('/Script/Engine.KismetSystemLibrary','IsValid',Object=obj))
        return self.branch(execute,condition)


def wire_equipped_notch():
    package = QUICK+'/WBP_AZ_QuickSelectEntry'
    S.idle(); bp,_ = S.defaults(package)
    S.require(S.ue().EditorAssetLibrary.get_metadata_tag(bp,FOLLOW_KEY+'NotchTree')=='v1','Prepare/native-compile notch first.')
    bp,cdo,folder = begin_followup(package,'NotchEvent',('EventGraph',))
    if folder is None: return {'already_wired':True}
    g = AppendGraph(bp,'EventGraph'); b = g.b
    event = g.editor.find_event_node('OnEntryViewChanged')
    if event is None: event = S.ue().BlueprintEditorLibrary.add_event_override(bp,'OnEntryViewChanged',S.ue().IntPoint(0,0))
    S.require(event is not None,'Native view-change event unavailable.')
    begin = b.out(event,'then'); following = list(begin.list_connected_pins())
    S.require(len(following)<=1,'Existing view event has unexpected fanout.')
    fields = g.q.action(g.editor,'Break AZ_QuickSelectEntryView','K2Node_BreakStruct')
    b.connect(b.out(event,'View'),b.inp(fields,'AZ_QuickSelectEntryView'))
    nonzero = g.call('/Script/Engine.KismetMathLibrary','NotEqual_IntInt',A=b.out(fields,'SlotIndex'))
    b.literal(b.inp(nonzero,'B'),'0')
    condition = g.call('/Script/Engine.KismetMathLibrary','BooleanAND',A=b.out(nonzero),B=b.out(fields,'bEquipped'))
    branch,yes,no = g.branch(None,b.out(condition))
    target = g.get('EquippedNotch')
    tails = []
    for execute,visibility in ((yes,'HitTestInvisible'),(no,'Collapsed')):
        node = g.call('/Script/UMG.Widget','SetVisibility',self=target)
        b.literal(b.inp(node,'InVisibility'),visibility); b.connect(execute,b.inp(node,'execute'))
        tails.append(b.out(node,'then'))
    # The existing continuation runs exactly once after either visibility choice.
    S.ue().BlueprintGraphPinLibrary.break_pin_links(begin)
    b.connect(begin,b.inp(branch,'execute'))
    for tail in tails:
        for pin in following: b.connect(tail,pin)
    patch_object(cdo,{'equippedColor':rgba('edge',.70)})
    before = json.loads((folder/'graphs-before.json').read_text(encoding='utf-8'))['EventGraph']
    after = graph_readback(bp,'EventGraph')
    allowed = {(event.get_path_name(),'then',True),(event.get_path_name(),'View',True)}
    allowed.update((p.get_owning_node().get_path_name(),str(p.get_pin_name()),False) for p in following)
    preserve_existing_graph(before,after,allowed)
    S.write(folder/'graphs-after.json',{'EventGraph':after})
    return end_followup(bp,'NotchEvent',folder,equipped_condition='SlotIndex != 0 && bEquipped',preserved_continuation=len(following))


def prepare_side_profiles():
    package = QUEST+'/WBP_AZ_QuestModule'
    S.idle(); bp,_ = S.defaults(package)
    lib = S.ue().BlueprintEditorLibrary
    names = {str(name) for name in lib.list_member_variable_names(bp,False)}
    stage = S.ue().EditorAssetLibrary.get_metadata_tag(bp,FOLLOW_KEY+'SideProfiles')
    S.require(stage=='v1' or not(names & {'SideCompassInfo','SideWorldInfo'}),'Side field collision; inspect ownership.')
    bp,_,folder = begin_followup(package,'SideProfiles')
    if folder is None: return {'already_prepared':True}
    q = runtime_support(); types = q.types()
    for name,kind in (('SideCompassInfo','Compass'),('SideWorldInfo','World')):
        S.require(lib.add_member_variable(bp,name,types[kind]),'Cannot add '+name)
        lib.set_blueprint_variable_category(bp,name,'Quest|Runtime|FieldNotes')
    return end_followup(bp,'SideProfiles',folder,next='Native compile, configure_side_profiles(), then wire_marker_categories().')


def configure_side_profiles():
    package = QUEST+'/WBP_AZ_QuestModule'
    bp,cdo,folder = begin_followup(package,'SideProfileValues')
    if folder is None: return {'already_configured':True}
    data = S.read_object(cdo)
    patch = {'sideCompassInfo':merge(data['values']['questCompassInfo'],marker_profile('side')),
             'sideWorldInfo':merge(data['values']['questWorldInfo'],marker_profile('side',True))}
    patch_object(cdo,patch)
    return end_followup(bp,'SideProfileValues',folder,preserved_policy_from='QuestCompassInfo/QuestWorldInfo')


def wire_marker_categories():
    """Splice only the non-personal arm of existing RuntimeMarkerUpsert."""
    package = QUEST+'/WBP_AZ_QuestModule'
    S.idle(); bp,_ = S.defaults(package)
    if S.ue().EditorAssetLibrary.get_metadata_tag(bp,FOLLOW_KEY+'CategoryEvent')=='v1':
        return {'already_wired':True}
    S.require(S.ue().EditorAssetLibrary.get_metadata_tag(bp,FOLLOW_KEY+'SideProfileValues')=='v1','Compile/configure Side profiles first.')
    # These are the verified native enum values; fail closed if the source changes.
    source = (ROOT/'Source/AZ/Public/Quests/AZ_QuestTypes.h').read_text(encoding='utf-8')
    S.require('enum class EAZ_QuestCategory : uint8 { Story, Side };' in source,'Category enum changed; inspect native value mapping.')
    g = AppendGraph(bp,'RuntimeMarkerUpsert'); b = g.b
    entry = g.editor.find_graph_entry_pin().get_owning_node()
    conditions = list(b.out(entry,'bPersonal').list_connected_pins())
    S.require(len(conditions)==1,'Expected one direct bPersonal condition.')
    personal_branch = conditions[0].get_owning_node()
    S.require(personal_branch.get_class().get_name()=='K2Node_IfThenElse' and str(conditions[0].get_pin_name())=='Condition','Unexpected personal branch topology.')
    quest_arm = b.out(personal_branch,'else')
    old = list(quest_arm.list_connected_pins())
    S.require(len(old)==1,'Expected one existing Quest profile arm.')
    first = old[0].get_owning_node()
    compass_input = b.inp(first,'SelectedCompassInfo')
    sourcepins = list(compass_input.list_connected_pins())
    S.require(len(sourcepins)==1 and str(sourcepins[0].get_pin_name())=='QuestCompassInfo','Existing Story profile assignment differs.')
    secondpins = list(b.out(first,'then').list_connected_pins())
    S.require(len(secondpins)==1,'Existing profile chain differs.')
    second = secondpins[0].get_owning_node()
    worldpins = list(b.inp(second,'SelectedWorldInfo').list_connected_pins())
    S.require(len(worldpins)==1 and str(worldpins[0].get_pin_name())=='QuestWorldInfo','Existing Story world profile differs.')
    join = list(b.out(second,'then').list_connected_pins())
    S.require(len(join)==1,'Expected one existing marker-upsert continuation.')
    bp,_,folder = begin_followup(package,'CategoryEvent',('RuntimeMarkerUpsert','RuntimeMarkerRemove','RuntimeShutdown'))
    if folder is None: return {'already_wired':True}
    nav = g.get('BoundQuestMap')
    guard,execute,_ = g.valid(None,nav)
    progress = b.out(g.call('/Script/AZ.AZ_QuestMapComponent','GetQuestProgress',self=nav))
    _,execute,_ = g.valid(execute,progress)
    tracked = b.out(g.call('/Script/AZ.AZ_QuestProgressComponent','GetTrackedQuestId',self=progress))
    definition = b.out(g.call('/Script/AZ.AZ_QuestProgressComponent','FindQuestDefinition',self=progress,QuestId=tracked))
    _,execute,_ = g.valid(execute,definition)
    category = g.get('Category',owner='/Script/AZ.AZ_QuestDefinition',target=definition)
    side_test = g.call('/Script/Engine.KismetMathLibrary','EqualEqual_ByteByte',A=category)
    b.literal(b.inp(side_test,'B'),'1')
    _,side,story = g.branch(execute,b.out(side_test))
    side = g.set_local('SelectedCompassInfo',side,g.get('SideCompassInfo'))
    side = g.set_local('SelectedWorldInfo',side,g.get('SideWorldInfo'))
    # All prior identity/owner/lifetime checks remain upstream. Both arms use the
    # existing shared compass + labelled-world upsert continuation downstream.
    b.connect(side,join[0]); b.connect(story,old[0])
    S.ue().BlueprintGraphPinLibrary.break_pin_links(quest_arm)
    b.connect(quest_arm,b.inp(guard,'execute'))
    before = json.loads((folder/'graphs-before.json').read_text(encoding='utf-8'))
    for name in ('RuntimeMarkerRemove','RuntimeShutdown'):
        S.require(graph_readback(bp,name)==before[name],'Unrelated marker lifetime graph changed.')
    after = graph_readback(bp,'RuntimeMarkerUpsert')
    allowed = {(personal_branch.get_path_name(),'else',True),
               (first.get_path_name(),str(old[0].get_pin_name()),False),
               (join[0].get_owning_node().get_path_name(),str(join[0].get_pin_name()),False)}
    preserve_existing_graph(before['RuntimeMarkerUpsert'],after,allowed)
    S.write(folder/'graphs-after.json',{'RuntimeMarkerUpsert':after})
    return end_followup(bp,'CategoryEvent',folder,personal_arm_unchanged=True,marker_key_unchanged=True,
                        event_driven=True,no_tick=True)


def verify_followups():
    """Read after external compile/save. No tick/PIE or behaviour claim."""
    result = {'runtime_verified':False,'widgets':{},'stages':{},'graph_errors':{}}
    expected = {HUD:37,QUICK+'/WBP_AZ_QuickSelect':33,
                QUICK+'/WBP_AZ_QuickSelectEntry':13,QUICK+'/WBP_AZ_QuickSelectFists':14}
    for package,count in expected.items():
        current = snapshot(package)
        S.require(current['tree']['info']['widgetCount']==count,'Unexpected widget count '+package)
        result['widgets'][package]=count
    for leaf in ('WBP_AZ_QuickSelectEntry','WBP_AZ_QuickSelectFists'):
        package = QUICK+'/'+leaf; bp,cdo = S.defaults(package); current=snapshot(package)
        S.require(S.ue().EditorAssetLibrary.get_metadata_tag(bp,FOLLOW_KEY+'Frame')=='v1','Frame stage missing.')
        expected_brush = {'drawAs':'Image','resourceObject':S.ref(ART+'/T_FN_QuickFrame'),
                          'tintColor':S.slate({'r':1,'g':1,'b':1,'a':1})}
        S.require(S._contains(current['objects']['HighlightBorder']['values']['background'],expected_brush),'Frame brush changed.')
        if leaf.endswith('Entry'):
            S.require(S.ue().EditorAssetLibrary.get_metadata_tag(bp,FOLLOW_KEY+'NotchEvent')=='v1','Notch event missing.')
            S.require(S._contains(S.read_object(cdo)['values'],{'equippedColor':rgba('edge',.70)}),'Equipped frame colour differs.')
            S.require(current['objects']['EquippedNotch']['values']['visibility']=='Collapsed','Notch must begin collapsed.')
            editor=S.ue().BlueprintGraphEditor.get_graph_editor_by_name(bp,'EventGraph')
            result['graph_errors'][package]=[n.get_path_name() for n in editor.list_nodes_with_errors()]
    package=QUEST+'/WBP_AZ_QuestModule'; bp,cdo=S.defaults(package)
    for stage in ('SideProfiles','SideProfileValues','CategoryEvent'):
        value=S.ue().EditorAssetLibrary.get_metadata_tag(bp,FOLLOW_KEY+stage)
        S.require(value=='v1','Missing category stage '+stage); result['stages'][stage]=value
    values=S.read_object(cdo)['values']
    for prefix,kind in (('quest','story'),('personal','personal'),('side','side')):
        for suffix,world in (('CompassInfo',False),('WorldInfo',True)):
            S.require(S._contains(values[prefix+suffix],marker_profile(kind,world)),'Marker profile style mismatch '+prefix+suffix)
    editor=S.ue().BlueprintGraphEditor.get_graph_editor_by_name(bp,'RuntimeMarkerUpsert')
    result['graph_errors'][package]=[n.get_path_name() for n in editor.list_nodes_with_errors()]
    S.require(not any(result['graph_errors'].values()),'Graph errors remain after external compile.')
    result['native_assignment_pulse']='Existing NativeTick and BrushColor path retained; user-run visual review remains.'
    return S.write(OUT/'followups-verified.json',result)
