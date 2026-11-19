// Mountain - Emscripten JavaScript library: touch detection, photo downloads,
// persistent storage (IndexedDB) and WebXR (immersive VR). MIT License.
mergeInto(LibraryManager.library, {
  mtn_js_is_touch: function () {
    var coarse = window.matchMedia && window.matchMedia('(pointer: coarse)').matches;
    return ((navigator.maxTouchPoints || 0) > 0 && coarse) ? 1 : 0;
  },

  mtn_js_download: function (name, data, size) {
    var bytes = HEAPU8.slice(data, data + size);
    var blob = new Blob([bytes], { type: 'image/png' });
    var a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = UTF8ToString(name);
    document.body.appendChild(a);
    a.click();
    setTimeout(function () { URL.revokeObjectURL(a.href); a.remove(); }, 1000);
  },

  mtn_js_sync_fs__deps: ['$FS'],
  mtn_js_sync_fs: function () {
    if (typeof FS !== 'undefined' && FS.syncfs) FS.syncfs(false, function (err) { if (err) console.warn('sync', err); });
  },

  mtn_js_set_title: function (t) { document.title = UTF8ToString(t); },

  mtn_js_ready: function () {
    var l = document.getElementById('loading');
    if (l) l.style.display = 'none';
    window.mtnReady = true;
  },

  mtn_js_xr_supported: function () { return (Module.mtnXR && Module.mtnXR.supported) ? 1 : 0; },

  mtn_js_xr_init__deps: ['$GL'],
  mtn_js_xr_init: function () {
    Module.mtnXR = { supported: 0, session: null };
    var button = document.getElementById('vr-button');
    if (navigator.xr && navigator.xr.isSessionSupported) {
      navigator.xr.isSessionSupported('immersive-vr').then(function (ok) {
        Module.mtnXR.supported = ok ? 1 : 0;
        if (ok && button) button.style.display = 'block';
      }).catch(function () {});
    }
    Module.mtnEnterVR = function () {
      if (!navigator.xr || Module.mtnXR.session) return;
      var gl = GL.currentContext.GLctx;
      navigator.xr.requestSession('immersive-vr', { optionalFeatures: ['local-floor'] }).then(function (session) {
        Module.mtnXR.session = session;
        return Promise.resolve(gl.makeXRCompatible ? gl.makeXRCompatible() : null).then(function () {
          session.updateRenderState({ baseLayer: new XRWebGLLayer(session, gl) });
          return session.requestReferenceSpace('local-floor').catch(function () { return session.requestReferenceSpace('local'); });
        }).then(function (space) {
          Module.pauseMainLoop();
          var fbId = GL.getNewId(GL.framebuffers);
          var data = _malloc(4 * (5 + 36 * 4));
          var onFrame = function (t, frame) {
            var s = frame.session;
            s.requestAnimationFrame(onFrame);
            var pose = frame.getViewerPose(space);
            if (!pose) return;
            var layer = s.renderState.baseLayer;
            GL.framebuffers[fbId] = layer.framebuffer;
            var lx = 0, ly = 0, rx = 0, ry = 0, buttons = 0;
            for (var i = 0; i < s.inputSources.length; i++) {
              var src = s.inputSources[i], gp = src.gamepad;
              if (!gp) continue;
              var ax = gp.axes, sx = ax.length >= 4 ? ax[2] : (ax[0] || 0), sy = ax.length >= 4 ? ax[3] : (ax[1] || 0);
              var pressed = function (k) { return gp.buttons[k] && gp.buttons[k].pressed; };
              if (src.handedness === 'left') {
                lx = sx; ly = sy;
                if (pressed(0)) buttons |= 2;
                if (pressed(4)) buttons |= 16;
                if (pressed(5)) buttons |= 32;
              } else {
                rx = sx; ry = sy;
                if (pressed(0)) buttons |= 1;
                if (pressed(1)) buttons |= 64;
                if (pressed(4)) buttons |= 4;
                if (pressed(5)) buttons |= 8;
              }
            }
            var f = HEAPF32, o = data >> 2;
            f[o] = lx; f[o + 1] = ly; f[o + 2] = rx; f[o + 3] = ry; f[o + 4] = buttons;
            var n = 0;
            for (var v = 0; v < pose.views.length && n < 4; v++) {
              var view = pose.views[v], vp = layer.getViewport(view), b = o + 5 + n * 36;
              f.set(view.projectionMatrix, b);
              f.set(view.transform.inverse.matrix, b + 16);
              f[b + 32] = vp.x; f[b + 33] = vp.y; f[b + 34] = vp.width; f[b + 35] = vp.height;
              n++;
            }
            _mtn_web_xr_frame(data, n, fbId, layer.framebufferWidth, layer.framebufferHeight);
          };
          session.requestAnimationFrame(onFrame);
          session.addEventListener('end', function () {
            Module.mtnXR.session = null;
            GL.framebuffers[fbId] = null;
            _free(data);
            _mtn_web_xr_ended();
            Module.resumeMainLoop();
          });
        });
      }).catch(function (e) { console.warn('WebXR', e); Module.mtnXR.session = null; });
    };
  },
});
