// Boot-check for the FunKey control-mapping prototype (ticket #9).
// Loads the served page, presses Enter to join as keyboard player, checks:
// - no page errors
// - window.__funkey exists (mapping layer live), scheme from URL param works
// - overlay appears once polling starts
// - synthesized coordinates for a few virtual-button chords are correct
const { chromium } = require("playwright");

const BASE = process.env.BASE || "http://localhost:8765/dist/meleelight.html";

(async () => {
  const browser = await chromium.launch({ channel: "chrome", headless: true });
  const page = await browser.newPage();
  const errors = [];
  page.on("pageerror", (e) => errors.push("pageerror: " + e.message));
  page.on("console", (m) => { if (m.type() === "error") errors.push("console.error: " + m.text()); });

  const url = process.argv[2] || BASE;
  await page.goto(url, { waitUntil: "load" });
  await page.waitForTimeout(3000); // let bundles load + start screen init

  const boot = await page.evaluate(() => ({
    funkey: typeof window.__funkey === "object",
    scheme: window.__funkey && window.__funkey.getScheme(),
    canvases: document.querySelectorAll("canvas").length
  }));
  console.log("boot:", JSON.stringify(boot));

  // join as keyboard player (hold Enter at start screen) then let polling run
  await page.keyboard.down("Enter");
  await page.waitForTimeout(700);
  await page.keyboard.up("Enter");
  let afterJoin = { overlay: false, lastInput: false };
  for (let t = 0; t < 20 && !afterJoin.lastInput; t++) {
    await page.waitForTimeout(500);
    afterJoin = await page.evaluate(() => ({
      overlay: !!document.getElementById("funkeyOverlay"),
      lastInput: window.__funkey.state.input ? true : false
    }));
  }
  console.log("afterJoin:", JSON.stringify(afterJoin));
  if (!afterJoin.lastInput) {
    console.log("FAIL: mapping layer never polled (keyboard player did not join)");
    await browser.close();
    process.exit(1);
  }

  // ---- chord table checks (pure resolver, via key events) ----
  async function sample(keys) {
    for (const k of keys) await page.keyboard.down(k);
    await page.waitForTimeout(120);
    const r = await page.evaluate(() => {
      const i = window.__funkey.state.input;
      return { lsX: i.lsX, lsY: i.lsY, csX: i.csX, csY: i.csY,
               a: i.a, x: i.x, r: i.r, rA: i.rA, z: i.z, lA: i.lA,
               family: window.__funkey.state.family };
    });
    for (const k of keys.slice().reverse()) await page.keyboard.up(k);
    await page.waitForTimeout(80);
    return r;
  }
  function expect(name, got, want) {
    const keys = Object.keys(want);
    const bad = keys.filter((k) => got[k] !== want[k]);
    if (bad.length) {
      console.log("FAIL " + name + ": " + bad.map((k) => k + "=" + got[k] + " (want " + want[k] + ")").join(", ") + "  [" + got.family + "]");
      process.exitCode = 1;
    } else {
      console.log("ok   " + name + "  [" + got.family + "]");
    }
  }

  // S1 checks (default scheme)
  expect("plain right (dash)", await sample(["ArrowRight"]), { lsX: 1, lsY: 0 });
  expect("mod+right (walk 0.6625)", await sample(["KeyQ", "ArrowRight"]), { lsX: 0.6625, lsY: 0 });
  expect("mod+up (u-tilt 0.5375)", await sample(["KeyQ", "ArrowUp"]), { lsX: 0, lsY: 0.5375 });
  expect("diag up-right (0.7,0.7)", await sample(["ArrowUp", "ArrowRight"]), { lsX: 0.7, lsY: 0.7 });
  expect("shield+down-right (shield drop)", await sample(["KeyE", "ArrowDown", "ArrowRight"]), { lsX: 0.7, lsY: -0.6875, r: true, rA: 1 });
  expect("mod+shield+down-left (wavedash 30)", await sample(["KeyQ", "KeyE", "ArrowDown", "ArrowLeft"]), { lsX: -0.6375, lsY: -0.375, r: true });
  expect("SOCD L+R neutral", await sample(["ArrowLeft", "ArrowRight"]), { lsX: 0, lsY: 0 });
  expect("C-layer right (csX 1.0, ls neutral)", await sample(["KeyA", "ArrowRight"]), { lsX: 0, csX: 1, csY: 0 });
  expect("attack btn", await sample(["KeyD"]), { a: true });
  expect("jump btn", await sample(["KeyW"]), { x: true });

  // scheme switching via API
  await page.evaluate(() => window.__funkey.setScheme("s2"));
  expect("S2 modY+right (0.3375)", await sample(["KeyE", "ArrowRight"]), { lsX: 0.3375, lsY: 0 });
  expect("S2 modY+shield+up-right (0.4750,0.8750)", await sample(["KeyE", "KeyA", "ArrowUp", "ArrowRight"]), { lsX: 0.475, lsY: 0.875, r: true });
  expect("S2 L+R clayer", await sample(["KeyQ", "KeyE", "ArrowUp"]), { csY: 1, lsY: 0 });
  // S3 grab cannot be observed live in a MENU: upstream treats held keyboard Z
  // as frame-advance while !playing and clears tempBuffer[0].z in place
  // (src/main/main.js:834) — the polled object is mutated after the fact.
  // In an actual match (playing) grab works; here we check the resolver directly.
  const s3 = await page.evaluate(() => window.__funkey.resolve({ Y: true }, "s3"));
  if (s3.grab === true) { console.log("ok   S3 grab (resolver)"); }
  else { console.log("FAIL S3 grab (resolver): " + JSON.stringify(s3)); process.exitCode = 1; }

  console.log(errors.length ? "PAGE ERRORS:\n" + errors.join("\n") : "no page errors");
  if (errors.length) process.exitCode = 1;
  await browser.close();
})().catch((e) => { console.error(e); process.exit(1); });
