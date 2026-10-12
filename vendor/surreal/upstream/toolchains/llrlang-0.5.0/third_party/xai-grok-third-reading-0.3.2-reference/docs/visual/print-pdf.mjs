#!/usr/bin/env node
import { chromium } from "playwright";
import { pathToFileURL } from "node:url";
import { resolve } from "node:path";

const html = resolve(import.meta.dirname ?? ".", "third-reading.html");
const pdf = resolve(import.meta.dirname ?? ".", "third-reading.pdf");
const browser = await chromium.launch();
const page = await browser.newPage();
await page.goto(pathToFileURL(html).href, { waitUntil: "networkidle" });
await page.pdf({
  path: pdf,
  format: "A4",
  printBackground: true,
  displayHeaderFooter: true,
  headerTemplate: `<div></div>`,
  footerTemplate: `<div style="font-family:monospace;font-size:8px;color:#6f6860;width:100%;padding:0 16mm;display:flex;justify-content:space-between;"><span>Third Reading · LLRLANG 0.3.2</span><span class="pageNumber"></span></div>`,
  margin: { top: "16mm", bottom: "18mm", left: "16mm", right: "16mm" },
});
await browser.close();
console.log("wrote", pdf);
