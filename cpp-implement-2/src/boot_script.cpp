#include "app.h"

namespace wordlock {

std::wstring BuildBootScript() {
    std::wstring script = LR"JS((function () {
  'use strict';
  var pending = {};
  var listeners = {};
  var seq = 0;

  function send(message) {
    if (window.chrome && window.chrome.webview) {
      window.chrome.webview.postMessage(message);
    }
  }

  function emit(type, data) {
    var callbacks = listeners[type] || [];
    for (var i = 0; i < callbacks.length; i++) {
      try { callbacks[i](data); } catch (error) { /* a view must not break the host */ }
    }
  }

  var api = {
    theme: "__THEME__",

    call: function (type, params) {
      return new Promise(function (resolve, reject) {
        var id = 'm' + (++seq);
        pending[id] = { resolve: resolve, reject: reject };
        var message = { type: type, id: id };
        if (params) {
          Object.keys(params).forEach(function (key) { message[key] = params[key]; });
        }
        send(message);
      });
    },

    send: function (type, params) {
      var message = { type: type };
      if (params) {
        Object.keys(params).forEach(function (key) { message[key] = params[key]; });
      }
      send(message);
    },

    on: function (type, callback) {
      if (!listeners[type]) listeners[type] = [];
      listeners[type].push(callback);
    }
  };

  window.app = api;
  window.__WORDLOCK_DUMBBELL_MOCK__ = __DUMBBELL_MOCK__;
  try {
    if (document.documentElement) document.documentElement.dataset.theme = api.theme;
  } catch (error) { /* document may still be booting */ }

  api.on('theme:changed', function (data) {
    try {
      if (document.documentElement && data && data.theme) {
        document.documentElement.dataset.theme = data.theme;
      }
    } catch (error) { /* view may be closing */ }
  });

  if (window.chrome && window.chrome.webview) {
    window.chrome.webview.addEventListener('message', function (event) {
      var data = event.data;
      if (!data || !data.type) return;
      if (data.type === 'response') {
        var request = pending[data.id];
        if (!request) return;
        delete pending[data.id];
        if (data.ok) request.resolve(data.result);
        else request.reject(new Error(data.error || 'Host request failed'));
        return;
      }
      emit(data.type, data);
    });
  }
})();)JS";

    const size_t placeholder = script.find(L"__THEME__");
    if (placeholder != std::wstring::npos) {
        script.replace(placeholder, 9, JsonEscape(g_settings.theme));
    }
    const std::wstring mockPlaceholder = L"__DUMBBELL_MOCK__";
    const size_t mockPosition = script.find(mockPlaceholder);
    if (mockPosition != std::wstring::npos) {
        script.replace(mockPosition, mockPlaceholder.size(),
                       BoolLiteral(EnvFlagEnabled(kDumbbellMockEnv)));
    }
    return script;
}

}  // namespace wordlock
