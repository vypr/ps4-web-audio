'use strict';
const fs=require('fs'),vm=require('vm'),assert=require('assert'),path=require('path');
const html=fs.readFileSync(path.resolve(__dirname,'../app/web.html'),'utf8');
const script=html.match(/<script>([\s\S]*?)<\/script>/);
assert(script,'Embedded player script missing');
new vm.Script(script[1],{filename:'web_player.js'});

function browser(userAgent, platform, {noAPI=false,throws=false}={}) {
 const writes=[],elements=new Map();
 function el(id){
  if(!elements.has(id))elements.set(id,{
   value:id==='platform'?'auto':id==='vol'?'100':'',textContent:'',style:{},className:'',disabled:false,volume:1,
   addEventListener(){},setAttribute(){},removeAttribute(){},pause(){},load(){},play(){return Promise.resolve();}
  });
  return elements.get(id);
 }
 const radios=['fast','balanced','studio'].map(v=>({value:v,checked:v==='balanced'}));
 const doc={getElementById:el,querySelector:()=>radios.find(r=>r.checked),querySelectorAll:()=>radios};
 const audioSession={};
 Object.defineProperty(audioSession,'type', {configurable:true,set(v){writes.push(v);if(throws)throw Error('not allowed');}});
 const navigator={userAgent,platform,maxTouchPoints:0};
 if(!noAPI)navigator.audioSession=audioSession;
 const context={
  document:doc,navigator,window:{},localStorage:{getItem(){return null;},setItem(){}},
  setInterval(){},setTimeout(){},fetch:async()=>({ok:false,status:404,json:async()=>({})}),
  MediaMetadata:function(){},Float32Array,Uint8Array,DataView,AbortController,Math,Number,console
 };
 vm.createContext(context);vm.runInContext(script[1],context);
 return {writes,el,context,run(expr){return vm.runInContext(expr,context);}};
}
const IOS_UA='Mozilla/5.0 (iPhone; CPU iPhone OS 18_1 like Mac OS X)';
const iphone=browser(IOS_UA,'iPhone');
// Main low-latency 'Listen' button must set playback routing synchronously,
// BEFORE the first awaited promise / AudioContext construction.
iphone.run("start('game')");
assert.deepStrictEqual(iphone.writes,['playback']);
iphone.run('stop()');

const android=browser('Mozilla/5.0 (Linux; Android 15; Pixel)','Linux armv8l');
android.run('requestPlaybackAudioSession()');
assert.strictEqual(android.writes.length,0,'Android mode should not be modified');
const pc=browser('Mozilla/5.0 (Windows NT 10.0; Win64; x64)','Win32');
pc.run('requestPlaybackAudioSession()');
assert.strictEqual(pc.writes.length,0,'Desktop mode should not be modified');
const noAPI=browser(IOS_UA,'iPhone',{noAPI:true});
assert.doesNotThrow(()=>noAPI.run('requestPlaybackAudioSession()'));
const blocked=browser(IOS_UA,'iPhone',{throws:true});
assert.doesNotThrow(()=>blocked.run('requestPlaybackAudioSession()'));
assert.deepStrictEqual(blocked.writes,['playback']);
const forcedIOS=browser('Mozilla/5.0 (Linux; Android 15; Pixel)','Linux armv8l');
forcedIOS.el('platform').value='ios';
forcedIOS.run('requestPlaybackAudioSession()');
assert.deepStrictEqual(forcedIOS.writes,['playback'],'Manual iOS selection is supported');
console.log('iOS audioSession playback routing: PASS (Listen, Android/PC unaffected, optional API)');
