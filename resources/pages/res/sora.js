// Conecta la página interna con Sora: window.sora (QWebChannel). Llama a onSora(fn) para usarlo.
(function () {
  const waiting = [];
  window.onSora = function (fn) { window.sora ? fn(window.sora) : waiting.push(fn); };
  new QWebChannel(qt.webChannelTransport, function (channel) {
    window.sora = channel.objects.sora;
    waiting.splice(0).forEach(function (fn) { fn(window.sora); });
  });
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
