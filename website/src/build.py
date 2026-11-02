#!/usr/bin/env python3
"""Erzeugt die statische Webseite tangenta.ch in vier Sprachen.

    python website/src/build.py

Vorlage hier, Texte je Sprache in i18n/<sprache>.py. Deutsch liegt im Wurzelordner, die anderen
Sprachen in en/, fr/ und it/. Alle Sprachen haben dieselben Seiten mit denselben Dateinamen, damit
der Sprachwechsel immer auf die gleiche Seite führt.
"""
import html
import importlib.util
import os

HERE = os.path.dirname(os.path.abspath(__file__))
SITE = os.path.dirname(HERE)
LANGS = ['de', 'en', 'fr', 'it']
PAGES = ['index', 'features', 'ai', 'download']
DOMAIN = 'https://tangenta.ch'
GITHUB = 'https://github.com/roli8200/tangenta'
RELEASES = GITHUB + '/releases/latest'
VERSION = '0.85'
DEBS = ['debian12', 'debian13', 'ubuntu24.04', 'ubuntu26.04']


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


def head(t, lang, page, title, description):
    alts = '\n'.join(f'  <link rel="alternate" hreflang="{l}" href="{page_url(l, page)}">' for l in LANGS)
    p = prefix(lang)
    return f'''<!doctype html>
<html lang="{lang}">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{esc(title)}</title>
  <meta name="description" content="{esc(description)}">
  <link rel="canonical" href="{page_url(lang, page)}">
{alts}
  <link rel="alternate" hreflang="x-default" href="{page_url('de', page)}">
  <meta property="og:type" content="website">
  <meta property="og:title" content="{esc(title)}">
  <meta property="og:description" content="{esc(description)}">
  <meta property="og:image" content="{DOMAIN}/img/01-hauptfenster.png">
  <meta property="og:locale" content="{t['locale']}">
  <link rel="icon" href="{p}img/favicon.png">
  <link rel="stylesheet" href="{p}styles.css">
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


def downloads_cards(t, lang):
    d = t['download_cards']
    return f'''<div class="downloads">
      <article class="card">
        <h3>{d['win_h']}</h3>
        <p>{d['win_p']}</p>
        <a class="btn btn-primary" href="{RELEASES}">{d['win_btn']}</a>
        <pre><code>TANGENTA-{VERSION}-x64-setup.exe /quiet
msiexec /i TANGENTA-{VERSION}-x64.msi /qn</code></pre>
      </article>
      <article class="card">
        <h3>{d['lin_h']}</h3>
        <p>{d['lin_p']}</p>
        <a class="btn btn-primary" href="{RELEASES}">{d['lin_btn']}</a>
        <pre><code>sudo apt install ./tangenta_{VERSION}-1~debian12_amd64.deb</code></pre>
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
    debs = '\n'.join(f'tangenta_{VERSION}-1~{x}_amd64.deb' for x in DEBS)
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


LANG_NAMES = {}


def main():
    texts = {l: load(l) for l in LANGS}
    for l in LANGS:
        LANG_NAMES[l] = texts[l]['lang_name']
    for l in LANGS:
        t = texts[l]
        out = SITE if l == 'de' else os.path.join(SITE, l)
        os.makedirs(out, exist_ok=True)
        bodies = {'index': home, 'features': features, 'ai': ai_page, 'download': download_page}
        for page in PAGES:
            meta = t['meta'][page]
            doc = head(t, l, page, meta[0], meta[1]) + nav(t, l, page) + '\n' + bodies[page](t, l) + '\n' + footer(t, l)
            with open(os.path.join(out, page + '.html'), 'w', encoding='utf-8', newline='\n') as f:
                f.write(doc)
    print('Seiten:', len(LANGS) * len(PAGES))


if __name__ == '__main__':
    main()
