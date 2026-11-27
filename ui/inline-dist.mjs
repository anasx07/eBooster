import fs from 'fs';
import path from 'path';

const distDir = path.resolve('dist');
const indexPath = path.join(distDir, 'index.html');
let html = fs.readFileSync(indexPath, 'utf8');

// Inline CSS
html = html.replace(/<link\s+rel="stylesheet"[^>]*href="([^"]+)"[^>]*>/g, (match, href) => {
  const cssPath = path.join(distDir, href.replace(/^\.\//, ''));
  if (fs.existsSync(cssPath)) {
    const css = fs.readFileSync(cssPath, 'utf8');
    return `<style>${css}</style>`;
  }
  return match;
});

// Inline JS
html = html.replace(/<script\s+type="module"[^>]*src="([^"]+)"[^>]*><\/script>/g, (match, src) => {
  const jsPath = path.join(distDir, src.replace(/^\.\//, ''));
  if (fs.existsSync(jsPath)) {
    const js = fs.readFileSync(jsPath, 'utf8');
    return `<script type="module">${js}</script>`;
  }
  return match;
});

fs.writeFileSync(indexPath, html, 'utf8');
console.log('Successfully inlined JS and CSS into dist/index.html');
