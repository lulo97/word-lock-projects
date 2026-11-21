/**
 * Word gate + dictionary card, rewritten with jQuery.
 *
 * Romaji is computed by main.js with Kuroshiro; this file only renders it.
 */

const $ = window.jQuery;

const AUTO_ACCEPT_DELAY_MS = 500;
const SPEAK_ANIMATION_MS = 320;
const SPEAK_DEBOUNCE_MS = 350;
const ANSWER_IGNORE_MS = 500;

const CHIP_CLASS = { "": "chip", error: "chip error", muted: "chip muted" };

const state = {
  style: null,
  entry: null,
  mode: "quiz", // "quiz" | "done"
  finishTimer: null,
  focusSequence: 0,
  finishedAt: 0,
};

const speaking = {
  button: null,
  idleText: "Listen",
  prefix: "Speaking",
  dots: 1,
  timer: null,
  startedAt: 0,
};

let audioContext = null;
let playback = null;

// ------------------------------------------------------------------ helpers

function normalizeRomaji(value) {
  return String(value || "")
    .normalize("NFD")
    .replace(/[\u0300-\u036f]/g, "")
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9]/g, "");
}

function expectedAnswers() {
  const entry = state.entry || {};
  const values = entry._answerRomaji || [entry._romaji || ""];
  return new Set(values.map(normalizeRomaji).filter(Boolean));
}

function answersMatch(value) {
  const typed = normalizeRomaji(value);
  return Boolean(typed) && expectedAnswers().has(typed);
}

// -------------------------------------------------------------------- style

function applyStyle(style) {
  if (!style) return;
  state.style = style;
  window.WordLockTheme.applyStyle(style);

  stopSpeaking(true, true);
  cancelFinishTimer();
  cancelFocus();

  if (state.mode === "done" && state.entry) buildInfo(state.entry);
  else if (state.mode === "quiz") scheduleFocus();
}

// ------------------------------------------------------------------- audio

function ensureAudioContext() {
  if (!audioContext) {
    const Context = window.AudioContext || window.webkitAudioContext;
    if (!Context) return null;
    audioContext = new Context();
  }
  if (audioContext.state === "suspended") audioContext.resume().catch(() => {});
  return audioContext;
}

function stopPlayback() {
  const current = playback;
  playback = null;
  if (!current) return;
  try {
    current.source.onended = null;
    current.source.stop();
  } catch (error) {}
  try {
    current.source.disconnect();
    current.gain.disconnect();
  } catch (error) {}
}

function isPlaying() {
  return Boolean(playback && !playback.ended);
}

async function playAudio(audioPath) {
  if (!audioPath) return false;
  stopPlayback();

  try {
    const bytes = await window.wordlock.readAudio(audioPath);
    const context = ensureAudioContext();
    if (!context) return false;

    let arrayBuffer;
    if (bytes instanceof ArrayBuffer) {
      arrayBuffer = bytes;
    } else {
      const offset = bytes.byteOffset || 0;
      arrayBuffer = bytes.buffer.slice(
        offset,
        offset + (bytes.byteLength || bytes.length),
      );
    }

    const buffer = await context.decodeAudioData(arrayBuffer);
    const source = context.createBufferSource();
    source.buffer = buffer;

    const gain = context.createGain();
    const volume = await window.wordlock.getVolume();
    gain.gain.value = Number.isFinite(volume) ? volume : 2.0;

    source.connect(gain);
    gain.connect(context.destination);
    source.start();

    const token = { source, gain, ended: false };
    source.onended = () => {
      token.ended = true;
    };
    playback = token;
    return true;
  } catch (error) {
    playback = null;
    return false;
  }
}

function setSpeakingText() {
  const button = speaking.button;
  if (!button || !button.isConnected) {
    forgetSpeaking();
    return;
  }
  const dots = ".".repeat(Math.max(1, speaking.dots));
  button.textContent = (speaking.prefix || "") + dots;
}

function animateSpeaking() {
  speaking.timer = null;
  if (!speaking.button || !speaking.button.isConnected) {
    forgetSpeaking();
    return;
  }
  if (!isPlaying()) {
    stopSpeaking(true);
    return;
  }
  speaking.dots = (speaking.dots % 3) + 1;
  setSpeakingText();
  speaking.timer = setTimeout(animateSpeaking, SPEAK_ANIMATION_MS);
}

function forgetSpeaking() {
  speaking.button = null;
  speaking.dots = 1;
  speaking.prefix = "Speaking";
  speaking.idleText = "Listen";
  speaking.startedAt = 0;
}

function stopSpeaking(reset = true, stopAudio = false) {
  if (speaking.timer !== null) {
    clearTimeout(speaking.timer);
    speaking.timer = null;
  }

  const button = speaking.button;
  const idleText = speaking.idleText;
  forgetSpeaking();

  if (reset && button && button.isConnected) button.textContent = idleText;
  if (stopAudio) stopPlayback();
}

async function speak(audioPath, button, idleText = "Listen", prefix = "Speaking") {
  if (!audioPath || !button || button.disabled) return;

  const now = performance.now();
  if (button === speaking.button && now - speaking.startedAt < SPEAK_DEBOUNCE_MS) {
    return;
  }
  if (button === speaking.button) {
    stopSpeaking(true, true);
    return;
  }

  stopSpeaking(true, false);
  const started = await playAudio(audioPath);
  if (!started || !button.isConnected) return;

  speaking.button = button;
  speaking.idleText = idleText;
  speaking.prefix = prefix;
  speaking.dots = 1;
  speaking.startedAt = now;
  setSpeakingText();
  speaking.timer = setTimeout(animateSpeaking, SPEAK_ANIMATION_MS);
}

// ------------------------------------------------------------------- focus

function scheduleFocus() {
  state.focusSequence += 1;
  const sequence = state.focusSequence;
  for (const delay of [0, 120, 350, 700, 1200, 2000]) {
    setTimeout(() => {
      if (sequence !== state.focusSequence || state.mode !== "quiz") return;
      focusAnswer();
    }, delay);
  }
}

function cancelFocus() {
  state.focusSequence += 1;
}

function focusAnswer() {
  if (state.mode !== "quiz") return;
  const input = $("#answerInput")[0];
  if (!input) return;
  input.focus();
  try {
    input.setSelectionRange(input.value.length, input.value.length);
  } catch (error) {}
}

// --------------------------------------------------------------- quiz flow

function cancelFinishTimer() {
  if (state.finishTimer !== null) {
    clearTimeout(state.finishTimer);
    state.finishTimer = null;
  }
}

function setFeedback(text, color) {
  $("#feedback")
    .text(text)
    .css("color", color || "var(--error)");
}

function setInputBorder(color) {
  $("#answerInput").css("border-color", color);
}

function scheduleFinish() {
  if (state.finishTimer !== null) return;
  setFeedback("Correct!", "var(--success)");
  setInputBorder("var(--success)");
  state.finishTimer = setTimeout(finish, AUTO_ACCEPT_DELAY_MS);
}

function finish() {
  cancelFinishTimer();
  state.mode = "done";
  state.finishedAt = Date.now();
  cancelFocus();

  const entry = state.entry;
  if (entry && entry.word) {
    window.wordlock.recordSuccess(entry.word).catch(() => {});
  }

  $("#quizScreen").addClass("hidden");
  $("#infoScreen").removeClass("hidden");
  buildInfo(entry || {});

  if (entry && entry._audio) {
    speak(entry._audio, $("#listenButton")[0], "Listen", "Speaking");
  }
}

function onAnswerChanged() {
  if (state.mode !== "quiz") return;
  if (answersMatch($("#answerInput").val())) {
    scheduleFinish();
  } else {
    cancelFinishTimer();
    setFeedback("", "var(--error)");
    setInputBorder("var(--accent)");
  }
}

function onEnter(event) {
  if (event) event.preventDefault();
  const input = $("#answerInput");

  if (state.mode === "quiz") {
    if (answersMatch(input.val())) {
      scheduleFinish();
    } else {
      cancelFinishTimer();
      setFeedback("Incorrect, try again", "var(--error)");
      input.val("");
      setInputBorder("var(--accent)");
      focusAnswer();
    }
    return;
  }

  // Ignore a reflex Enter right after the word was auto-accepted.
  if (Date.now() - state.finishedAt < ANSWER_IGNORE_MS) return;
  window.wordlock.exitCard();
}

// -------------------------------------------------------------- info screen

function buildInfo(entry) {
  stopSpeaking(true, false);
  $("#infoScroll").scrollTop(0);

  $("#infoWord").text(entry.word || "");
  $("#infoRomaji").text(entry._romaji ? `romaji: ${entry._romaji}` : "");

  const audioPath = entry._audio || null;
  $("#listenButton")
    .toggleClass("hidden", !audioPath)
    .prop("disabled", false)
    .text("Listen");
  $("#ignoreInfoButton").prop("disabled", false).text("Ignore");

  const meta = $("#infoMeta").empty();
  if (entry.part_of_speech) {
    meta.append($("<span>", { class: "meta-pos heading", text: entry.part_of_speech }));
  }
  if (entry._reading) {
    meta.append($("<span>", { class: "meta-text", text: entry._reading }));
  }

  const body = $("#infoBody").empty();
  const definitions = Array.isArray(entry.definitions) ? entry.definitions : [];
  if (definitions.length) {
    body.append($("<div>", { class: "section-title heading", text: "Meaning" }));
    for (const definition of definitions) {
      const japanese = definition.japanese || definition.english || "";
      const vietnamese = definition.vietnamese || "";
      if (japanese) body.append($("<div>", { class: "def-jp", text: japanese }));
      if (definition.romaji) {
        body.append(
          $("<div>", { class: "romaji-text def-romaji", text: definition.romaji }),
        );
      }
      if (vietnamese) {
        body.append($("<div>", { class: "def-vi", text: vietnamese }));
      }
    }
  }

  const examples = Array.isArray(entry.examples) ? entry.examples : [];
  if (examples.length) {
    body.append($("<div>", { class: "section-title heading", text: "Examples" }));
    for (const example of examples) {
      const japanese = example.japanese || example.english || "";
      const vietnamese = example.vietnamese || "";
      if (japanese) {
        const row = $("<div>", { class: "example-row" }).append(
          $("<div>", { class: "example-jp", text: `•  ${japanese}` }),
        );
        const button = $("<button>", {
          class: "btn btn-small speak-btn",
          type: "button",
          text: "Speak",
        });
        if (example._audio) {
          button.on("click", () => speak(example._audio, button[0], "Speak", ""));
        } else {
          button.prop("disabled", true);
        }
        body.append(row.append(button));
        if (example.romaji) {
          body.append(
            $("<div>", {
              class: "romaji-text example-romaji",
              text: example.romaji,
            }),
          );
        }
      }
      if (vietnamese) {
        body.append($("<div>", { class: "example-vi", text: vietnamese }));
      }
    }
  }

  appendChips("Synonyms", entry.synonyms, entry._synonymsRomaji, "");
  appendChips("Antonyms", entry.antonyms, entry._antonymsRomaji, "error");
  appendChips("Tags", entry.tags, null, "muted");
}

function appendChips(title, values, romajiValues, tone) {
  if (!Array.isArray(values) || !values.length) return;

  const row = $("<div>", { class: "chips-row" }).append(
    $("<span>", { class: "chips-label heading", text: `${title}:` }),
  );
  values.forEach((value, index) => {
    const chip = $("<span>", { class: CHIP_CLASS[tone], text: value });
    const romaji = Array.isArray(romajiValues) ? romajiValues[index] : "";
    if (romaji) chip.append($("<span>", { class: "chip-romaji", text: ` ${romaji}` }));
    row.append(chip);
  });
  $("#infoBody").append(row);
}

async function ignoreCurrent() {
  const entry = state.entry;
  const word = entry && entry.word ? String(entry.word).trim() : "";
  if (!word) return;

  let result;
  try {
    result = await window.wordlock.ignoreWord(word);
  } catch (error) {
    result = { ok: false };
  }

  if (!result || result.ok === false) {
    if (state.mode === "quiz") {
      setFeedback("Could not save IGNORE_WORDS", "var(--error)");
    } else {
      $("#ignoreInfoButton").text("Ignore failed");
    }
    return;
  }

  if (state.mode === "quiz") {
    window.wordlock.hide();
    return;
  }

  $("#ignoreInfoButton").text("Ignored").prop("disabled", true);
}

// -------------------------------------------------------------- question UI

function showQuestion(entry) {
  state.entry = entry || {};
  state.mode = "quiz";
  stopSpeaking(false, true);
  cancelFinishTimer();
  cancelFocus();

  $("#infoScreen").addClass("hidden");
  $("#quizScreen").removeClass("hidden");

  const data = state.entry;
  $("#wordLabel").text(data.word || "");
  $("#readingLabel").text(
    data._reading && data._reading !== data.word ? data._reading : "",
  );
  $("#romajiHint").text(data._romaji ? `Romaji: ${data._romaji}` : "");
  $("#answerInput").val("").prop("disabled", false);

  setFeedback("", "var(--error)");
  setInputBorder("var(--accent)");
  scheduleFocus();
}

// ---------------------------------------------------------------- bootstrap

function start() {
  $("#answerInput").on("input", onAnswerChanged);
  $("#ignoreQuizButton, #ignoreInfoButton").on("click", ignoreCurrent);
  $("#listenButton").on("click", () => {
    if (state.entry && state.entry._audio) {
      speak(state.entry._audio, $("#listenButton")[0], "Listen", "Speaking");
    }
  });

  $(document).on("keydown", (event) => {
    if (event.key === "Enter") onEnter(event);
  });
  $(window).on("focus", () => {
    if (state.mode === "quiz") setTimeout(focusAnswer, 0);
  });
  // Re-showing a hidden BrowserWindow can restore the OS window before the
  // page is focused again; visibility changes are the reliable cue here.
  document.addEventListener("visibilitychange", () => {
    if (state.mode === "quiz" && !document.hidden) setTimeout(focusAnswer, 0);
  });
  $(window).one("pointerdown keydown", ensureAudioContext);

  window.wordlock.onStyle(applyStyle);
  window.wordlock.onQuestion(showQuestion);
  window.wordlock.onFocusAnswer(focusAnswer);
  window.wordlock.onStopAudio(() => stopSpeaking(true, true));
  window.wordlock.onCancelFocus(cancelFocus);

  window.wordlock
    .bootstrap()
    .then((bootstrap) => {
      if (bootstrap && bootstrap.style) applyStyle(bootstrap.style);
    })
    .catch(() => {});
}

$(start);
