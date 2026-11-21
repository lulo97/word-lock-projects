const fs = require("fs");
const path = require("path");

const config = require("./config");
const romaji = require("./romaji");

const AUDIO_EXTENSIONS = [
  ".ogg",
  ".oga",
  ".opus",
  ".mp3",
  ".wav",
  ".m4a",
  ".aac",
  ".flac",
  ".webm",
];

const FALLBACK_ENTRY = {
  word: "データなし",
  language: { source: "ja", target: "vi" },
  reading: "",
  pronunciation: {
    kana: "",
    audio: "",
  },
  part_of_speech: "",
  definitions: [],
  examples: [],
  synonyms: [],
  antonyms: [],
  tags: [],
};

function wordKey(entry) {
  return String((entry && entry.word) || "").trim().toLowerCase();
}

function walk(directory, files) {
  let entries;
  try {
    entries = fs.readdirSync(directory, { withFileTypes: true });
  } catch (error) {
    return files;
  }
  for (const entry of entries) {
    const full = path.join(directory, entry.name);
    if (entry.isDirectory()) {
      walk(full, files);
    } else if (entry.isFile() && entry.name.toLowerCase().endsWith(".json")) {
      files.push(full);
    }
  }
  return files;
}

function iterWordFiles() {
  return walk(config.DATABASE_DIR, []).sort((a, b) => a.localeCompare(b));
}

function loadFile(file) {
  try {
    const entry = JSON.parse(fs.readFileSync(file, "utf8"));
    if (!entry || typeof entry !== "object" || Array.isArray(entry)) {
      return null;
    }
    // Keep the source path out of renderer payloads but available for audio
    // auto-discovery. Non-enumerable keys survive no spread.
    Object.defineProperty(entry, "__file", {
      value: file,
      enumerable: false,
      configurable: true,
    });
    return entry;
  } catch (error) {
    return null;
  }
}

function loadEntries(ignoreWords) {
  const ignored = new Set(
    Array.from(ignoreWords || [], (word) => String(word).trim().toLowerCase()),
  );
  const entries = [];
  for (const file of iterWordFiles()) {
    const entry = loadFile(file);
    if (entry && !ignored.has(wordKey(entry))) {
      entries.push(entry);
    }
  }
  return entries;
}

function loadNextEntry(attempts, ignoreWords) {
  const counts = attempts || {};
  const entries = loadEntries(ignoreWords);
  if (!entries.length) return { ...FALLBACK_ENTRY };

  let lowest = Infinity;
  for (const entry of entries) {
    const count = counts[wordKey(entry)] || 0;
    if (count < lowest) lowest = count;
  }

  const candidates = entries.filter(
    (entry) => (counts[wordKey(entry)] || 0) === lowest,
  );
  return candidates[Math.floor(Math.random() * candidates.length)];
}

function loadRandomEntry() {
  const entries = loadEntries();
  if (!entries.length) return { ...FALLBACK_ENTRY };
  return entries[Math.floor(Math.random() * entries.length)];
}

function isFile(candidate) {
  try {
    return fs.statSync(candidate).isFile();
  } catch (error) {
    return false;
  }
}

/**
 * Resolve an explicit audio path, then fall back to `<base><extension>` for
 * every supported format. The fallback lets users drop files into the standard
 * audios/ folders without editing each JSON entry.
 */
function resolveAudioValue(value, bases) {
  const raw = String(value || "").trim();
  if (raw) {
    const candidate = path.isAbsolute(raw)
      ? raw
      : path.join(config.DATABASE_DIR, raw);
    if (isFile(candidate)) return candidate;
  }

  for (const base of bases || []) {
    if (!base) continue;
    for (const extension of AUDIO_EXTENSIONS) {
      const candidate = `${base}${extension}`;
      if (isFile(candidate)) return candidate;
    }
  }

  return null;
}

function entryFolder(entry) {
  if (entry && entry.__file) {
    return path.dirname(entry.__file);
  }
  return config.DATABASE_DIR;
}

function audioStems(entry, extra) {
  const stems = [];
  for (const value of extra || []) {
    if (value) stems.push(value);
  }
  if (entry && entry.__file) {
    stems.push(path.basename(entry.__file, path.extname(entry.__file)));
  }
  const word = String((entry && entry.word) || "").trim().toLowerCase();
  if (word) stems.push(word);

  const seen = new Set();
  return stems.filter((stem) => {
    const key = String(stem);
    if (!key || seen.has(key)) return false;
    seen.add(key);
    return true;
  });
}

function resolveAudio(entry, extraStems) {
  const pronunciation = entry && entry.pronunciation;
  if (!pronunciation || typeof pronunciation !== "object") return null;

  const folder = entryFolder(entry);
  const bases = audioStems(entry, extraStems).map((stem) =>
    path.join(folder, "audios", stem),
  );
  return resolveAudioValue(pronunciation.audio, bases);
}

function resolveExampleAudio(example, entry, index, extraStems) {
  if (!example || typeof example !== "object") return null;

  const folder = entryFolder(entry);
  const number = Number(index) + 1;
  const bases = [path.join(folder, "audios_example", `example_${number}`)];
  for (const stem of audioStems(entry, extraStems)) {
    bases.push(path.join(folder, "audios_example", `${stem}_${number}`));
  }
  return resolveAudioValue(example.audio, bases);
}

function readingOf(entry) {
  const pronunciation = entry && entry.pronunciation;
  return (
    (pronunciation && pronunciation.kana) || (entry && entry.reading) || ""
  );
}

async function definitionCopies(entry) {
  const definitions = Array.isArray(entry.definitions) ? entry.definitions : [];
  return Promise.all(
    definitions.map(async (definition) => {
      const copy =
        definition && typeof definition === "object" ? { ...definition } : {};
      copy.romaji = await romaji.sentenceRomaji(copy.japanese || "");
      return copy;
    }),
  );
}

async function exampleCopies(entry, extraStems) {
  const examples = Array.isArray(entry.examples) ? entry.examples : [];
  return Promise.all(
    examples.map(async (example, index) => {
      const copy = example && typeof example === "object" ? { ...example } : {};
      copy.romaji = await romaji.sentenceRomaji(copy.japanese || "");
      copy._audio = resolveExampleAudio(example, entry, index, extraStems);
      return copy;
    }),
  );
}

async function chipRomaji(values) {
  const chips = Array.isArray(values) ? values : [];
  return Promise.all(chips.map(async (value) => romaji.displayRomaji(value)));
}

/**
 * Return a renderer-safe copy of an entry.
 *
 * Romaji is never read from the entry: it is computed from the kana reading
 * (or the word itself) with Kuroshiro. Accepted answers and audio-file stems
 * are derived from that same conversion.
 */
async function presentEntry(entry) {
  const source = entry || FALLBACK_ENTRY;
  const word = String(source.word || "");
  const reading = String(readingOf(source) || "");
  const base = reading || word;

  const display = await romaji.displayRomaji(base);
  // The kana reading is authoritative; only fall back to the word when no
  // reading exists (e.g. an entry written in kana already).
  const answers = await romaji.answerVariants(base);

  // Audio files can be named after any accepted romaji spelling.
  const stems = [display, ...answers];
  const pronunciation =
    source.pronunciation && typeof source.pronunciation === "object"
      ? { ...source.pronunciation }
      : {};
  // Old entries may still carry romaji: strip it so the UI can only use the
  // dynamically computed value.
  delete pronunciation.romaji;
  delete pronunciation.romaji_alternatives;

  const [definitions, examples, synonymsRomaji, antonymsRomaji] =
    await Promise.all([
      definitionCopies(source),
      exampleCopies(source, stems),
      chipRomaji(source.synonyms),
      chipRomaji(source.antonyms),
    ]);

  return {
    ...source,
    word,
    language: source.language || { source: "ja", target: "vi" },
    pronunciation,
    definitions,
    examples,
    synonyms: Array.isArray(source.synonyms) ? source.synonyms : [],
    antonyms: Array.isArray(source.antonyms) ? source.antonyms : [],
    tags: Array.isArray(source.tags) ? source.tags : [],
    _reading: reading,
    _romaji: display,
    _answerRomaji: answers,
    _synonymsRomaji: synonymsRomaji,
    _antonymsRomaji: antonymsRomaji,
    _audio: resolveAudio(source, stems),
  };
}

module.exports = {
  FALLBACK_ENTRY,
  wordKey,
  iterWordFiles,
  loadEntries,
  loadNextEntry,
  loadRandomEntry,
  resolveAudio,
  resolveExampleAudio,
  presentEntry,
};
