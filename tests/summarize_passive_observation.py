"""Create a path-free measured summary; never infer backend execution."""
import hashlib
import json
import pathlib
import sys

source, destination = map(pathlib.Path, sys.argv[1:])
raw = source.read_bytes()
data = json.loads(raw)
assert data['diagnostic_revision'] == 2
summary = {key: data[key] for key in (
    'schema_version', 'diagnostic_revision', 'started_utc', 'requested_duration_ms',
    'elapsed_ms', 'focus_interval_ms', 'window_interval_ms', 'module_interval_ms',
    'exit_code', 'limitations')}
summary['source_sha256'] = hashlib.sha256(raw).hexdigest()
summary['redactions'] = ['module paths', 'candidate inventory']
summary['targets'] = []
for target in data['targets']:
    item = {key: target[key] for key in (
        'pid', 'name', 'module_enumeration', 'complete_snapshots', 'focus',
        'focus_samples', 'window_samples', 'window_changes', 'errors', 'mechanisms')}
    item['unique_module_path_count'] = len(target['modules'])
    item['observed_module_names'] = sorted({m['name'].lower() for m in target['modules']})
    assert set(item['mechanisms'].values()) == {'UNKNOWN'}
    summary['targets'].append(item)
destination.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print('Saved', destination, 'targets', len(summary['targets']))