/**
 * Dynamic Japanese -> romaji conversion.
 *
 * Every romaji string shown or accepted by Kotoba Lock is produced here by
 * Kuroshiro + the Kuromoji analyzer. No dictionary entry stores romaji and the
 * application contains no per-word romaji table.
 *
 * Kuroshiro's Hepburn output uses macrons for long vowels (gakkō). Those are
 * turned into plain vowels for display, while the answer matcher also accepts
 * the usual wapuro spellings (gakkou / gakkoo), generated algorithmically.
 */

const Kuroshiro = require("kuroshiro");
const KuromojiAnalyzer = require("kuroshiro-analyzer-kuromoji");

const ROMAJI_SYSTEM = "hepburn";

// Any kana, kanji or CJK punctuation means "run the analyzer".
const JAPANESE_RE =
  /[\u3000-\u303f\u3040-\u309f\u30a0-\u30ff\u3400-\u4dbf\u4e00-\u9fff\uf900-\ufaff]/;

// Macrons are decomposed to base letter + combining mark before stripping.
const MACRON_EXPANSIONS = {
  ā: ["aa", "a"],
  ī: ["ii", "i"],
  ū: ["uu", "u"],
  ē: ["ee", "e"],
  ō: ["ou", "oo", "o"],
};

let engine = null;
let initPromise = null;
const cache = new Map();

function ensureEngine() {
  if (!initPromise) {
    initPromise = (async () => {
      const kuroshiro = new Kuroshiro();
      await kuroshiro.init(new KuromojiAnalyzer());
      engine = kuroshiro;
      return kuroshiro;
    })().catch((error) => {
      // Allow a later call to retry after a transient failure.
      initPromise = null;
      throw error;
    });
  }
  return initPromise;
}

/**
 * Initialise the analyzer early so the first word gate opens instantly.
 */
function init() {
  return ensureEngine();
}

async function convert(text, mode = "normal") {
  const value = String(text || "").trim();
  if (!value) return "";

  const key = `${mode}\u0000${value}`;
  if (cache.has(key)) return cache.get(key);

  const kuroshiro = engine || (await ensureEngine());
  let output = "";
  try {
    output = await kuroshiro.convert(value, {
      to: "romaji",
      mode,
      romajiSystem: ROMAJI_SYSTEM,
    });
  } catch (error) {
    output = "";
  }

  output = String(output || "").trim();
  cache.set(key, output);
  return output;
}

function stripMarks(text) {
  return String(text || "")
    .normalize("NFD")
    .replace(/[\u0300-\u036f]/g, "");
}

function cleanDisplay(text) {
  return stripMarks(text)
    .replace(/\s+/g, " ")
    .replace(/\s+([,.;:!?、。！？])/g, "$1")
    .trim();
}

/**
 * Display form for a single word or phrase: plain vowels, single spaces.
 */
async function displayRomaji(text) {
  if (!JAPANESE_RE.test(String(text || ""))) return "";
  return cleanDisplay(await convert(text, "normal"));
}

/**
 * Display form for a sentence: Kuroshiro's spaced mode keeps word boundaries.
 */
async function sentenceRomaji(text) {
  if (!JAPANESE_RE.test(String(text || ""))) return "";
  return cleanDisplay(await convert(text, "spaced"));
}

/**
 * All accepted spellings for one converted string. Expands each macron into
 * its common wapuro forms, e.g. gakkō -> gakkou, gakkoo, gakko, gakkō.
 */
function expandVariants(raw) {
  const value = String(raw || "").trim();
  if (!value) return [];

  const results = new Set([value, stripMarks(value)]);
  let variants = [""];

  for (const char of value) {
    const expansions = MACRON_EXPANSIONS[char.toLowerCase()];
    if (!expansions) {
      variants = variants.map((prefix) => prefix + char);
      continue;
    }

    const next = [];
    for (const prefix of variants) {
      for (const expansion of expansions) {
        next.push(prefix + expansion);
      }
    }
    // Keep the combinatorics bounded for long sentences.
    variants = next.slice(0, 32);
  }

  for (const variant of variants) results.add(variant);
  return [...results];
}

/**
 * Accepted answers for a word/reading pair. The renderer normalises input
 * (lowercase, spaces and punctuation removed) before comparing.
 */
async function answerVariants(...texts) {
  const answers = new Set();

  for (const text of texts) {
    if (!JAPANESE_RE.test(String(text || ""))) continue;
    const raw = await convert(text, "normal");
    for (const variant of expandVariants(raw)) {
      const normalised = stripMarks(variant)
        .replace(/\s+/g, "")
        .toLowerCase();
      if (normalised) answers.add(normalised);
    }
  }

  return [...answers];
}

function isJapanese(text) {
  return JAPANESE_RE.test(String(text || ""));
}

module.exports = {
  ROMAJI_SYSTEM,
  init,
  convert,
  stripMarks,
  cleanDisplay,
  displayRomaji,
  sentenceRomaji,
  expandVariants,
  answerVariants,
  isJapanese,
};
