// uso: node svg2png.js entrada.svg saida.png [largura]
const { chromium } = require('playwright');
const fs = require('fs');
(async () => {
  const [inp, out, w] = process.argv.slice(2);
  const svg = fs.readFileSync(inp, 'utf8');
  const b = await chromium.launch();
  const p = await b.newPage({ viewport: { width: parseInt(w || '900'), height: 700 } });
  await p.setContent(`<html><body style="margin:0;background:#fff">${svg.replace(/<\?xml[^>]*>/, '')}</body></html>`);
  await p.evaluate((w) => { const s = document.querySelector('svg'); s.setAttribute('width', w); s.removeAttribute('height'); }, w || '900');
  const el = await p.$('svg');
  await el.screenshot({ path: out });
  await b.close();
})();
