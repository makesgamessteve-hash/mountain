// Mountain - runs inside the Emscripten module before main(): mounts the
// persistent user folder (settings, photos) backed by IndexedDB. MIT License.
Module['preRun'] = Module['preRun'] || [];
Module['preRun'].push(function () {
  FS.mkdir('/user');
  FS.mount(IDBFS, {}, '/user');
  addRunDependency('mtn-syncfs');
  FS.syncfs(true, function (err) {
    if (err) console.warn('IDBFS', err);
    removeRunDependency('mtn-syncfs');
  });
});
