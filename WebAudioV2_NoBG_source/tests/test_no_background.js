const fs=require('fs'),assert=require('assert');
const page=fs.readFileSync(__dirname+'/../app/web.html','utf8');
const worker=fs.readFileSync(__dirname+'/../app/worker.c','utf8');
for(const forbidden of ['bgplayer','startBackground','backgroundPlaying','/stream.wav','Jump to Live','Background playback']){
  assert(!page.includes(forbidden),'Background playback feature remains in page: '+forbidden);
}
assert(!worker.includes('/stream.wav'),'WAV endpoint remains in server');
assert(page.includes('id="listen"'),'Listen button removed');
assert(page.includes('id="platform"'),'Phone override removed');
assert(page.includes("navigator.audioSession.type='playback'"),'iOS playback routing removed');
console.log('Background playback removed; standard player + iOS routing retained: PASS');
