# tangenta.ch

Statische Seite (HTML + CSS, ohne Fremdskripte und ohne externe Schriften), Vorbild getartcraft.com.

- Seiten entstehen aus `src/build.py` (Vorlage) und `src/i18n/<sprache>.py` (Texte): `python website/src/build.py`.
  Deutsch liegt im Wurzelordner, Englisch, Französisch und Italienisch in `en/`, `fr/`, `it/`.
  Die HTML-Dateien nie direkt bearbeiten, sie werden beim nächsten Bau überschrieben.
- `styles.css` und Bilder in `img/` gelten für alle Sprachen
- Bildschirmfotos kommen aus dem Schaufenster-Test:
  `VEK_SHOWCASE_DIR=docs/screenshots VEK_AI_LIVE=1 VEK_NETWORK_TESTS=1 tst_tools showcase`,
  danach `docs/screenshots/0[1-8]-*.png` (ohne 05) nach `website/img/` kopieren.
- Veröffentlichen: den Ordner auf den Webserver legen, ohne `src/` und diese Datei (z.B. `rsync -a --exclude src --exclude README.md website/ server:/var/www/tangenta.ch/`).
