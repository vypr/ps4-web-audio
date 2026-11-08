'use strict';
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const path=require('path');
const page=fs.readFileSync(path.resolve(__dirname,'../app/web.html'),'utf8');
const match=page.match(/<script>([\s\S]*?)<\/script>/);
assert(match,'Web page script missing');
new vm.Script(match[1],{filename:'web_player.js'});
function simulate({userAgent,platform,maxTouchPoints=0,stored=null}){
 const elements=new Map();
 const local={};if(stored)local['ps4_web_audio_v2_phone_mode']=stored;
 function el(id){if(!elements.has(id))elements.set(id,{
   value:id==='platform'?'auto':id==='vol'?'100':'',textContent:'',style:{},className:'',disabled:false,
   listeners:{},addEventListener(event,callback){this.listeners[event]=callback;},
   setAttribute(){},removeAttribute(){},pause(){},removeAttribute(){},load(){},
 });return elements.get(id);}
 const radios=['fast','balanced','studio'].map(v=>({value:v,checked:v==='balanced'}));
 const doc={getElementById:el,querySelector(q){return radios.find(r=>r.checked);},querySelectorAll(){return radios;}};
 const sandbox={document:doc,navigator:{userAgent,platform,maxTouchPoints},window:{},localStorage:{getItem:k=>local[k]??null,setItem:(k,v)=>{local[k]=v;}},
  setInterval(){},fetch:async()=>({json:async()=>({capture_open:1,capture_fresh:1,packets:0,underruns:0,clients:0,max_listeners:8})}),
  console,Uint8Array,Float32Array,DataView,Math,Number,AbortController,MediaMetadata:function(){}};
 vm.createContext(sandbox);vm.runInContext(match[1],sandbox);
 const detect=vm.runInContext('detectPlatform()',sandbox);
 return {el,detect,local,actual:()=>vm.runInContext('selectedPlatform()',sandbox)};
}
const ios=simulate({userAgent:'Mozilla/5.0 (iPhone; CPU iPhone OS 18_1 like Mac OS X)',platform:'iPhone'});
assert.strictEqual(ios.detect,'ios');assert.strictEqual(ios.actual(),'ios');
const ipad=simulate({userAgent:'Mozilla/5.0 (Macintosh; Intel Mac OS X)',platform:'MacIntel',maxTouchPoints:5});
assert.strictEqual(ipad.detect,'ios');
const android=simulate({userAgent:'Mozilla/5.0 (Linux; Android 15; Pixel)',platform:'Linux armv8l'});
assert.strictEqual(android.detect,'android');
const desktop=simulate({userAgent:'Mozilla/5.0 (Windows NT 10.0; Win64; x64)',platform:'Win32'});
assert.strictEqual(desktop.detect,'desktop');
ios.el('platform').value='android';ios.el('platform').listeners.change();
assert.strictEqual(ios.actual(),'android');assert.strictEqual(ios.local['ps4_web_audio_v2_phone_mode'],'android');
assert.match(ios.el('platformSummary').textContent,/manual override/);
const stored=simulate({userAgent:'Mozilla/5.0 (iPhone; CPU iPhone OS 18_1 like Mac OS X)',platform:'iPhone',stored:'android'});
assert.strictEqual(stored.actual(),'android');
console.log('Phone mode tests: PASS (iPhone, iPadOS, Android, desktop, override, persistence)');
