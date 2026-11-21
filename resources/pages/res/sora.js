// Conecta la página interna con Sora: window.sora (por window.cefQuery de CEF, solo en sora://).
// Misma forma que tenía con QWebChannel: sora.metodo(args..., callback) y sora.señal.connect(fn).
// Llama a onSora(fn) para usarlo.
(function () {
  const signals = ['bookmarksChanged', 'downloadsChanged', 'extensionsChanged'];
  function query(request, persistent, done) {
    window.cefQuery({
      request: JSON.stringify(request), persistent: persistent,
      onSuccess: function (r) { done(r ? JSON.parse(r)[0] : undefined); },
      onFailure: function (code, message) { console.error('sora:', message); }
    });
  }
  window.sora = new Proxy({}, {
    get: function (_, name) {
      if (signals.indexOf(name) >= 0)
        return { connect: function (fn) { query({ subscribe: name }, true, function () { fn(); }); } };
      return function () {
        const args = Array.prototype.slice.call(arguments);
        const callback = typeof args[args.length - 1] === 'function' ? args.pop() : null;
        query({ method: name, args: args }, false, function (r) { if (callback) callback(r); });
      };
    }
  });
  window.onSora = function (fn) { fn(window.sora); };
})();

// Utilidades comunes
function el(tag, attrs, children) {
  const node = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs || {})) {
    if (k === 'text') node.textContent = v;
    else if (k.startsWith('on')) node.addEventListener(k.slice(2), v);
    else node.setAttribute(k, v);
  }
  (children || []).forEach(function (c) { node.append(c); });
  return node;
}
function hostOf(url) { try { return new URL(url).host; } catch (e) { return url; } }
