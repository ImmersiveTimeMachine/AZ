"""File-only, hash-checked staging of the reviewed Field Notes native proposals.
prepare() never writes live Source. apply() is a separate explicit build-boundary step.
"""
import difflib
import hashlib
import json
import shutil
import subprocess
from pathlib import Path

ROOT = Path('C:/UnrealEngine/Games/AZ')
BASE = ROOT/'Saved/FieldNotesImplementation'
OUT = BASE/'CombinedNative'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def specs():
    result = []
    folder = BASE/'JournalNativeProposal'
    doc = json.loads((folder/'manifest.json').read_text())
    for row in doc['files']:
        result.append(('journal', row['path'], row['baseline_sha256'], folder/row['path'], row['proposal_sha256']))
    for part in ('Quick','Map'):
        folder = BASE/'UIInputNativeProposal'/part
        doc = json.loads((folder/'manifest.json').read_text())
        assert doc['version'] >= 3, 'Include reviewed input lifetime and presentation fixes'
        for row in doc['files']:
            result.append(('input-'+part, row['path'], row['live_sha256'], folder/'after'/row['path'], row['staged_sha256']))
    folder = BASE/'UIPreferencesNativeProposal'
    doc = json.loads((folder/'manifest.json').read_text())
    for row in doc['new_source_files']:
        result.append(('preferences', row['relative_path'], None, folder/row['relative_path'], row['staged_sha256']))
    folder = BASE/'MenuRoutesNativeProposal'
    doc = json.loads((folder/'manifest.json').read_text())
    for row in doc['source_files']:
        result.append(('menus', row['path'], row['baseline_sha256'], folder/row['path'], row['proposal_sha256']))
    folder = BASE/'InventoryNavigationNativeProposal'
    doc = json.loads((folder/'manifest.json').read_text())
    for row in doc['files']:
        result.append(('inventory-navigation', row['relative_path'], row['before_sha256'],
                       folder/row['relative_path'], row['after_sha256']))
    folder = BASE/'WorldInteractionPromptNativeProposal'
    doc = json.loads((folder/'manifest.json').read_text())
    for row in doc['files']:
        result.append(('world-interaction-prompts', row['path'], row['baseline_sha256'],
                       folder/'SourceProposal'/row['path'], row['proposal_sha256']))
    return result


def prepare():
    definitions = specs(); records = {}; final = {}
    # Validate every source/proposal before preparing or merging any output.
    for owner, relative, expected, proposal, proposed_hash in definitions:
        assert relative.startswith('Source/AZ/') and '..' not in Path(relative).parts
        assert proposal.is_file() and sha(proposal.read_bytes()) == proposed_hash, 'Proposal changed: '+str(proposal)
        live = ROOT/relative
        assert (not live.exists()) if expected is None else (live.is_file() and sha(live.read_bytes()) == expected), 'Live source changed: '+relative
    for owner, relative, expected, proposal, proposed_hash in definitions:
        live = ROOT/relative
        baseline = live.read_bytes() if live.exists() else b''
        text = proposal.read_text(encoding='utf-8-sig')
        if relative in final:
            assert records[relative]['before_sha256'] == expected, 'Proposals use different baselines'
            work = OUT/'MergeWork'/Path(relative).name; work.mkdir(parents=True, exist_ok=True)
            current, base, other = work/'current.txt', work/'base.txt', work/'other.txt'
            current.write_text(final[relative], encoding='utf-8', newline='\n')
            base.write_text(baseline.decode('utf-8-sig').replace('\r\n','\n'), encoding='utf-8', newline='\n')
            other.write_text(text, encoding='utf-8', newline='\n')
            merged = subprocess.run(['git','merge-file','--diff3','-p',str(current),str(base),str(other)],
                                    capture_output=True, text=True, encoding='utf-8')
            (work/'result.txt').write_text(merged.stdout, encoding='utf-8', newline='\n')
            assert merged.returncode == 0, 'Review staged merge conflict: '+str(work/'result.txt')
            text = merged.stdout
        else:
            records[relative] = {'before_sha256': expected, 'contributors': [], 'original_crlf': b'\r\n' in baseline}
            if live.exists():
                before = OUT/'BeforeSource'/relative; before.parent.mkdir(parents=True, exist_ok=True)
                if before.exists(): assert before.read_bytes() == baseline, 'Original baseline backup changed'
                else: before.write_bytes(baseline)
        records[relative]['contributors'].append(owner)
        final[relative] = text
    patch = []
    for relative, text in final.items():
        assert not any(line.startswith(('<<<<<<<','|||||||','=======','>>>>>>>')) for line in text.splitlines()), 'Unresolved merge marker'
        if relative.endswith('.h'):
            includes = [line for line in text.splitlines() if line.startswith('#include')]
            generated = [line for line in includes if '.generated.h' in line]
            assert not generated or includes[-1] == generated[-1], 'Generated include must be last: '+relative
        output = OUT/'SourceTree'/relative; output.parent.mkdir(parents=True, exist_ok=True)
        raw = text.replace('\r\n','\n').encode('utf-8')
        if records[relative]['original_crlf']: raw = raw.replace(b'\n', b'\r\n')
        output.write_bytes(raw); records[relative]['after_sha256'] = sha(raw)
        records[relative]['staged_file'] = str(output)
        before = (ROOT/relative).read_text(encoding='utf-8-sig') if (ROOT/relative).exists() else ''
        patch.extend(difflib.unified_diff(before.splitlines(True), text.splitlines(True),
                     fromfile='a/'+relative if before else '/dev/null', tofile='b/'+relative))
    (OUT/'combined-native.patch').write_text(''.join(patch), encoding='utf-8', newline='\n')
    manifest = {'files': records, 'source_applied': False, 'build_result': 'Not run',
                'new_files': sum(r['before_sha256'] is None for r in records.values()),
                'modified_files': sum(r['before_sha256'] is not None for r in records.values())}
    (OUT/'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    return {'files':len(records),'new_files':manifest['new_files'],'modified_files':manifest['modified_files'],
            'source_applied':False}


def apply():
    manifest = json.loads((OUT/'manifest.json').read_text(encoding='utf-8'))
    assert not manifest['source_applied'], 'Source batch already applied'
    for relative, row in manifest['files'].items():
        path = ROOT/relative
        assert (not path.exists()) if row['before_sha256'] is None else sha(path.read_bytes()) == row['before_sha256'], 'Source drift: '+relative
        assert sha(Path(row['staged_file']).read_bytes()) == row['after_sha256'], 'Staged file drift'
    for relative, row in manifest['files'].items():
        path = ROOT/relative; path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(row['staged_file'], path)
        assert sha(path.read_bytes()) == row['after_sha256']
    manifest['source_applied'] = True
    (OUT/'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    return {'applied_files':len(manifest['files']),'normal_build_required':True}


if __name__ == '__main__':
    import argparse
    p=argparse.ArgumentParser(); p.add_argument('stage',choices=('prepare','apply')); args=p.parse_args()
    print(json.dumps(prepare() if args.stage=='prepare' else apply(),indent=2))
