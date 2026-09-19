# @Description: Dim the passive navigation host while the existing quick selector is open.
import gc
import importlib.util
from pathlib import Path
import unreal

spec = importlib.util.spec_from_file_location('az_compass_hud', Path('C:/UnrealEngine/Games/AZ/Tools/compass_hud_setup.py'))
H = importlib.util.module_from_spec(spec)
spec.loader.exec_module(H)
A, B, BL, GE = H.A, H.B, H.BL, H.GE


def prepare():
    A.idle()
    bp = A.load(H.HUD)
    names = set(BL.list_member_variable_names(bp, False))
    for name, kind in [('NavigationQuickSelectRef', BL.get_object_reference_type(unreal.AZ_QuickSelectComponent.static_class())),
                       ('bNavigationDimmed', BL.get_basic_type_by_name('bool'))]:
        if name not in names:
            A.require(BL.add_member_variable(bp, name, kind), 'Cannot create cached UI state')
    if BL.find_graph(bp, 'UpdateNavigationDimming') is None:
        GE.create_and_edit_function_graph(bp, 'UpdateNavigationDimming').set_function_is_private()
    return {'next': 'Native compile HUD, then wire().'}


def wire():
    A.idle()
    bp = A.load(H.HUD)
    if unreal.EditorAssetLibrary.get_metadata_tag(bp, 'AZ.Compass.QuickSelectDim') == 'v1':
        return {'state': 'already wired'}
    e = GE.get_graph_editor_by_name(bp, 'InitializeNavigationHost')
    owner = next(n for n in e.list_all_nodes() if A.normalized(A.title(n)) == 'getowningplayer')
    init = next(n for n in e.list_all_nodes() if A.normalized(A.title(n)) == 'initializenavigation')
    A.require(not B.out(init, 'then').list_connected_pins(), 'HUD initialization tail changed')
    get = e.add_call_function_node('/Script/Engine.Actor:GetComponentByClass')
    B.connect(B.out(owner), B.inp(get, 'self'))
    B.literal(B.inp(get, 'ComponentClass'), '/Script/AZ.AZ_QuickSelectComponent')
    cast, value = H.cast(e, H.HUD, 'InitializeNavigationHost', unreal.AZ_QuickSelectComponent.static_class())
    B.connect(B.out(get), B.inp(cast, 'Object'))
    B.connect(B.out(init, 'then'), B.inp(cast, 'execute'))
    set_ref = e.add_set_member_variable_node('NavigationQuickSelectRef')
    B.connect(B.out(cast, 'then'), B.inp(set_ref, 'execute'))
    B.connect(value, B.inp(set_ref, 'NavigationQuickSelectRef'))
    update = e.add_call_function_node('UpdateNavigationDimming')
    B.connect(B.out(set_ref, 'then'), B.inp(update, 'execute'))
    paths = {('KismetSystemLibrary', 'IsValid'): '/Script/Engine.KismetSystemLibrary:IsValid'}
    g = B.Graph(bp, 'UpdateNavigationDimming', paths)
    component = g.get('NavigationQuickSelectRef')
    yes, _ = g.branch(g.entry, g.valid(component))
    opened = g.call('/Script/AZ.AZ_QuickSelectComponent:IsOpen', self=component)
    neq = g.call('/Script/Engine.KismetMathLibrary:NotEqual_BoolBool', A=B.out(opened), B=g.get('bNavigationDimmed'))
    yes, _ = g.branch(yes, B.out(neq))
    yes = g.set('bNavigationDimmed', yes, value=B.out(opened))
    select = g.call('/Script/Engine.KismetMathLibrary:SelectFloat', bPickA=B.out(opened))
    B.literal(B.inp(select, 'A'), .45); B.literal(B.inp(select, 'B'), 1)
    opacity = g.call('/Script/UMG.Widget:SetRenderOpacity', self=g.get('NavigationHost'), InOpacity=B.out(select))
    B.connect(yes, B.inp(opacity, 'execute'))
    events = GE.get_graph_editor_by_name(bp, 'EventGraph')
    tick = next(n for n in events.list_all_nodes() if n.get_class().get_name() == 'K2Node_Event'
                and A.normalized(A.title(n)) == 'eventtick')
    A.require(not B.out(tick, 'then').list_connected_pins(), 'Existing Blueprint Tick must be preserved')
    call = events.add_call_function_node('UpdateNavigationDimming')
    B.connect(B.out(tick, 'then'), B.inp(call, 'execute'))
    unreal.EditorAssetLibrary.set_metadata_tag(bp, 'AZ.Compass.QuickSelectDim', 'v1')
    A.write('quickselect-dimming', {'opacity': .45, 'component_cached_once': True,
                                 'writes_only_on_state_change': True, 'no_input_changes': True})
    gc.collect()
    return {'state': 'wired; native compile/save required'}
