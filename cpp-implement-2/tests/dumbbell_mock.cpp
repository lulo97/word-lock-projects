#include <catch2/catch_test_macros.hpp>

#include "app_harness.h"

namespace {

constexpr char kDumbbellScript[] = R"JS((function () {
  var seen = {};
  function signal(type) {
    if (!seen[type]) { seen[type] = true; window.app.send(type); }
  }
  if (window.isSecureContext && window.location.protocol === 'https:') {
    signal('dumbbell:secure-origin');
  }
  if (navigator.mediaDevices &&
      typeof navigator.mediaDevices.getUserMedia === 'function') {
    signal('dumbbell:media-devices');
  }
  fetch('mediapipe/vision_bundle.mjs', { method: 'HEAD' }).then(function (response) {
    var contentType = response.headers.get('content-type') || '';
    if (response.ok && contentType.indexOf('javascript') >= 0) {
      signal('dumbbell:mjs-mime-ok');
    }
  }).catch(function () {});
  var timer = setInterval(function () {
    var mock = window.__dumbbellMock;
    if (mock && mock.modelAssetPath === 'pose_landmarker_heavy.task') {
      signal('dumbbell:model-heavy');
    }
    if (mock && mock.detectCount > 2) signal('dumbbell:mock-detecting');
    var left = document.getElementById('left-reps');
    var right = document.getElementById('right-reps');
    if (left && left.textContent !== '0 / 10') signal('dumbbell:reps-advancing');
    if (right && right.textContent !== '0 / 10') signal('dumbbell:both-arms');
    if (seen['dumbbell:secure-origin'] && seen['dumbbell:media-devices'] &&
        seen['dumbbell:mjs-mime-ok'] && seen['dumbbell:model-heavy'] &&
        seen['dumbbell:mock-detecting'] && seen['dumbbell:reps-advancing'] &&
        seen['dumbbell:both-arms']) {
      clearInterval(timer);
    }
  }, 50);
})();)JS";

}  // namespace

TEST_CASE("Dumbbell mock uses heavy model and advances reps",
          "[dumbbell][mediapipe][mock][runtime]") {
    const std::filesystem::path app = AppExecutablePath();
    REQUIRE(std::filesystem::exists(app));

    const std::filesystem::path assetDir = app.parent_path();
    const std::filesystem::path model = assetDir / L"pose_landmarker_heavy.task";
    REQUIRE(std::filesystem::exists(model));
    REQUIRE(std::filesystem::file_size(model) > 1000000);
    REQUIRE(std::filesystem::exists(assetDir / L"mediapipe/vision_bundle.mjs"));
    REQUIRE(std::filesystem::exists(
        assetDir / L"mediapipe/wasm/vision_wasm_internal.wasm"));

    TestApp::Options options;
    options.args = L"--test-open-dumbbell";
    options.commandScript = kDumbbellScript;
    options.wantDomDump = true;
    options.env = {{L"WORDLOCK_DUMBBELL_MOCK", L"1"}};
    options.prefix = L"PomodoroLockDumbbellMock";
    TestApp testApp(options);

    REQUIRE(WaitForWindow(testApp.mainClass, true, 2000) != nullptr);
    REQUIRE(WaitForNonEmptyFile(testApp.dumpFile, 2000));

    const std::vector<std::string> requiredMessages{
        "dumbbell:secure-origin", "dumbbell:media-devices", "dumbbell:mjs-mime-ok",
        "dumbbell:model-heavy", "dumbbell:mock-detecting",
        "dumbbell:reps-advancing", "dumbbell:both-arms"};
    REQUIRE(WaitForAllMessages(testApp.messageLog, requiredMessages, 2500));

    INFO("Dumbbell DOM dump: " << ReadFile(testApp.dumpFile));
    INFO("Host messages: " << ReadFile(testApp.messageLog));
    testApp.Quit();
}
