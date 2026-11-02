# Erzeugt resources/fonts/googlefonts.tsv: alle Google-Fonts-Familien mit Kategorie und Schnitten,
# damit TANGENTA beim PDF-Import fehlende Schriften direkt bei Google Fonts zeigen kann.
#
#   python tools/google-fonts-list.py

import json
import os
import urllib.request

with urllib.request.urlopen('https://fonts.google.com/metadata/fonts') as r:
    data = json.loads(r.read().decode('utf-8').lstrip(")]}'\n"))
rows = []
for f in data['familyMetadataList']:
    styles = ','.join(sorted(f.get('fonts', {}).keys()))
    rows.append('%s\t%s\t%s' % (f['family'], f.get('category', ''), styles))
target = os.path.join(os.path.dirname(__file__), '..', 'resources', 'fonts', 'googlefonts.tsv')
os.makedirs(os.path.dirname(target), exist_ok=True)
open(target, 'w', encoding='utf-8', newline='\n').write('\n'.join(sorted(rows, key=str.lower)) + '\n')
print(len(rows), 'Familien ->', os.path.abspath(target))
