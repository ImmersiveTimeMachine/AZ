# @Description: Verify the refused inventory style write and checkpoint its unchanged asset.
"""Import-inert recovery for one known failed composite, not a general reset.

Root: checkpoint_before_save(); native compile; checkpoint_after_save().
After a normal metadata build/restart: resume_after_metadata_build().
The recovery never changes widget properties, graphs or existing completed work.
"""
import importlib.util
import difflib
import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path('C:/UnrealEngine/Games/AZ')
OUT = ROOT / 'Saved/FieldNotesImplementation/InventoryMetadata'
FAILED = '/Game/AZ/Blueprints/Menu/CommonInventory/CommonUI/UI/Widgets/ItemDescription/EditInstanceOnly/Composits/WBP_Inv_CommonUI_Composite_Accuracy'


def source_receipt():
    """File-only proof that the source change is three style metadata flags."""
    folder = ROOT / 'Source/AZ/Public/InventoryUI/Widgets/Composite'
    fields = {'AZ_Inv_CommonUI_LeafWidget_Text.h': ('TextStyle',),
              'AZ_Inv_CommonUI_LeafWidget_LabeledValue.h': ('LabelStyle', 'ValueStyle')}
    report = {'compiled': False, 'source_sha256': {}, 'changes': [], 'patch': str(OUT / 'inventory-style-metadata.patch')}
    diff = []
    for name, properties in fields.items():
        path = folder / name
        before = (OUT / 'BeforeSource' / name).read_text(encoding='utf-8-sig')
        after = path.read_text(encoding='utf-8-sig')
        expected = before
        for index, prop in enumerate(properties):
            old = '\tUPROPERTY(EditInstanceOnly, Category = "AZ|Inventory")\n\tTSubclassOf<UCommonTextStyle> ' + prop + ';'
            new = '\tUPROPERTY(EditAnywhere, Category = "AZ|Inventory")\n\tTSubclassOf<UCommonTextStyle> ' + prop + ';'
            if index == 0:
                new = '\t// Reusable styles must also be authorable on WidgetTree templates.\n' + new
            assert expected.count(old) == 1, prop
            expected = expected.replace(old, new)
        assert after == expected, 'Unexpected source changes: ' + name
        report['source_sha256'][str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
        report['changes'].append({'file': str(path), 'properties': list(properties),
                                  'from': 'EditInstanceOnly', 'to': 'EditAnywhere'})
        relative = path.relative_to(ROOT).as_posix()
        diff.extend(difflib.unified_diff(before.splitlines(True), after.splitlines(True),
                                       fromfile='a/' + relative, tofile='b/' + relative))
    OUT.mkdir(parents=True, exist_ok=True)
    Path(report['patch']).write_text(''.join(diff), encoding='utf-8')
    (OUT / 'source-receipt.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    return report


def support():
    spec = importlib.util.spec_from_file_location('fn_inventory_recovery_base', ROOT / 'Tools/field_notes_inventory_setup.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def verify_refused_write():
    m = support(); s = m.support(); s.idle()
    progress_path = m.OUT / 'author-progress.json'
    progress = json.loads(progress_path.read_text(encoding='utf-8'))
    record = progress['assets'].get(FAILED)
    m.require(record and record['state'] == 'writing', 'Not the known interrupted composite stage')
    before = json.loads((Path(record['backup']) / 'live-before.json').read_text(encoding='utf-8'))
    m.require(before['asset'] == FAILED, 'Wrong attempt backup')
    bp, _ = s.defaults(FAILED)
    tree = s.native_tool('UMGToolSet.UMGToolSet.GetWidgets', widgetBlueprint=s.ref(FAILED))
    m.require(m._tree_identity(tree) == m._tree_identity(before['snapshot']['tree']), 'Composite tree changed')
    current = []
    for old in before['objects']:
        live = s.read_object(s.resolve(old['object']))
        m.require(m._exact_value(live['values'], old['values']), 'Refused operation changed a property: ' + old['object'])
        current.append(live)
    return m, s, bp, progress, current


def checkpoint_before_save():
    m, s, bp, progress, current = verify_refused_write()
    OUT.mkdir(parents=True, exist_ok=True)
    disk = s.package_file(FAILED)
    backup = OUT / 'CompositeBeforeSave.uasset'
    if not backup.exists():
        shutil.copy2(disk, backup)
    return s.write(OUT / 'composite-before-save.json', {
        'asset': FAILED, 'all_attempt_objects_unchanged': True,
        'objects': current, 'saved_package_sha256': s.digest(disk),
        'backup': str(backup), 'source_metadata_build_pending': True})


def checkpoint_after_save():
    m, s, bp, progress, current = verify_refused_write()
    m.require((OUT / 'composite-before-save.json').is_file(), 'Missing precompile proof')
    m.require(s.ue().EditorAssetLibrary.save_asset(FAILED, only_if_is_dirty=False), 'Save failed')
    return s.write(OUT / 'composite-refused-write-saved.json', {
        'asset': FAILED, 'all_attempt_objects_unchanged': True,
        'saved_package_sha256': s.digest(s.package_file(FAILED)),
        'saved': True, 'source_metadata_build_pending': True})


def resume_after_metadata_build():
    m, s, bp, progress, current = verify_refused_write()
    build = json.loads((OUT / 'build-receipt.json').read_text(encoding='utf-8'))
    m.require(build.get('result') == 'Succeeded', 'Normal metadata build is not verified')
    for path, digest in build['source_sha256'].items():
        m.require(s.digest(path) == digest, 'Source differs from the verified metadata build')
    saved = json.loads((OUT / 'composite-refused-write-saved.json').read_text(encoding='utf-8'))
    digest = s.digest(s.package_file(FAILED))
    m.require(digest == saved['saved_package_sha256'], 'Composite changed since verified checkpoint')
    # Keep the original attempt and receipts. Only this refused stage is reopened.
    history = OUT / ('Resume-' + s.stamp())
    s.write(history / 'old-progress.json', progress)
    recovery = progress['assets'].pop(FAILED)
    progress.setdefault('recoveries', []).append({
        'asset': FAILED, 'reason': 'EditInstanceOnly style metadata corrected by normal build',
        'original_attempt': recovery, 'full_attempt_values_unchanged': True,
        'receipt': str(history)})
    # Honor the same verified-resave table used by the normal full-property guard.
    hashes_path = m.OUT / 'accepted-resaves.json'
    hashes = json.loads(hashes_path.read_text(encoding='utf-8'))
    m.require(isinstance(hashes.get('assets'), dict), 'Unexpected verified-resave schema')
    _, snapshots = m.baseline()
    hashes['assets'][FAILED] = {
        'asset': FAILED, 'original_baseline_sha256': snapshots[FAILED]['saved_package_sha256'],
        'accepted_saved_sha256': digest,
        'baseline_manifest_sha256': m.plan()['baseline_manifest_sha256'],
        'proof_plan_sha256': progress['plan_sha256'], 'backup': str(history),
        'verification': 'All objects unchanged from refused-write backup; compiled and saved before restart'}
    hashes.setdefault('history', []).append({'receipt': str(history), 'records': [hashes['assets'][FAILED]]})
    s.write(history / 'old-verified-resaves.json', json.loads(hashes_path.read_text(encoding='utf-8')))
    s.write(hashes_path, hashes)
    s.write(m.OUT / 'author-progress.json', progress)
    return {'recovered': FAILED, 'completed_assets_preserved': len(progress['assets']), 'uobject_writes': False}


def configure_caption_variant():
    """Configure one explicitly owned variant; base resource allowlist stays intact."""
    m = support(); s = m.support(); s.idle()
    target = s.DEST + '/Text/TS_FN_Paper_Caption_Story'
    bp, cdo = s.defaults(target)
    m.require(s.ue().EditorAssetLibrary.get_metadata_tag(bp, s.OWNER_KEY) == s.OWNER, 'Unowned caption variant')
    before = s.read_object(cdo)
    patch = s.text_patch('Paper', 'Caption'); patch['color'] = s.linear('Paper', 'story')
    s._validate_patch(before['schema'], patch)
    s.write(OUT / 'caption-variant-before.json', before)
    m.require(s.ue().ToolsetLibrary.set_object_properties(cdo, json.dumps(patch)), 'Caption variant write failed')
    m.require(s._contains(s.read_object(cdo)['values'], patch), 'Caption variant readback failed')
    return {'configured': target, 'native_compile_save_required': True}


def apply_caption_variant():
    """Migrate only one style reference and preserve all completed assets."""
    import copy
    m = support(); s = m.support(); s.idle()
    old = json.loads((m.OUT / 'inventory-plan.json').read_text(encoding='utf-8'))
    new = m.plan(); progress = json.loads((m.OUT / 'author-progress.json').read_text(encoding='utf-8'))
    preflight = json.loads((m.OUT / 'preflight.json').read_text(encoding='utf-8'))
    m.require(old['plan_sha256'] == progress['plan_sha256'] == preflight['plan_sha256'], 'Plan histories disagree')
    m.require(all(x['state'] == 'authored' for x in progress['assets'].values()), 'Finish asset authoring first')
    m.require(len(old['operations']) == len(new['operations']), 'Variant migration changed operation count')
    changed = []
    for previous, desired in zip(old['operations'], new['operations']):
        if m._exact_value(previous, desired):
            continue
        m.require(previous['asset'] == m.SKILLS and previous['widget'] == 'UpgradeNotificationText', 'Unexpected changed target')
        expected = copy.deepcopy(previous)
        expected['patch']['style'] = {'refPath': s.object_path(s.DEST + '/Text/TS_FN_Paper_Caption_Story') + '_C'}
        m.require(m._exact_value(expected, desired), 'Variant changed more than its style reference')
        changed.append(desired)
    m.require(len(changed) == 1, 'Expected exactly one caption-style replacement')
    target = changed[0]
    # CommonTextBlock reloaded the Caption CDO color; prove that exact known
    # difference while retaining the complete-property guard for everything else.
    validate_old = copy.deepcopy(old)
    op = next(x for x in validate_old['operations'] if x['object'] == target['object'])
    op['patch']['colorAndOpacity'] = s.slate(s.linear('Paper', 'text'))
    _, snapshots = m.baseline()
    m._require_live_baseline(m.SKILLS, validate_old, snapshots, completed=True)
    backup = OUT / ('CaptionMigration-' + s.stamp())
    s.write(backup / 'old-plan.json', old); s.write(backup / 'old-progress.json', progress)
    s.write(backup / 'old-preflight.json', preflight)
    shutil.copy2(s.package_file(m.SKILLS), backup / s.package_file(m.SKILLS).name)
    obj = s.resolve(target['object']); obj.modify()
    m.require(s.ue().ToolsetLibrary.set_object_properties(obj, json.dumps(target['patch'])), 'Caption assignment failed')
    m._require_live_baseline(m.SKILLS, new, snapshots, completed=True)
    progress['plan_sha256'] = new['plan_sha256']; preflight['plan_sha256'] = new['plan_sha256']
    s.write(m.OUT / 'inventory-plan.json', new)
    s.write(m.OUT / 'author-progress.json', progress); s.write(m.OUT / 'preflight.json', preflight)
    return {'asset': m.SKILLS, 'new_plan_sha256': new['plan_sha256'], 'preserved_assets': len(progress['assets']),
            'backup': str(backup), 'native_compile_save_required': True}
