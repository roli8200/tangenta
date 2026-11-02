# tangenta.ch

Statische Seite (HTML + CSS, ohne Fremdskripte und ohne externe Schriften), Vorbild getartcraft.com.

- `index.html`, `styles.css`, Bilder in `img/`
- Bildschirmfotos kommen aus dem Schaufenster-Test:
  `VEK_SHOWCASE_DIR=docs/screenshots VEK_AI_LIVE=1 VEK_NETWORK_TESTS=1 tst_tools showcase`,
  danach `docs/screenshots/0[1-4]-*.png` nach `website/img/` kopieren.
- Veröffentlichen: den Ordner unverändert auf den Webserver legen (z.B. `rsync -a website/ server:/var/www/tangenta.ch/`).
