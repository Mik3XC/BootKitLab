# Deploying lab.init.wtf (GitHub Pages)

The site is a static HTML site rooted at `index.html`. Repo: `michaelcohee/BootKitLab` (branch `main`).

## Files added for deploy
- `CNAME` — custom domain (`lab.init.wtf`). GitHub Pages reads this.
- `.nojekyll` — skip Jekyll so `_`-prefixed files serve and builds are instant.
- `robots.txt` + `sitemap.xml` — crawl/indexing.
- `404.html` — branded not-found page (GitHub Pages serves this automatically).

## One-time setup

1. **Push the repo** (from `~/bootkit`):
   ```sh
   git add -A
   git commit -m "Add GitHub Pages deploy config + Oct-15 Vigil"
   git push origin main
   ```
2. **Enable Pages**: GitHub → repo **Settings → Pages** → Source = **Deploy from a branch** → Branch = **main** / **/(root)** → Save.
3. **Custom domain**: same Pages screen → Custom domain = `lab.init.wtf` → Save. (The `CNAME` file already sets this; the UI just confirms it.)
4. **DNS** at your `init.wtf` provider — add one record:
   ```
   Type: CNAME   Name: lab   Value: michaelcohee.github.io
   ```
5. Wait for DNS + Pages TLS (minutes to ~an hour). Then **Enforce HTTPS** in the Pages settings once the cert is issued.

Live at: **https://lab.init.wtf/** → homepage, with `/signals.html` for the Vigil feed.

## Cleanup (optional — do in Finder/terminal, your call)
- `index-1.html` (~539 KB) looks like a stray duplicate — not linked by the site. Safe to delete.
- `_main.html` is the claude.ai-artifact build variant; harmless but unused on the live site. Safe to delete.
- Add `.DS_Store` to `.gitignore` if it isn't already.
- The `10biForthOS/` and `ChainLoader/` project folders will also be served as raw files. Fine to leave; move them out of the web root if you want a leaner deploy.

## Updating later
Edit the HTML, `git commit`, `git push` — Pages redeploys automatically. The claude.ai artifact and this repo are separate copies; I keep both in sync when I update the Vigil feed.
