#!/usr/bin/env python3
"""Erzeugt die statische Webseite tangenta.ch in vier Sprachen.

    python website/src/build.py

Vorlage hier, Texte je Sprache in i18n/<sprache>.py. Deutsch liegt im Wurzelordner, die anderen
Sprachen in en/, fr/ und it/. Alle Sprachen haben dieselben Seiten mit denselben Dateinamen, damit
der Sprachwechsel immer auf die gleiche Seite führt.
"""
import html
import importlib.util
import json
import os
import re
from html.parser import HTMLParser
from urllib.parse import urljoin

HERE = os.path.dirname(os.path.abspath(__file__))
SITE = os.path.dirname(HERE)
LANGS = ['de', 'en', 'fr', 'it']
PAGES = ['index', 'features', 'ai', 'download']
DOMAIN = 'https://tangenta.ch'
GITHUB = 'https://github.com/roli8200/tangenta'
RELEASES = GITHUB + '/releases/latest'
# Direkte Links auf die Dateien der aktuellen Version (GitHub ersetzt ~ im Namen durch .)
ASSETS = GITHUB + '/releases/download/v{}/'
VERSION = '0.86'
DEBS = ['debian12', 'debian13', 'ubuntu24.04', 'ubuntu26.04']
DEB_NAMES = {'debian12': 'Debian 12', 'debian13': 'Debian 13', 'ubuntu24.04': 'Ubuntu 24.04', 'ubuntu26.04': 'Ubuntu 26.04'}
PUBLISHER = {'@type': 'Organization', 'name': 'Intersoft Networks', 'url': 'https://intersoft-networks.ch'}
# Vorschaubild je Seite (Open Graph, Twitter, JSON-LD)
OG_IMAGE = {'index': '01-hauptfenster.png', 'features': '06-andockfenster.png', 'ai': '02-ki-assistent.png',
            'download': '01-hauptfenster.png'}


def load(lang):
    spec = importlib.util.spec_from_file_location(lang, os.path.join(HERE, 'i18n', lang + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.T


def prefix(lang):
    return '' if lang == 'de' else '../'


def page_url(lang, page):
    folder = '' if lang == 'de' else lang + '/'
    return DOMAIN + '/' + folder + ('' if page == 'index' else page + '.html')


def rel(lang, target_lang, page):
    """Relativer Link von einer Seite in lang auf page in target_lang."""
    up = prefix(lang)
    folder = '' if target_lang == 'de' else target_lang + '/'
    return up + folder + page + '.html'


def esc(s):
    return html.escape(s, quote=True)


def md_url(lang, page):
    """Markdown-Fassung einer Seite (llmstxt.org: gleiche Adresse mit angehängtem .md)."""
    folder = '' if lang == 'de' else lang + '/'
    return DOMAIN + '/' + folder + page + '.html.md'


def json_ld(t, lang, page, title, description):
    """Strukturierte Daten: Herausgeber, Webseite und auf Start- und Downloadseite das Programm."""
    image = f'{DOMAIN}/img/{OG_IMAGE[page]}'
    graph = [
        dict(PUBLISHER, **{'@id': PUBLISHER['url'] + '/#organization', 'logo': f'{DOMAIN}/img/tangenta-256.png'}),
        {'@type': 'WebPage', '@id': page_url(lang, page) + '#webpage', 'url': page_url(lang, page), 'name': title,
         'description': description, 'inLanguage': lang, 'primaryImageOfPage': image,
         'isPartOf': {'@type': 'WebSite', '@id': DOMAIN + '/#website', 'url': DOMAIN + '/', 'name': 'TANGENTA',
                      'publisher': {'@id': PUBLISHER['url'] + '/#organization'}}},
    ]
    if page in ('index', 'download'):
        graph.append({
            '@type': 'SoftwareApplication', '@id': DOMAIN + '/#software', 'name': 'TANGENTA',
            'description': t['meta']['index'][1], 'applicationCategory': 'DesignApplication',
            'applicationSubCategory': 'Vector graphics editor',
            'operatingSystem': 'Windows 10, Windows 11, Debian 12, Debian 13, Ubuntu 24.04, Ubuntu 26.04',
            'softwareVersion': VERSION, 'license': 'https://www.gnu.org/licenses/gpl-3.0.html',
            'isAccessibleForFree': True, 'downloadUrl': GITHUB + f'/releases/tag/v{VERSION}', 'codeRepository': GITHUB,
            'screenshot': [f'{DOMAIN}/img/{n}' for n in ('01-hauptfenster.png', '02-ki-assistent.png', '03-supertrace.png')],
            'image': f'{DOMAIN}/img/tangenta-256.png', 'inLanguage': LANGS, 'url': page_url(lang, 'index'),
            'offers': {'@type': 'Offer', 'price': '0', 'priceCurrency': 'CHF'},
            'publisher': {'@id': PUBLISHER['url'] + '/#organization'}})
    data = json.dumps({'@context': 'https://schema.org', '@graph': graph}, ensure_ascii=False, indent=1)
    return '<script type="application/ld+json">\n' + data.replace('</', '<\\/') + '\n</script>'


def head(t, lang, page, title, description):
    alts = '\n'.join(f'  <link rel="alternate" hreflang="{l}" href="{page_url(l, page)}">' for l in LANGS)
    other_locales = '\n'.join(f'  <meta property="og:locale:alternate" content="{LOCALES[l]}">' for l in LANGS if l != lang)
    p = prefix(lang)
    image = f'{DOMAIN}/img/{OG_IMAGE[page]}'
    return f'''<!doctype html>
<html lang="{lang}">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{esc(title)}</title>
  <meta name="description" content="{esc(description)}">
  <meta name="robots" content="index, follow, max-image-preview:large">
  <meta name="theme-color" content="#0b1020">
  <link rel="canonical" href="{page_url(lang, page)}">
{alts}
  <link rel="alternate" hreflang="x-default" href="{page_url('de', page)}">
  <link rel="alternate" type="text/markdown" href="{md_url(lang, page)}" title="Markdown">
  <meta property="og:type" content="website">
  <meta property="og:site_name" content="TANGENTA">
  <meta property="og:url" content="{page_url(lang, page)}">
  <meta property="og:title" content="{esc(title)}">
  <meta property="og:description" content="{esc(description)}">
  <meta property="og:image" content="{image}">
  <meta property="og:image:alt" content="{esc(title)}">
  <meta property="og:locale" content="{t['locale']}">
{other_locales}
  <meta name="twitter:card" content="summary_large_image">
  <meta name="twitter:title" content="{esc(title)}">
  <meta name="twitter:description" content="{esc(description)}">
  <meta name="twitter:image" content="{image}">
  <link rel="icon" href="{p}img/favicon.png">
  <link rel="apple-touch-icon" href="{p}img/tangenta-256.png">
  <link rel="stylesheet" href="{p}styles.css">
  {json_ld(t, lang, page, title, description)}
</head>
<body>
'''


def nav(t, lang, page):
    n = t['nav']
    links = [('features', n['features']), ('ai', n['ai']), ('download', n['download'])]
    items = ''.join(f'<a href="{rel(lang, lang, k)}"{" aria-current=\"page\"" if k == page else ""}>{v}</a>' for k, v in links)
    items += f'<a href="{GITHUB}">GitHub</a>'
    langs = ''.join(f'<a href="{rel(lang, l, page)}" hreflang="{l}" lang="{l}"{" aria-current=\"true\"" if l == lang else ""}>'
                    f'<b>{l.upper()}</b> {LANG_NAMES[l]}</a>' for l in LANGS)
    return f'''<header class="nav">
  <a class="brand" href="{rel(lang, lang, 'index')}"><img src="{prefix(lang)}img/tangenta.svg" alt="" width="32" height="32"><span>TANGENTA</span></a>
  <nav class="links">{items}</nav>
  <details class="lang"><summary aria-label="{n['language']}">{lang.upper()}</summary><div class="lang-menu">{langs}</div></details>
  <a class="btn btn-primary btn-small" href="{rel(lang, lang, 'download')}">{n['cta']}</a>
  <details class="menu"><summary aria-label="{n['menu']}"><span></span><span></span><span></span></summary><div class="menu-panel">{items}</div></details>
</header>
'''


def footer(t, lang):
    f = t['footer']
    r = lambda k: rel(lang, lang, k)
    return f'''<footer class="footer">
  <div class="footer-cols">
    <div>
      <a class="brand" href="{r('index')}"><img src="{prefix(lang)}img/tangenta.svg" alt="" width="28" height="28"><span>TANGENTA</span></a>
      <p>{f['claim']}</p>
    </div>
    <div><h4>{f['program']}</h4><a href="{r('features')}">{t['nav']['features']}</a><a href="{r('ai')}">{t['nav']['ai']}</a><a href="{r('download')}">{t['nav']['download']}</a></div>
    <div><h4>{f['project']}</h4><a href="{GITHUB}">GitHub</a><a href="{GITHUB}/blob/main/BUILD.md">{f['build']}</a><a href="{GITHUB}/issues">{f['issues']}</a></div>
    <div><h4>{f['publisher']}</h4><a href="https://intersoft-networks.ch">Intersoft Networks</a><a href="{GITHUB}/blob/main/LICENSE">{f['license']}</a></div>
  </div>
  <p class="fine">© 2026 Intersoft Networks · {f['fine']}</p>
</footer>

</body>
</html>
'''


def img(lang, name, alt, cls='shot', eager=False):
    return (f'<figure class="{cls}"><img src="{prefix(lang)}img/{name}" alt="{esc(alt)}" '
            f'loading="{"eager" if eager else "lazy"}"></figure>')


def label(num, text):
    return f'<p class="label"><b>{num}</b> / {text}</p>'


def ticks(items):
    return '<ul class="ticks">' + ''.join(f'<li>{i}</li>' for i in items) + '</ul>'


def faq(items):
    return ''.join(f'<details><summary>{q}</summary><p>{a}</p></details>' for q, a in items)


def asset(name):
    return ASSETS.format(VERSION) + name


def deb_file(target):
    return f'tangenta_{VERSION}-1.{target}_amd64.deb'


def downloads_cards(t, lang):
    d = t['download_cards']
    debs = ''.join(f'<li><a class="more" href="{asset(deb_file(x))}">{DEB_NAMES[x]}</a></li>' for x in DEBS)
    return f'''<div class="downloads">
      <article class="card">
        <h3>{d['win_h']}</h3>
        <p>{d['win_p']}</p>
        <a class="btn btn-primary" href="{asset(f'TANGENTA-{VERSION}-x64-setup.exe')}">{d['win_btn']}</a>
        <p><a class="more" href="{asset(f'TANGENTA-{VERSION}-x64.msi')}">{d['msi']} →</a></p>
        <pre><code>TANGENTA-{VERSION}-x64-setup.exe /quiet
msiexec /i TANGENTA-{VERSION}-x64.msi /qn</code></pre>
      </article>
      <article class="card">
        <h3>{d['lin_h']}</h3>
        <p>{d['lin_p']}</p>
        <a class="btn btn-primary" href="{asset(deb_file('debian13'))}">{d['lin_btn']} (Debian 13)</a>
        <p>{d['debs_h']}</p>
        <ul class="dl-list">{debs}</ul>
        <pre><code>sudo apt install ./{deb_file('debian13')}</code></pre>
      </article>
      <article class="card">
        <h3>{d['flat_h']}</h3>
        <p>{d['flat_p']}</p>
        <a class="btn btn-primary" href="{asset(f'TANGENTA-{VERSION}-x86_64.flatpak')}">{d['flat_btn']}</a>
        <pre><code>flatpak install --user TANGENTA-{VERSION}-x86_64.flatpak
flatpak run ch.tangenta.Tangenta</code></pre>
      </article>
      <article class="card">
        <h3>GitHub</h3>
        <p><a class="more" href="{GITHUB}/releases/tag/v{VERSION}">{d['all']} →</a></p>
        <a class="btn btn-primary" href="{asset(f'SHA256SUMS-{VERSION}.txt')}">SHA256SUMS-{VERSION}.txt</a>
        <pre><code>sha256sum --ignore-missing -c SHA256SUMS-{VERSION}.txt</code></pre>
      </article>
    </div>'''


# ---------------------------------------------------------------- Seiten

def home(t, lang):
    h = t['home']
    hero = h['hero']
    ticker = ''.join(f'<span>{x}</span>' for x in h['ticker']) * 2
    stats = ''.join(f'<div><b>{n}</b><span>{s}</span></div>' for n, s in h['stats'])
    keys = ''.join(f'<span>{k} {v}</span>' for k, v in h['familiar']['keys'])
    cols = ''.join(f'<div><h3>{a}</h3><p>{b}</p></div>' for a, b in h['familiar']['cols'])
    cards = ''.join(f'<article class="card {c}"><span class="num">{i + 1:02d}</span><h3>{a}</h3><p>{b}</p></article>'
                    for i, (a, b, c) in enumerate(h['tools']['cards']))
    acc = h['tools']['accent']
    free = ''.join(f'<div><span class="letter">{"ABC"[i]}</span><h3>{a}</h3><p>{b}</p></div>' for i, (a, b) in enumerate(h['free']['cols']))
    who = ''.join(f'<div class="card"><h3>{a}</h3><p>{b}</p></div>' for a, b in h['who']['cards'])
    chips = ''.join(f'<span>{c}</span>' for c in t['providers_short'])
    r = lambda k: rel(lang, lang, k)
    return f'''<main id="top">

  <section class="hero">
    <p class="eyebrow">{hero['eyebrow']}</p>
    <h1>{hero['h1a']}<br><span class="grad">{hero['h1b']}</span></h1>
    <p class="lead">{hero['lead']}</p>
    <div class="ctas">
      <a class="btn btn-primary" href="{r('download')}">{hero['cta']}</a>
      <a class="btn btn-ghost" href="{GITHUB}">{hero['cta2']}</a>
    </div>
    <p class="platforms">Windows 10 / 11 · Debian 12 / 13 · Ubuntu 24.04 / 26.04</p>
    <p class="note">{hero['note']}</p>
    <figure class="window">
      <div class="window-bar"><i></i><i></i><i></i><span>{hero['window']}</span></div>
      <img src="{prefix(lang)}img/01-hauptfenster.png" alt="{esc(hero['alt'])}" loading="eager">
    </figure>
  </section>

  <div class="ticker" aria-hidden="true"><div class="ticker-track">{ticker}</div></div>

  <section class="section stats">{stats}</section>

  <section class="section">
    {label('01', h['familiar']['label'])}
    <h2>{h['familiar']['h2']}</h2>
    <p class="sub">{h['familiar']['sub']}</p>
    <div class="keys">{keys}</div>
    <div class="cols3">{cols}</div>
  </section>

  <section class="section split">
    <div class="split-text">
      {label('02', h['ai']['label'])}
      <h2>{h['ai']['h2']}</h2>
      <p class="sub">{h['ai']['sub']}</p>
      {ticks(h['ai']['ticks'])}
      <div class="chips">{chips}</div>
      <p><a class="more" href="{r('ai')}">{h['ai']['more']} →</a></p>
    </div>
    {img(lang, '02-ki-assistent.png', h['ai']['alt'])}
  </section>

  <section class="section">
    {label('03', h['tools']['label'])}
    <h2>{h['tools']['h2']}</h2>
    <div class="bento">{cards}
      <article class="card accent full"><span class="num">{len(h['tools']['cards']) + 1:02d}</span><h3>{acc[0]}</h3><p>{acc[1]}</p><a href="{r('features')}">{acc[2]} →</a></article>
    </div>
  </section>

  <section class="section split reverse">
    <div class="split-text">
      {label('04', 'SuperTrace')}
      <h2>{h['trace']['h2']}</h2>
      <p class="sub">{h['trace']['sub']}</p>
    </div>
    {img(lang, '03-supertrace.png', h['trace']['alt'])}
  </section>

  <section class="section split">
    <div class="split-text">
      {label('05', h['search']['label'])}
      <h2>{h['search']['h2']}</h2>
      <p class="sub">{h['search']['sub']}</p>
    </div>
    {img(lang, '04-bildersuche.png', h['search']['alt'])}
  </section>

  <section class="section split reverse">
    <div class="split-text">
      {label('06', h['print']['label'])}
      <h2>{h['print']['h2']}</h2>
      <p class="sub">{h['print']['sub']}</p>
    </div>
    {img(lang, '07-ausschiessen.png', h['print']['alt'])}
  </section>

  <section class="section">
    {label('07', h['free']['label'])}
    <h2>{h['free']['h2']}</h2>
    <div class="cols3 lettered">{free}</div>
  </section>

  <section class="section">
    {label('08', h['who']['label'])}
    <h2>{h['who']['h2']}</h2>
    <div class="cols4">{who}</div>
  </section>

  <section id="download" class="section">
    {label('09', t['nav']['download'])}
    <h2>{h['download_h2']}</h2>
    {downloads_cards(t, lang)}
    <p class="center"><a class="more" href="{r('download')}">{h['download_more']} →</a></p>
  </section>

  <section id="faq" class="section narrow">
    {label('10', h['faq']['label'])}
    <h2>{h['faq']['h2']}</h2>
    {faq(h['faq']['items'])}
  </section>

  {final(t, lang)}

</main>
'''


def final(t, lang):
    f = t['final']
    return f'''<section class="final">
    <h2>{f['h2']}</h2>
    <p>{f['p']}</p>
    <div class="ctas">
      <a class="btn btn-primary" href="{rel(lang, lang, 'download')}">{t['nav']['cta']}</a>
      <a class="btn btn-ghost" href="{GITHUB}">{f['gh']}</a>
    </div>
  </section>'''


def page_hero(p):
    return f'''<section class="hero page-hero">
    <p class="eyebrow">{p['eyebrow']}</p>
    <h1>{p['h1']}</h1>
    <p class="lead">{p['lead']}</p>
  </section>'''


def features(t, lang):
    f = t['features']
    groups = ''.join(f'<article class="card"><h3>{g}</h3><ul class="dots">{"".join(f"<li>{i}</li>" for i in items)}</ul></article>'
                     for g, items in f['groups'])
    keys = ''.join(f'<tr><td>{k}</td><td>{v}</td></tr>' for k, v in f['shortcuts']['rows'])
    fmt_in = ''.join(f'<span>{x}</span>' for x in f['formats']['import'])
    fmt_out = ''.join(f'<span>{x}</span>' for x in f['formats']['export'])
    return f'''<main id="top">
  {page_hero(f['hero'])}

  <section class="section">
    <div class="grid3">{groups}</div>
  </section>

  <section class="section split">
    <div class="split-text">
      {label('01', f['docks']['label'])}
      <h2>{f['docks']['h2']}</h2>
      <p class="sub">{f['docks']['sub']}</p>
      {ticks(f['docks']['ticks'])}
    </div>
    {img(lang, '06-andockfenster.png', f['docks']['alt'])}
  </section>

  <section class="section split reverse">
    <div class="split-text">
      {label('02', f['options']['label'])}
      <h2>{f['options']['h2']}</h2>
      <p class="sub">{f['options']['sub']}</p>
    </div>
    {img(lang, '08-optionen.png', f['options']['alt'])}
  </section>

  <section class="section">
    {label('03', f['formats']['label'])}
    <h2>{f['formats']['h2']}</h2>
    <div class="downloads">
      <article class="card"><h3>{f['formats']['import_h']}</h3><div class="chips plain">{fmt_in}</div></article>
      <article class="card"><h3>{f['formats']['export_h']}</h3><div class="chips plain">{fmt_out}</div></article>
    </div>
    <p class="sub small">{f['formats']['note']}</p>
  </section>

  <section class="section narrow">
    {label('04', f['shortcuts']['label'])}
    <h2>{f['shortcuts']['h2']}</h2>
    <p class="sub">{f['shortcuts']['sub']}</p>
    <table class="table"><thead><tr><th>{f['shortcuts']['th'][0]}</th><th>{f['shortcuts']['th'][1]}</th></tr></thead><tbody>{keys}</tbody></table>
  </section>

  {final(t, lang)}
</main>
'''


def ai_page(t, lang):
    a = t['ai']
    steps = ''.join(f'<div class="step"><span class="letter">{i + 1}</span><h3>{h}</h3><p>{p}</p></div>' for i, (h, p) in enumerate(a['how']['steps']))
    can = ''.join(f'<article class="card"><h3>{h}</h3><p>{p}</p></article>' for h, p in a['can']['cards'])
    th = ''.join(f'<th>{x}</th>' for x in a['providers']['th'])
    rows = ''.join('<tr>' + ''.join(f'<td>{c}</td>' for c in row) + '</tr>' for row in a['providers']['rows'])
    prompts = ''.join(f'<blockquote>{p}</blockquote>' for p in a['prompts']['items'])
    setup = ''.join(f'<li>{s}</li>' for s in a['setup']['steps'])
    return f'''<main id="top">
  {page_hero(a['hero'])}

  <section class="section">
    {img(lang, '02-ki-assistent.png', a['hero']['alt'], cls='window')}
  </section>

  <section class="section">
    {label('01', a['how']['label'])}
    <h2>{a['how']['h2']}</h2>
    <div class="cols3 lettered">{steps}</div>
  </section>

  <section class="section">
    {label('02', a['can']['label'])}
    <h2>{a['can']['h2']}</h2>
    <div class="grid3">{can}</div>
  </section>

  <section class="section">
    {label('03', a['providers']['label'])}
    <h2>{a['providers']['h2']}</h2>
    <p class="sub">{a['providers']['sub']}</p>
    <div class="table-wrap"><table class="table"><thead><tr>{th}</tr></thead><tbody>{rows}</tbody></table></div>
  </section>

  <section class="section split">
    <div class="split-text">
      {label('04', a['safety']['label'])}
      <h2>{a['safety']['h2']}</h2>
      {ticks(a['safety']['ticks'])}
    </div>
    <div class="prompts">
      <h3>{a['prompts']['h3']}</h3>
      {prompts}
    </div>
  </section>

  <section class="section narrow">
    {label('05', a['setup']['label'])}
    <h2>{a['setup']['h2']}</h2>
    <ol class="steps">{setup}</ol>
    <p class="sub small">{a['setup']['note']}</p>
  </section>

  {final(t, lang)}
</main>
'''


def download_page(t, lang):
    d = t['download']
    req = ''.join(f'<tr><td>{a}</td><td>{b}</td><td>{c}</td></tr>' for a, b, c in d['req']['rows'])
    win = ''.join(f'<li>{s}</li>' for s in d['win']['steps'])
    lin = ''.join(f'<li>{s}</li>' for s in d['lin']['steps'])
    debs = '\n'.join(deb_file(x) for x in DEBS)
    silent = ''.join(f'<tr><td>{label_}</td><td><code>{cmd.replace("{v}", VERSION)}</code></td></tr>' for label_, cmd in d['silent']['rows'])
    notes = ''.join(f'<li>{n}</li>' for n in d['notes']['items'])
    return f'''<main id="top">
  {page_hero(d['hero'])}

  <section class="section">
    {downloads_cards(t, lang)}
  </section>

  <section class="section split">
    <div class="split-text">
      {label('01', 'Windows')}
      <h2>{d['win']['h2']}</h2>
      <ol class="steps">{win}</ol>
      <p class="sub small">{d['win']['note']}</p>
    </div>
    <div class="split-text">
      {label('02', 'Linux')}
      <h2>{d['lin']['h2']}</h2>
      <ol class="steps">{lin}</ol>
      <pre><code>{debs}</code></pre>
    </div>
  </section>

  <section class="section narrow">
    {label('03', d['req']['label'])}
    <h2>{d['req']['h2']}</h2>
    <table class="table"><thead><tr>{''.join(f'<th>{x}</th>' for x in d['req']['th'])}</tr></thead><tbody>{req}</tbody></table>
  </section>

  <section class="section narrow">
    {label('04', d['silent']['label'])}
    <h2>{d['silent']['h2']}</h2>
    <p class="sub">{d['silent']['sub']}</p>
    <div class="table-wrap"><table class="table"><tbody>{silent}</tbody></table></div>
  </section>

  <section class="section narrow">
    {label('05', d['notes']['label'])}
    <h2>{d['notes']['h2']}</h2>
    <ul class="dots">{notes}</ul>
  </section>

  <section class="section narrow">
    {label('06', d['source']['label'])}
    <h2>{d['source']['h2']}</h2>
    <p class="sub">{d['source']['sub']}</p>
    <pre><code>git clone {GITHUB}.git
cd tangenta
make test &amp;&amp; make run</code></pre>
    <p><a class="more" href="{GITHUB}/blob/main/BUILD.md">{d['source']['more']} →</a></p>
  </section>

  {final(t, lang)}
</main>
'''


# ---------------------------------------------------------------- Markdown, llms.txt, Sitemap

class Markdown(HTMLParser):
    """Wandelt den Inhalt einer Seite (<main>) in Markdown für Sprachmodelle (llms.txt).

    Bewusst klein: nur die Elemente, die build.py selbst erzeugt. Links und Bilder werden absolut.
    """
    BLOCKS = {'p', 'figure', 'section', 'article', 'div', 'details', 'ul', 'ol', 'table', 'blockquote'}

    def __init__(self, base):
        super().__init__(convert_charrefs=True)
        self.base = base
        self.out = []
        self.skip = 0           # in aria-hidden (Laufband) oder Fensterleiste
        self.pre = False
        self.lists = []         # ['ul'|'ol', Zähler]
        self.link = None        # [href, text]
        self.row = None
        self.rows = 0
        self.quote = False
        self.lists_of_chips = 0  # in Kürzel-/Chip-Leisten: Einträge mit · trennen

    def write(self, s):
        if self.link is not None:
            self.link[1] += s
        elif self.row is not None and not self.pre:
            self.row[-1] += s
        else:
            self.out.append(s)

    def block(self):
        self.out.append('\n\n')

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if self.skip or a.get('aria-hidden') == 'true' or a.get('class') == 'window-bar':
            if tag not in ('img', 'br', 'i'):
                self.skip += 1
            return
        if tag in ('h1', 'h2', 'h3', 'h4'):
            self.block()
            self.out.append('#' * int(tag[1]) + ' ')
        elif tag in ('ul', 'ol'):
            self.lists.append([tag, 0])
            self.block()
        elif tag == 'li':
            kind = self.lists[-1] if self.lists else ['ul', 0]
            kind[1] += 1
            self.out.append('\n' + ('  ' * (len(self.lists) - 1)) + (f'{kind[1]}. ' if kind[0] == 'ol' else '- '))
        elif tag == 'pre':
            self.block()
            self.out.append('```\n')
            self.pre = True
        elif tag == 'code' and not self.pre:
            self.write('`')
        elif tag in ('b', 'strong'):
            self.write('**')
        elif tag == 'summary':
            self.block()
            self.out.append('**')
        elif tag == 'blockquote':
            self.block()
            self.out.append('> ')
        elif tag == 'br' or (tag == 'span' and not self.lists_of_chips):
            self.write(' ')
        elif tag == 'a':
            self.link = [urljoin(self.base, a.get('href', '')), '']
        elif tag == 'img':
            if a.get('alt'):
                self.block()
                self.out.append(f'![{a["alt"]}]({urljoin(self.base, a.get("src", ""))})')
                self.block()
        elif tag == 'div' and any(c in a.get('class', '').split() for c in ('keys', 'chips')):
            self.lists_of_chips += 1
            self.block()
        elif tag == 'tr':
            self.row = []
        elif tag in ('td', 'th') and self.row is not None:
            self.row.append('')
        elif tag in self.BLOCKS:
            self.block()

    def handle_endtag(self, tag):
        if self.skip:
            if tag not in ('img', 'br', 'i'):
                self.skip -= 1
            return
        if tag in ('h1', 'h2', 'h3', 'h4'):
            self.block()
        elif tag in ('ul', 'ol'):
            if self.lists:
                self.lists.pop()
            self.block()
        elif tag == 'pre':
            self.out.append('\n```')
            self.block()
            self.pre = False
        elif tag == 'code' and not self.pre:
            self.write('`')
        elif tag in ('b', 'strong'):
            self.write('**')
        elif tag == 'summary':
            self.out.append('**')
            self.block()
        elif tag == 'a' and self.link is not None:
            href, text = self.link
            self.link = None
            text = ' '.join(text.split())
            if text:
                self.write(f'[{text}]({href})')
        elif tag == 'tr' and self.row is not None:
            cells = [' '.join(c.split()).replace('|', '\\|') for c in self.row]
            self.row = None
            self.out.append('| ' + ' | '.join(cells) + ' |\n')
            if self.rows == 0:
                self.out.append('|' + '---|' * len(cells) + '\n')
            self.rows += 1
        elif tag == 'table':
            self.rows = 0
            self.block()
        elif tag == 'span':
            self.write(' · ' if self.lists_of_chips else ' ')
        elif tag == 'div' and self.lists_of_chips:
            self.lists_of_chips -= 1
            self.block()
        elif tag in self.BLOCKS:
            self.block()

    def handle_data(self, data):
        if self.skip:
            return
        if self.pre:
            self.out.append(data)
        else:
            self.write(re.sub(r'\s+', ' ', data))

    def text(self):
        s = ''.join(self.out)
        s = re.sub(r'[ \t·]+\n', '\n', s)
        s = re.sub(r'\n[ \t]+(?=[^\s-])', '\n', s)
        s = re.sub(r'\n{3,}', '\n\n', s)
        return s.strip() + '\n'


def markdown(lang, page, title, description, body):
    m = Markdown(page_url(lang, page))
    m.feed(body)
    return f'# {title}\n\n> {description}\n\nURL: {page_url(lang, page)}\n\n' + m.text()


def llms_txt(texts):
    """llms.txt (llmstxt.org): Überblick und Links auf die Markdown-Fassungen aller Seiten."""
    de = texts['de']
    lines = ['# TANGENTA', '', f'> {de["meta"]["index"][1]}', '',
             f'TANGENTA ist freie Software (GPLv3) von Intersoft Networks, Version {VERSION}, '
             'für Windows 10/11, Debian 12/13 und Ubuntu 24.04/26.04. Quellcode und Downloads: '
             f'{GITHUB}. Die Seiten gibt es auf Deutsch, Englisch, Französisch und Italienisch; '
             'jede Seite liegt zusätzlich als Markdown unter ihrer Adresse mit angehängtem .md.', '']
    for l in LANGS:
        t = texts[l]
        lines += [f'## {t["lang_name"]}', '']
        lines += [f'- [{t["meta"][p][0]}]({md_url(l, p)}): {t["meta"][p][1]}' for p in PAGES]
        lines.append('')
    lines += ['## Optional', '',
              f'- [Quellcode auf GitHub]({GITHUB}): Programm, Tests und Bauanleitung',
              f'- [Bauanleitung]({GITHUB}/blob/main/BUILD.md): eigene Build-Umgebung für Windows und Linux',
              f'- [Neueste Version]({RELEASES}): Setup, MSI, .deb-Pakete und Flatpak', '']
    return '\n'.join(lines)


def sitemap():
    entries = []
    for l in LANGS:
        for p in PAGES:
            alts = ''.join(f'\n    <xhtml:link rel="alternate" hreflang="{a}" href="{page_url(a, p)}"/>' for a in LANGS)
            alts += f'\n    <xhtml:link rel="alternate" hreflang="x-default" href="{page_url("de", p)}"/>'
            prio = '1.0' if p == 'index' else '0.8'
            entries.append(f'  <url>\n    <loc>{page_url(l, p)}</loc>{alts}\n    <changefreq>monthly</changefreq>\n'
                           f'    <priority>{prio}</priority>\n  </url>')
    return ('<?xml version="1.0" encoding="UTF-8"?>\n'
            '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9" xmlns:xhtml="http://www.w3.org/1999/xhtml">\n'
            + '\n'.join(entries) + '\n</urlset>\n')


def robots():
    return f'User-agent: *\nAllow: /\nDisallow: /src/\n\nSitemap: {DOMAIN}/sitemap.xml\n'


LANG_NAMES = {}
LOCALES = {}


def write(path, text):
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)


def main():
    texts = {l: load(l) for l in LANGS}
    for l in LANGS:
        LANG_NAMES[l] = texts[l]['lang_name']
        LOCALES[l] = texts[l]['locale']
    for l in LANGS:
        t = texts[l]
        out = SITE if l == 'de' else os.path.join(SITE, l)
        os.makedirs(out, exist_ok=True)
        bodies = {'index': home, 'features': features, 'ai': ai_page, 'download': download_page}
        for page in PAGES:
            title, description = t['meta'][page]
            body = bodies[page](t, l)
            write(os.path.join(out, page + '.html'), head(t, l, page, title, description) + nav(t, l, page) + '\n' + body + '\n' + footer(t, l))
            write(os.path.join(out, page + '.html.md'), markdown(l, page, title, description, body))
    write(os.path.join(SITE, 'llms.txt'), llms_txt(texts))
    write(os.path.join(SITE, 'sitemap.xml'), sitemap())
    write(os.path.join(SITE, 'robots.txt'), robots())
    print('Seiten:', len(LANGS) * len(PAGES), '+ Markdown, llms.txt, sitemap.xml, robots.txt')


if __name__ == '__main__':
    main()
