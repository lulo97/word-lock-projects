/** Settings window, rewritten with jQuery. */

const $ = window.jQuery;
const api = window.wordlock;

const $delay = $("#delayEntry");
const $volume = $("#volumeSlider");
const $volumeValue = $("#volumeValue");
const $startup = $("#startupSwitch");
const $ignore = $("#ignoreBox");
const $status = $("#status");
const $save = $("#saveButton");
const $cancel = $("#cancelButton");

let saving = false;

const parseWords = (text) =>
  String(text || "")
    .trim()
    .split(/[\s,;]+/)
    .filter(Boolean);

const setStatus = (message) => $status.text(message || "");

function updateVolumeLabel() {
  const value = Number($volume.val());
  $volumeValue.text(`x${(Number.isFinite(value) ? value : 2).toFixed(1)}`);
}

async function initialise() {
  try {
    const init = await api.settingsInit();

    if (init.style) window.WordLockTheme.applyStyle(init.style);

    $("#configPath").text(init.configFile || "");
    $("#styleNote").text(
      `UI style: ${init.styleName || ""} — a new random style is picked after each completed word.`,
    );

    $delay.val(String(init.delayMs));
    $volume.attr({ min: init.minVolume, max: init.maxVolume }).val(String(init.volume));
    updateVolumeLabel();
    $startup.prop("checked", Boolean(init.startWithWindows));
    $ignore.val((init.ignoreWords || []).join("\n"));
  } catch (error) {
    setStatus("Could not read config.json.");
    return;
  }

  $delay.trigger("focus").trigger("select");
}

async function save() {
  if (saving) return;

  const rawDelay = String($delay.val()).trim();
  if (!rawDelay || !Number.isFinite(Number(rawDelay))) {
    setStatus("AUTO_SHOW_DELAY_MS must be a number in milliseconds.");
    $delay.trigger("focus");
    return;
  }

  const volume = Number($volume.val());
  if (!Number.isFinite(volume)) {
    setStatus("SOUND_VOLUME must be a number.");
    return;
  }

  saving = true;
  $save.add($cancel).prop("disabled", true);
  setStatus("");

  let result;
  try {
    result = await api.saveSettings({
      delayMs: rawDelay,
      volume,
      startWithWindows: $startup.prop("checked"),
      ignoreWords: parseWords($ignore.val()),
    });
  } catch (error) {
    result = { ok: false, error: String(error.message || error) };
  }

  saving = false;
  $save.add($cancel).prop("disabled", false);

  if (!result || !result.ok) {
    setStatus((result && result.error) || "Could not save config.json.");
    return;
  }

  api.closeSettings();
}

function close() {
  if (!saving) api.closeSettings();
}

$volume.on("input", updateVolumeLabel);
$save.on("click", save);
$cancel.on("click", close);
$(document).on("keydown", (event) => {
  if (event.key === "Escape") {
    event.preventDefault();
    close();
  }
});

initialise();
