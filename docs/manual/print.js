const { chromium } = require('playwright');
const path = require('path');
(async () => {
  const dir = __dirname;
  const browser = await chromium.launch();
  const page = await browser.newPage();
  await page.goto('file://' + path.join(dir, 'Mist-Manual.html'), { waitUntil: 'load' });
  await page.pdf({
    path: path.join(dir, '..', 'Mist-Manual.pdf'), format: 'A4', printBackground: true, preferCSSPageSize: true,
    displayHeaderFooter: true, headerTemplate: '<span></span>',
    footerTemplate: '<div style="width:100%;font-size:8px;color:#8e8e93;text-align:center;font-family:Inter,sans-serif">Mist · <span class="pageNumber"></span> / <span class="totalPages"></span></div>',
  });
  await browser.close();
})();
