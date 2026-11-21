// ==UserScript==
// @name        sora-youtube
// @match       https://youtube.com/*
// @match       https://*.youtube.com/*
// @run-at      document-start
// ==/UserScript==
// Quita los anuncios de YouTube antes de que el reproductor los vea, como hacen las reglas
// json-prune de uBlock Origin que usa Brave. Si aun así sale uno, se silencia y se salta.
(function () {
  'use strict';
  const AD_KEYS = ['adPlacements', 'adSlots', 'playerAds', 'adBreakHeartbeatParams'];
  const prune = function (o) {
    if (o && typeof o === 'object') {
      for (const k of AD_KEYS) if (k in o) delete o[k];
      if (o.playerResponse) prune(o.playerResponse);
    }
    return o;
  };

  // 1. La respuesta del reproductor que viene escrita en la página.
  let initial;
  try {
    Object.defineProperty(window, 'ytInitialPlayerResponse', {
      configurable: true,
      get: function () { return initial; },
      set: function (v) { initial = prune(v); }
    });
  } catch (e) { /* ya definida: se sigue con lo demás */ }

  // 2. Las que se leen con JSON.parse (al cambiar de vídeo sin recargar).
  const parse = JSON.parse;
  JSON.parse = function () { return prune(parse.apply(this, arguments)); };

  // 3. Las que llegan por fetch (Response.json no pasa por JSON.parse).
  const realFetch = window.fetch;
  window.fetch = function (input) {
    const pending = realFetch.apply(this, arguments);
    const url = typeof input === 'string' ? input : (input && input.url) || '';
    if (!/\/youtubei\/v1\/(player|next)/.test(url)) return pending;
    return pending.then(function (response) {
      return response.clone().json().then(function (data) {
        const headers = new Headers(response.headers);
        headers.delete('content-length');
        return new Response(JSON.stringify(prune(data)),
                            { status: response.status, statusText: response.statusText, headers: headers });
      }).catch(function () { return response; });
    });
  };

  // 4. Huecos de anuncios en la portada, la búsqueda y junto al vídeo.
  const css = '#masthead-ad, #player-ads, ytd-ad-slot-renderer, ytd-in-feed-ad-layout-renderer,' +
    ' ytd-banner-promo-renderer, ytd-statement-banner-renderer, ytd-promoted-sparkles-web-renderer,' +
    ' ytd-display-ad-renderer, ytd-companion-slot-renderer, ytd-action-companion-ad-renderer,' +
    ' ytd-player-legacy-desktop-watch-ads-renderer, ytd-merch-shelf-renderer, .ytp-ad-overlay-container,' +
    ' ytd-rich-item-renderer:has(ytd-ad-slot-renderer), ytd-search-pyv-renderer { display: none !important; }';
  const addStyle = function () {
    const style = document.createElement('style');
    style.textContent = css;
    (document.head || document.documentElement).append(style);
  };
  if (document.documentElement) addStyle();
  else document.addEventListener('readystatechange', addStyle, { once: true });

  // 5. Plan B: si un anuncio llega a sonar, silenciarlo, llevarlo al final y pulsar "Saltar".
  setInterval(function () {
    const video = document.querySelector('#movie_player.ad-showing video');
    if (!video) return;
    video.muted = true;
    if (isFinite(video.duration)) video.currentTime = video.duration;
    const skip = document.querySelector('.ytp-skip-ad-button, .ytp-ad-skip-button, .ytp-ad-skip-button-modern');
    if (skip) skip.click();
  }, 300);
})();
