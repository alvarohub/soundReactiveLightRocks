#include "webui.h"

#include <WiFi.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

namespace {

const char kPage[] PROGMEM = R"PAGE(<!doctype html>
<html lang=en>
<head>
<meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1,maximum-scale=1">
<meta http-equiv=cache-control content=no-store>
<title>Light Rocks</title>
<style>
:root{color-scheme:dark}
*{box-sizing:border-box}
/* Right 1/6 of the viewport (16.7vw) is kept empty as a safe thumb-scroll gutter. */
body{margin:0;padding:14px 16.7vw 28px 12px;max-width:640px;background:#0f1115;color:#e6edf3;font:16px/1.45 system-ui,-apple-system,sans-serif;border-right:1px dashed #21262e}
h1{margin:0 0 10px;font-size:1.15rem;font-weight:600;display:flex;justify-content:space-between;align-items:baseline;gap:10px}
h1 span{font-size:.72rem;color:#8b949e;font-weight:400}
.padopen{width:100%;padding:12px 14px;margin-bottom:10px;border-radius:12px;border:2px solid #58a6ff;background:linear-gradient(135deg,#16233a,#171b21);color:#f0f6fc;font-size:1rem;font-weight:700;display:flex;justify-content:space-between;align-items:center;cursor:pointer;touch-action:manipulation}
.padopen small{font-weight:500;font-size:.76rem;color:#8b949e}
.card{background:#171b21;border:1px solid #262c36;border-radius:12px;padding:12px 14px;margin-bottom:10px}
.row{display:flex;justify-content:space-between;align-items:baseline;margin:9px 0 2px;font-size:.9rem;color:#9da7b3}
.row.first{margin-top:0}
.row b{color:#e6edf3;font-variant-numeric:tabular-nums;font-weight:600}
.hint{color:#8b949e;font-size:.76rem;margin:2px 0 4px}
input[type=range]{-webkit-appearance:none;appearance:none;width:100%;height:26px;background:transparent;margin:0}
input[type=range]::-webkit-slider-runnable-track{height:6px;border-radius:3px;background:#2b323c}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:22px;height:22px;margin-top:-8px;border-radius:50%;background:#3fb950}
input[type=range]::-moz-range-track{height:6px;border-radius:3px;background:#2b323c}
input[type=range]::-moz-range-thumb{width:22px;height:22px;border:0;border-radius:50%;background:#3fb950}
.cwrap{display:flex;align-items:center;gap:14px;margin:6px 0 8px}
#wheel{width:152px;height:152px;border-radius:50%;cursor:crosshair;touch-action:none;user-select:none;-webkit-user-select:none;box-shadow:0 0 0 2px #2b323c;flex:none}
.cbox{width:54px;height:54px;border-radius:10px;border:2px solid #e6edf3;box-shadow:0 2px 10px rgba(0,0,0,.45);flex:none}
.btn:disabled{opacity:.35}
.btn.wide{width:100%;margin-top:8px}
.bar{position:relative;height:24px;border-radius:7px;background:#21262e;border:1px solid #30363d;overflow:visible;margin:4px 0 12px;cursor:ew-resize;touch-action:none;user-select:none;-webkit-user-select:none}
.bar i{position:absolute;left:0;top:0;height:100%;width:0;border-radius:6px;transition:width .07s linear;pointer-events:none}
.bar u{position:absolute;top:-3px;bottom:-3px;width:4px;border-radius:2px;background:#f0f6fc;box-shadow:0 0 6px rgba(0,0,0,.9);pointer-events:none}
.bar u:after{content:"";position:absolute;top:-4px;left:-5px;width:14px;height:8px;border-radius:4px;background:#f0f6fc}
.rline{display:flex;align-items:center;gap:8px}
.rline input[type=range]{flex:1}
.tbtn{padding:6px 10px;border-radius:8px;border:1px solid #3a424d;background:#21262e;color:#8b949e;font-size:.82rem;font-weight:600;white-space:nowrap;flex:none}
.tbtn.on{background:#17301e;border-color:#2ea043;color:#7ee787}
.tbtn.off{background:#2b1d1d;border-color:#6b2b2b;color:#ff9d95}
.tbtn.solo{background:#2a2410;border-color:#d29922;color:#e3b341}
.stepbtn{width:40px;height:34px;border-radius:8px;border:1px solid #3a424d;background:#21262e;color:#e6edf3;font-size:1rem;font-weight:700;flex:none;touch-action:manipulation}
.stepbtn:active{background:#30363d}
.seg{display:flex;gap:8px;margin-top:4px}
.seg button{flex:1;padding:11px 8px;font-size:.95rem;border-radius:9px;border:2px solid #3a424d;background:#21262e;color:#8b949e;font-weight:600}
.seg button.on{background:#17301e;border-color:#2ea043;color:#7ee787}
.seg button.off{background:#2b1d1d;border-color:#6b2b2b;color:#ff9d95}
.seg button.sel{outline:3px solid #58a6ff;outline-offset:2px}
.seg.tight button{padding:8px 4px;font-size:.84rem}
.chips{display:flex;flex-wrap:wrap;gap:7px;margin:6px 0 2px}
.chip{padding:9px 11px;border-radius:9px;border:2px solid #3a424d;background:#21262e;color:#8b949e;font-size:.84rem;font-weight:600;touch-action:manipulation}
.chip.on{background:#17301e;border-color:#2ea043;color:#7ee787}
.chip:disabled{opacity:.35}
.stats{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-top:8px}
.stats div{background:#21262e;border-radius:9px;padding:8px 4px;text-align:center}
.stats span{display:block;font-size:.68rem;letter-spacing:.04em;color:#8b949e;text-transform:uppercase}
.stats b{font-size:.95rem;font-variant-numeric:tabular-nums}
.sw{position:relative;display:inline-block;width:52px;height:28px;flex:none}
.sw input{opacity:0;width:0;height:0;position:absolute}
.sw i{position:absolute;inset:0;background:#2b323c;border-radius:14px;transition:background .15s;cursor:pointer}
.sw i:before{content:"";position:absolute;width:22px;height:22px;left:3px;top:3px;border-radius:50%;background:#8b949e;transition:transform .15s,background .15s}
.sw input:checked + i{background:#238636}
.sw input:checked + i:before{transform:translateX(24px);background:#fff}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.btn{padding:11px;border-radius:9px;border:1px solid #2b323c;background:#21262e;color:#e6edf3;font-size:.95rem}
.btn.alt{background:#16261a;border-color:#2ea043}
.btn.warn{background:#2a1a1a;border-color:#6b2b2b;color:#ff7b72}
#pmsg{color:#8b949e;font-size:.78rem;font-weight:400}
.ptitle{margin:0 0 8px;padding-left:9px;border-left:4px solid #58a6ff;font-weight:600;color:#f0f6fc}
.ptitle small{display:block;font-weight:400;font-size:.76rem;color:#8b949e;margin-top:2px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
/* Live readouts must never reflow: a value that grows ('lvl 7%' to 'lvl 100%') would
   otherwise wrap the label and shove the meter and every card below it up and down. */
#mt0{white-space:nowrap;flex:none}
#mtlbl{min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
/* ---- full-screen pad view ---- */
/* Same 1/6 viewport thumb gutter as the control page, and it scrolls: the pads
   are tall, and a non-scrollable overlay on a phone traps the user. */
#padscreen{position:fixed;inset:0;z-index:100;background:#0b0e13;display:none;flex-direction:column;padding:8px 16.7vw 22px 8px;gap:6px;user-select:none;-webkit-user-select:none;touch-action:pan-y;overflow-y:auto;-webkit-overflow-scrolling:touch;border-right:1px dashed #21262e}
#padscreen.open{display:flex}
.padtop{display:flex;justify-content:space-between;align-items:center;gap:8px;flex:none}
.padtop b{font-size:.98rem;color:#f0f6fc}
.padclose{padding:7px 14px;border-radius:9px;border:1px solid #3a424d;background:#21262e;color:#e6edf3;font-size:.86rem;font-weight:600;cursor:pointer}
/* Strip A | Strip B side by side; each column stacks only the layers that exist. */
#padgroups{display:grid;grid-template-columns:1fr 1fr;gap:8px;align-items:start}
.padcol{display:flex;flex-direction:column;gap:6px;min-width:0}
.colhdr{display:flex;justify-content:space-between;align-items:center;gap:4px;font-size:.78rem;font-weight:700;color:#f0f6fc;padding:1px 2px}
.colhdr span{font-weight:400;font-size:.66rem;color:#8b949e;font-variant-numeric:tabular-nums}
.pgrp{flex:none;border-radius:10px;border:2px solid var(--c,#58a6ff);background:#141820;padding:5px 6px;box-shadow:inset 0 0 14px rgba(0,0,0,.6)}
.pghdr{display:flex;justify-content:space-between;align-items:center;gap:4px;font-size:.72rem;margin-bottom:4px}
.pghdr b{color:#f0f6fc;display:flex;align-items:center;gap:6px}
.pdot{width:11px;height:11px;border-radius:50%;background:var(--c,#58a6ff);display:inline-block;box-shadow:0 0 8px var(--c,#58a6ff)}
.pghdr span{color:#8b949e;font-size:.6rem;font-variant-numeric:tabular-nums;text-align:right;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.pgbar{height:3px;border-radius:2px;background:#21262e;overflow:hidden;margin-bottom:5px}
.pgbar i{display:block;height:100%;width:0;background:var(--c,#58a6ff);transition:width 60ms linear}
.pgrid{display:grid;grid-template-columns:1fr 1fr;gap:5px}
.pad{position:relative;min-height:42px;border-radius:8px;border:2px solid var(--c,#58a6ff);background:radial-gradient(circle at 38% 32%,rgba(255,255,255,.13),rgba(15,17,21,.93) 72%);color:#f0f6fc;font-weight:700;font-size:.78rem;display:flex;flex-direction:column;align-items:center;justify-content:center;cursor:pointer;user-select:none;-webkit-user-select:none;-webkit-tap-highlight-color:transparent;touch-action:none;box-shadow:0 0 14px -5px var(--c,#58a6ff),inset 0 0 16px rgba(0,0,0,.75);transition:transform 55ms ease,box-shadow 80ms ease,background 80ms ease}
.pad small{font-size:.52rem;font-weight:500;color:#9da7b3;margin-top:0;letter-spacing:0;line-height:1.1;text-align:center}
.pad.hit{transform:scale(.95);background:radial-gradient(circle at 50% 50%,#fff 0%,var(--c,#58a6ff) 58%,#141820 100%);box-shadow:0 0 26px 3px var(--c,#58a6ff),inset 0 0 12px #fff;color:#0b0e13}
.pad.hit small{color:#0b0e13}
</style>
</head>
<body>
<h1>Light Rocks <span id=host></span></h1>
<button type=button class=padopen id=openpads><span>Full-Screen Effect Pads</span><small>one group per layer &rarr;</small></button>
<div id=padscreen>
 <div class=padtop>
  <b>Live Effect Pads</b>
  <button type=button class=padclose id=closepads>&times; Back to Controls</button>
 </div>
 <div id=padgroups></div>
</div>

<div class=card>
<div class=ptitle style="border-left-color:#3fb950">Microphone (global)<small>shared input for every layer</small></div>
<div class="row first"><span>Microphone gain</span><b id=gainDbv>-</b></div>
<div class=rline>
<input type=range id=gainDb min=-24 max=18 step=3>
<button type=button id=agcbtn class=tbtn>AGC</button>
</div>
<div class=hint id=agchint>negative attenuates: use it when the room is too loud</div>
<div class=stats>
<div><span>Broadband</span><b id=sLvl>0%</b></div>
<div><span>dBFS</span><b id=sDb>-</b></div>
<div><span>Peak</span><b id=sPk>-</b></div>
</div>
</div>

<div class=card>
<div class="row first"><span>Physical strip</span><b id=tabhint style="color:#8b949e;font-weight:400;font-size:.76rem"></b></div>
<div class=seg id=pick></div>
<div class=row style="margin-top:10px"><span>Strip brightness</span><b id=brightnessPctv>-</b></div>
<input type=range id=brightnessPct min=1 max=100 step=1>
<div class="row" style="margin-top:12px"><span>Light layers inside</span><b id=lcv>-</b></div>
<div class=seg id=layertabs></div>
<div class=rline style="margin-top:8px">
<button type=button id=addlayer class=tbtn>+ Add layer</button>
<button type=button id=remlayer class=tbtn>Remove</button>
<button type=button id=solobtn class=tbtn style="margin-left:auto">Solo</button>
</div>
<div class=ptitle id=ptitle style="margin:12px 0 8px"></div>
<div class=row><span>Layer power</span><label class=sw><input type=checkbox id=pwr><i></i></label></div>
<div id=ipanel></div>
</div>

<div class=card id=trigbox></div>
<div class=card id=dispbox></div>
<div class=card id=animbox></div>
<div class=card id=calbox></div>

<div class=card>
<div class="row first"><span>Presets</span><b id=pmsg></b></div>
<div class=grid>
<button id=sp1 class=btn>Save 1</button>
<button id=ld1 class="btn alt">Load 1</button>
<button id=sp2 class=btn>Save 2</button>
<button id=ld2 class="btn alt">Load 2</button>
<button id=sp3 class=btn>Save 3</button>
<button id=ld3 class="btn alt">Load 3</button>
</div>
<button id=rst class="btn warn wide">Factory reset</button>
</div>

<div class=hint style="text-align:center">BtnA: next strip &middot; BtnB: cycle triggers &middot; hold A: Wi-Fi &middot; hold B: save</div>

<script>
var S=0,L=0,ST=null,BUILT=false,SEEDED=false,TRIG=1,EFF=0,REV=0,LOOP=1,RETRIG=1,CALIB=0,SOLO=-1;
var TB={thr:1,beat:2,tap:4,accel:8,gyro:16,lfo:32};
var CUR_COL='#ff6a00',WHEEL_BG=null,WX=76,WY=76,WDRAG=false,TDRAG=-1,MSGTMR=0;
var INFLIGHT=false,POLL_BUSY=false,LAST=null,PTMR=0,AFTER=null,HOLD_TMRS={},AGC=0;
var USER_TS=0,RETRY=0;

function $(i){return document.getElementById(i);}

/* ---- "the user is touching something" ----
   Polls must never overwrite a control mid-gesture. Catching the end of a drag
   on the window (not on the element) also fixes the old stuck state: a finger
   that left the bar, or a scrolled page, used to leave the drag flag set and
   silently stop all polling until the page was reloaded. */
var DRAGGING=0;
function editing(){return WDRAG||TDRAG>=0||DRAGGING>0;}
document.addEventListener('pointerdown',function(e){
 var t=e.target;
 if(t&&t.tagName==='INPUT'&&t.type==='range')DRAGGING=1;
},true);
function endDrag(){
 DRAGGING=0;
 if(WDRAG){WDRAG=false;flush();}
 TDRAG=-1;
}
document.addEventListener('pointerup',endDrag,false);
document.addEventListener('pointercancel',endDrag,false);
window.addEventListener('blur',endDrag);
function hex2(n){var s=Math.round(n).toString(16);return s.length<2?'0'+s:s;}
function hsvToRgb(h,s,v){
 var c=v*s,x=c*(1-Math.abs((h/60)%2-1)),m=v-c,r=0,g=0,b=0;
 if(h<60){r=c;g=x;}else if(h<120){r=x;g=c;}else if(h<180){g=c;b=x;}
 else if(h<240){g=x;b=c;}else if(h<300){r=x;b=c;}else{r=c;b=x;}
 return [(r+m)*255,(g+m)*255,(b+m)*255];
}
function rgbToHs(r,g,b){
 r/=255;g/=255;b/=255;
 var mx=Math.max(r,g,b),mn=Math.min(r,g,b),d=mx-mn,h=0,s=mx===0?0:d/mx;
 if(d>0){
  if(mx===r)h=60*(((g-b)/d)%6);
  else if(mx===g)h=60*((b-r)/d+2);
  else h=60*((r-g)/d+4);
  if(h<0)h+=360;
 }
 return [h,s];
}
function phys(){return ST&&ST.strips?ST.strips[S]:null;}
function cur(){var p=phys();return p&&p.layers?p.layers[L]:null;}
function colOf(layer){return '#'+(layer&&layer.color?layer.color:'808080');}
function layerCount(){var p=phys();return p?Math.max(1,p.layerCount):1;}
function nameOf(i){return 'Strip '+(i+1);}

/* ---- one request in flight, latest value wins ---- */
/* The board's web server serves one socket at a time. Two overlapping requests
   (a poll landing on top of a slider POST) made it drop the second one, and
   without a timeout that fetch never settled: INFLIGHT stayed true and the page
   silently refused to send anything again while the LEDs kept running. Every
   request now has a deadline, and both flags are cleared whatever happens. */
function abortAfter(ms){
 if(!window.AbortController)return {};
 var ctl=new AbortController();
 setTimeout(function(){try{ctl.abort();}catch(e){}},ms);
 return {signal:ctl.signal};
}
function busySocket(){return INFLIGHT||POLL_BUSY;}
function flush(){
 if(PTMR){clearTimeout(PTMR);PTMR=0;}
 if(busySocket()){
  // Only one socket at a time: wait for the state read to finish.
  if(!PTMR)PTMR=setTimeout(function(){PTMR=0;flush();},60);
  return;
 }
 if(!LAST){
  // Nothing to send: run a pending state refresh now, otherwise it waits for a
  // request that may never come and the page keeps a stale view of the board.
  var f=AFTER;AFTER=null;if(f)f();
  return;
 }
 var url=LAST;LAST=null;
 INFLIGHT=true;
 var done=function(ok){
  INFLIGHT=false;
  if(!ok&&RETRY<2&&!LAST){RETRY++;LAST=url;}
  else if(ok){RETRY=0;}
  if(LAST){if(!PTMR)PTMR=setTimeout(function(){PTMR=0;flush();},45);return;}
  if(AFTER){var f=AFTER;AFTER=null;f();}
 };
 var opt=abortAfter(2500);
 opt.method='POST';
 fetch('/api/params?'+url,opt).then(function(r){done(r.ok);},function(){done(false);});
}
function fetchState(){
 var opt=abortAfter(2500);
 opt.cache='no-store';
 return fetch('/api/state',opt).then(function(r){
  if(!r.ok)throw new Error('http '+r.status);
  return r.json();
 });
}
function syncState(then){
 AFTER=function(){
  fetchState().then(function(s){apply(s);if(then)then();})
   .catch(function(){flashMsg('sync lost - reload the page');});
 };
 flush();
}
/* Any request the page makes counts as "the user is busy": a poll that lands in
   that window would otherwise fight the control being moved. */
function sendRaw(q){
 LAST=q;USER_TS=Date.now();
 if(!PTMR&&!busySocket()){PTMR=setTimeout(function(){PTMR=0;flush();},55);}
}
function sendLayer(k,v){sendRaw('strip='+S+'&layer='+L+'&'+k+'='+encodeURIComponent(v));}
function sendPhys(k,v){sendRaw('strip='+S+'&'+k+'='+encodeURIComponent(v));}

/* ---- threshold drag on the live level meter ---- */
function pickThr(e,commit){
 var bar=$('bar0'),rect=bar.getBoundingClientRect();
 var pct=Math.round(((e.clientX-rect.left)/rect.width)*100);
 if(pct<0)pct=0;if(pct>100)pct=100;
 var c=cur();if(c)c.thresholdPct=pct;
 sendLayer('thresholdPct',pct);
 if(commit)flush();
 refreshTitle();
}

/* ---- colour wheel ---- */
function initWheel(){
 var cv=$('wheel'),w=cv.width,r=w/2;
 WHEEL_BG=document.createElement('canvas');
 WHEEL_BG.width=w;WHEEL_BG.height=w;
 var ctx=WHEEL_BG.getContext('2d'),img=ctx.createImageData(w,w),d=img.data;
 for(var y=0;y<w;y++){
  for(var x=0;x<w;x++){
   var dx=x-r,dy=y-r,dist=Math.sqrt(dx*dx+dy*dy);
   if(dist<=r){
    var ang=(Math.atan2(dy,dx)*180/Math.PI+360)%360;
    var rgb=hsvToRgb(ang,dist/r,1);
    var p=(y*w+x)*4;
    d[p]=rgb[0];d[p+1]=rgb[1];d[p+2]=rgb[2];
    d[p+3]=dist>r-1.5?Math.round((r-dist)/1.5*255):255;
   }
  }
 }
 ctx.putImageData(img,0,0);
}
function drawWheel(){
 if(!WHEEL_BG)return;
 var cv=$('wheel'),ctx=cv.getContext('2d');
 ctx.clearRect(0,0,cv.width,cv.height);
 ctx.drawImage(WHEEL_BG,0,0);
 ctx.beginPath();ctx.arc(WX,WY,7,0,Math.PI*2);
 ctx.lineWidth=2.5;ctx.strokeStyle='#0f1115';ctx.stroke();
 ctx.beginPath();ctx.arc(WX,WY,5.5,0,Math.PI*2);
 ctx.lineWidth=2;ctx.strokeStyle='#fff';ctx.stroke();
}
function setColorHex(hex){
 CUR_COL=hex;
 $('cbox').style.background=hex;
 $('colv').textContent=hex;
 var r=parseInt(hex.substr(1,2),16)||0,g=parseInt(hex.substr(3,2),16)||0,b=parseInt(hex.substr(5,2),16)||0;
 var hs=rgbToHs(r,g,b),rad=76,ang=hs[0]*Math.PI/180,dist=hs[1]*(rad-3);
 WX=rad+Math.cos(ang)*dist;WY=rad+Math.sin(ang)*dist;
 drawWheel();
}
function pickWheel(e,commit){
 var cv=$('wheel'),rect=cv.getBoundingClientRect(),rad=cv.width/2;
 var sx=(e.clientX-rect.left)*(cv.width/rect.width)-rad;
 var sy=(e.clientY-rect.top)*(cv.height/rect.height)-rad;
 var dist=Math.sqrt(sx*sx+sy*sy),maxR=rad-3;
 if(dist>maxR){sx*=maxR/dist;sy*=maxR/dist;dist=maxR;}
 WX=rad+sx;WY=rad+sy;
 var ang=(Math.atan2(sy,sx)*180/Math.PI+360)%360;
 var rgb=hsvToRgb(ang,dist/maxR,1);
 CUR_COL='#'+hex2(rgb[0])+hex2(rgb[1])+hex2(rgb[2]);
 $('cbox').style.background=CUR_COL;
 drawWheel();
 refreshTitle();
 var c=cur();if(c)c.color=CUR_COL.slice(1);
 sendLayer('color',CUR_COL.slice(1));
 if(commit)flush();
}

/* ---- fingertip pads ---- */
function padHit(p,l,eff,down){
 var el=$('pad_'+p+'_'+l+'_'+eff),key=p+'_'+l+'_'+eff;
 var url='/api/tap?strip='+p+'&layer='+l+'&effect='+eff+'&down='+(down?1:0);
 if(down){
  if(el)el.classList.add('hit');
  fetch(url,{method:'POST'}).catch(function(){});
  if(HOLD_TMRS[key])clearInterval(HOLD_TMRS[key]);
  HOLD_TMRS[key]=setInterval(function(){fetch(url,{method:'POST'}).catch(function(){});},900);
 }else{
  if(HOLD_TMRS[key]){clearInterval(HOLD_TMRS[key]);HOLD_TMRS[key]=0;}
  if(el&&el.classList.contains('hit')){
   el.classList.remove('hit');
   fetch(url,{method:'POST'}).catch(function(){});
  }
 }
}

/* ---- strip / layer selectors and the pad overlay ---- */
function buildSelectors(n){
 var p='';
 for(var i=0;i<n;i++)p+='<button id=sel'+i+'>Strip '+(i+1)+'</button>';
 $('pick').innerHTML=p;
 for(var j=0;j<n;j++){
  (function(k){$('sel'+k).onclick=function(){
   if(CALIB){CALIB=0;sendPhys('calib',0);}
   flush();S=k;if(L>=layerCount())L=0;fillInputs();live();
  };})(j);
 }
}
var TAB_N=-1;
function buildLayerTabs(){
 var p='',n=layerCount();
 // A single layer has nothing to choose between, so no pointless tab row.
 // The count says so in words, otherwise "1 of 4" with no tabs reads as four
 // layers hiding somewhere.
 if(n!==TAB_N){
  TAB_N=n;
  for(var i=0;i<n;i++)p+='<button id=lt'+i+'>Layer '+(i+1)+'</button>';
  var box=$('layertabs');
  box.innerHTML=p;
  box.style.display=n>1?'':'none';
  for(var j=0;j<n;j++){
   (function(k){$('lt'+k).onclick=function(){flush();L=k;fillInputs();live();};})(j);
  }
 }
 if($('lcv'))$('lcv').textContent=n>1?(n+' of 4'):'1 of 4 \u00b7 single beam';
 paintLayerTabs();
}

/* Each tab carries its layer's colour, so "the green beam" is one tap away
   without reading the titles. The selected tab is filled with that colour; the
   text flips to dark on light colours so it stays readable. */
function paintLayerTabs(){
 var p=phys();
 if(!p||!p.layers)return;
 for(var i=0;i<TAB_N;i++){
  var b=$('lt'+i);
  if(!b)continue;
  var lay=p.layers[i],col='#'+(lay&&lay.color?lay.color:'808080');
  var r=parseInt(col.substr(1,2),16)||0,g=parseInt(col.substr(3,2),16)||0,bl=parseInt(col.substr(5,2),16)||0;
  var luma=0.299*r+0.587*g+0.114*bl;
  b.style.borderColor=col;
  b.style.background=(i===L)?col:'#161a20';
  b.style.color=(i===L)?(luma>140?'#0b0e13':'#f0f6fc'):col;
  b.style.opacity=(lay&&!lay.enabled)?'.45':'1';
 }
}
function buildPads(){
 // One column per physical strip, side by side. Only layers that have actually
 // been added (l < layerCount) are drawn, so an untouched strip shows a single
 // pad block instead of four empty ones.
 var html='';
 for(var p=0;p<ST.strips.length;p++){
  var st=ST.strips[p];
  html+='<div class=padcol id=padcol'+p+'>'
   +'<div class=colhdr>Strip '+String.fromCharCode(65+p)
   +'<span id=colst'+p+'>'+st.layerCount+' layer'+(st.layerCount>1?'s':'')+'</span></div>';
  for(var l=0;l<st.layerCount;l++){
   var lay=st.layers[l],c='#'+(lay.color||'808080');
   html+='<div class=pgrp id=pgrp'+p+'_'+l+' style="--c:'+c+'">'
    +'<div class=pghdr><b><i class=pdot></i>Layer '+(l+1)+'</b>'
    +'<span id=pgst'+p+'_'+l+'>-</span></div>'
    +'<div class=pgbar><i id=pgb'+p+'_'+l+'></i></div><div class=pgrid>';
   PADEFFS.forEach(function(pe){
    html+='<button type=button class=pad id=pad_'+p+'_'+l+'_'+pe.e+'><span>'+pe.n+'</span><small>'+pe.d+'</small></button>';
   });
   html+='</div></div>';
  }
  html+='</div>';
 }
 $('padgroups').innerHTML=html;
 for(var p2=0;p2<ST.strips.length;p2++){
  for(var l2=0;l2<ST.strips[p2].layerCount;l2++){
   (function(pp,ll){
    PADEFFS.forEach(function(pe){
     var btn=$('pad_'+pp+'_'+ll+'_'+pe.e);
     btn.addEventListener('pointerdown',function(e){
      e.preventDefault();
      if(btn.setPointerCapture){try{btn.setPointerCapture(e.pointerId);}catch(_){}}
      padHit(pp,ll,pe.e,true);
     });
     btn.addEventListener('pointerup',function(){padHit(pp,ll,pe.e,false);});
     btn.addEventListener('pointercancel',function(){padHit(pp,ll,pe.e,false);});
     btn.addEventListener('lostpointercapture',function(){padHit(pp,ll,pe.e,false);});
     btn.addEventListener('contextmenu',function(e){e.preventDefault();});
    });
   })(p2,l2);
  }
 }
}
var PADEFFS=[
{e:1,n:'FLASH',d:'full strip &middot; hold'},
{e:0,n:'JET',d:'shoot &middot; hold streams'},
{e:2,n:'BURST',d:'centre shockwave'},
{e:3,n:'SPARKLE',d:'glitter &middot; hold'}
];
var TRIGSPEC=[
{k:'centerHz',n:'Band centre',min:65,max:7600,st:5,u:' Hz'},
{k:'widthHz',n:'Band width',min:40,max:16000,st:10,u:' Hz'}
];
var ANIMSPEC=[
{k:'blobLeds',n:'Blob / jet length',min:1,max:1000,st:1,u:' LEDs'},
{k:'speed',n:'Speed',min:1,max:500,st:1,u:' LEDs/s'},
{k:'fadeMs',n:'Fade time',min:50,max:10000,st:50,u:' ms'},
{k:'retriggerMs',n:'Retrigger gap',min:0,max:2000,st:10,u:' ms'}
];

function bindLayerRange(box,list){
 list.forEach(function(s){
  box.insertAdjacentHTML('beforeend',
   '<div class=row><span>'+s.n+'</span><b id="'+s.k+'v">-</b></div>'
   +'<input type=range id="'+s.k+'" min='+s.min+' max='+s.max+' step='+s.st+'>');
  var el=$(s.k);
  el.addEventListener('input',function(){labels();sendLayer(s.k,el.value);});
  el.addEventListener('change',function(){flush();});
 });
}
function bindPhysRange(box,list){
 list.forEach(function(s){
  box.insertAdjacentHTML('beforeend',
   '<div class=row><span>'+s.n+'</span><b id="'+s.k+'v">-</b></div>'
   +'<input type=range id="'+s.k+'" min='+s.min+' max='+s.max+' step='+s.st+'>');
  var el=$(s.k);
  el.addEventListener('input',function(){labels();sendPhys(s.k,el.value);});
  el.addEventListener('change',function(){flush();});
 });
}

function build(){
 /* ---- triggering conditions (per layer) ---- */
 var tbox=$('trigbox');
 tbox.innerHTML='<div class=ptitle id=ttitle>Trigger inputs<small id=thint>tick every source that may fire this layer</small></div>'
  +'<div class=chips id=trigchips>'
  +'<button type=button class=chip id=trig_thr>Threshold</button>'
  +'<button type=button class=chip id=trig_beat>Auto beat</button>'
  +'<button type=button class=chip id=trig_tap>Tap pads</button>'
  +'<button type=button class=chip id=trig_accel>Accelerometer</button>'
  +'<button type=button class=chip id=trig_gyro>Gyroscope</button>'
  +'<button type=button class=chip id=trig_lfo>LFO</button>'
  +'</div>'
  +'<div class=row><span id=mtlbl>Band level &amp; threshold</span><b id=mt0>0%</b></div>'
  +'<div class=bar id=bar0><i id=mb0></i><u id=mu0></u></div>'
  +'<div id=beatpanel style="display:none"><div class=row><span>Beat sensitivity</span><b id=beatSensitivityPctv>-</b></div>'
  +'<input type=range id=beatSensitivityPct min=0 max=100 step=1></div>'
  +'<div id=trig_lfopanel style="display:none"><div class=row><span>LFO period</span><b id=lfoPeriodMsv>-</b></div>'
  +'<input type=range id=lfoPeriodMs min=100 max=5000 step=50></div>'
  +'<div id=trig_motion style="display:none"><div class=row><span>Motion threshold</span><b id=motionThresholdPctv>-</b></div>'
  +'<input type=range id=motionThresholdPct min=0 max=100 step=1></div>'
  +'<div id=trig_freq></div>';
 var bar=$('bar0');
 bar.addEventListener('pointerdown',function(e){
  if(!(TRIG&TB.thr))return;
  e.preventDefault();TDRAG=S;
  if(bar.setPointerCapture){try{bar.setPointerCapture(e.pointerId);}catch(_){}}
  pickThr(e,false);
 });
 bar.addEventListener('pointermove',function(e){
  if(TDRAG<0||!(TRIG&TB.thr))return;
  e.preventDefault();pickThr(e,false);
 });
 bar.addEventListener('pointerup',function(e){if(TDRAG<0)return;TDRAG=-1;pickThr(e,true);});
 bar.addEventListener('pointercancel',function(){TDRAG=-1;flush();});
 bindLayerRange($('trig_freq'),TRIGSPEC);
 $('beatSensitivityPct').addEventListener('input',function(){
  labels();sendLayer('beatSensitivityPct',$('beatSensitivityPct').value);
 });
 $('beatSensitivityPct').addEventListener('change',flush);
 // The chips are static markup: generating them from JS meant that any earlier
 // error left the row empty, which looked exactly like a missing trigger.
 var CHIPS=[['thr','Threshold'],['beat','Auto beat'],['tap','Tap pads'],['accel','Accelerometer'],['gyro','Gyroscope'],['lfo','LFO']];
 CHIPS.forEach(function(c){
  $('trig_'+c[0]).onclick=function(){
   TRIG=(TRIG^TB[c[0]]);
   setTriggers();refreshTitle();
   sendLayer('triggers',TRIG);flush();
  };
 });
 ['motionThresholdPct','lfoPeriodMs'].forEach(function(k){
  $(k).addEventListener('input',function(){labels();sendLayer(k,$(k).value);});
  $(k).addEventListener('change',flush);
 });

 /* ---- display: colour + softness (layer) ---- */
 var dbox=$('dispbox');
 dbox.innerHTML='<div class=ptitle id=dtitle>Display<small>colour &amp; softness of this layer</small></div>'
  +'<div class=row><span>Layer colour</span><b id=colv>-</b></div>'
  +'<div class=cwrap><canvas id=wheel width=152 height=152></canvas><div class=cbox id=cbox></div></div>'
  +'<div id=spanel></div>';
 initWheel();
 var wh=$('wheel');
 wh.addEventListener('pointerdown',function(e){
  e.preventDefault();WDRAG=true;
  if(wh.setPointerCapture){try{wh.setPointerCapture(e.pointerId);}catch(_){}}
  pickWheel(e,false);
 });
 wh.addEventListener('pointermove',function(e){
  if(!WDRAG)return;
  e.preventDefault();pickWheel(e,false);
 });
 wh.addEventListener('pointerup',function(e){if(!WDRAG)return;WDRAG=false;pickWheel(e,true);});
 wh.addEventListener('pointercancel',function(){WDRAG=false;flush();});
 bindLayerRange($('spanel'),[
  {k:'softnessPct',n:'Softness (blur of the edges)',min:0,max:100,st:1,u:'%'}
 ]);
 // The layer's weight sits with its power switch, above the cards: the two are
 // the layer's on/off and how strongly it contributes, nothing else.
 bindLayerRange($('ipanel'),[
  {k:'intensityPct',n:'Layer intensity',min:1,max:100,st:1,u:'%'}
 ]);
 // Strip-level controls live in the strip card, directly under the strip tabs.
 ['brightnessPct'].forEach(function(k){
  $(k).addEventListener('input',function(){labels();sendPhys(k,$(k).value);});
  $(k).addEventListener('change',flush);
 });

 /* ---- animation (per layer) ---- */
 var abox=$('animbox');
 abox.innerHTML='<div class=ptitle id=atitle>Effect &amp; motion<small>what this layer draws and how it moves</small></div>'
  +'<div class=chips><button type=button class="chip" id=ef0>Jet</button>'
  +'<button type=button class="chip" id=ef1>Flash</button>'
  +'<button type=button class="chip" id=ef2>Burst</button>'
  +'<button type=button class="chip" id=ef3>Sparkle</button>'
  +'<button type=button class="chip" id=ef4>Whip</button></div>'
  +'<div class=row><span>Topology</span><b></b></div>'
  +'<div class=seg><button id=lp1>Loop</button><button id=lp0>One-shot</button></div>'
  +'<div class=row><span>Direction</span><b></b></div>'
  +'<div class=seg tight><button id=d0>Forward</button><button id=d1>Reverse</button><button id=d2>Ping-pong</button></div>';
 $('ef0').onclick=function(){EFF=0;setEff();refreshTitle();sendLayer('effect',0);flush();};
 $('ef1').onclick=function(){EFF=1;setEff();refreshTitle();sendLayer('effect',1);flush();};
 $('ef2').onclick=function(){EFF=2;setEff();refreshTitle();sendLayer('effect',2);flush();};
 $('ef3').onclick=function(){EFF=3;setEff();refreshTitle();sendLayer('effect',3);flush();};
 $('ef4').onclick=function(){EFF=4;setEff();refreshTitle();sendLayer('effect',4);flush();};
 $('lp1').onclick=function(){LOOP=1;setLoop();refreshTitle();sendLayer('loop',1);flush();};
 $('lp0').onclick=function(){LOOP=0;setLoop();refreshTitle();sendLayer('loop',0);flush();};
 $('d0').onclick=function(){REV=0;setDir();refreshTitle();sendLayer('reverse',0);flush();};
 $('d1').onclick=function(){REV=1;setDir();refreshTitle();sendLayer('reverse',1);flush();};
 $('d2').onclick=function(){REV=2;setDir();refreshTitle();sendLayer('reverse',2);flush();};
 abox.insertAdjacentHTML('beforeend',
  '<div id=flashgroup style="display:none">'
  +'<div class=row><span>Attack</span><b id=attackMsv>-</b></div>'
  +'<input type=range id=attackMs min=0 max=2000 step=10>'
  +'<div class=row><span>Release</span><b id=releaseMsv>-</b></div>'
  +'<input type=range id=releaseMs min=10 max=5000 step=10></div>',);
 ['attackMs','releaseMs'].forEach(function(k){
  $(k).addEventListener('input',function(){labels();sendLayer(k,$(k).value);});
  $(k).addEventListener('change',flush);
 });
 ANIMSPEC.forEach(function(s){
  if(s.k==='retriggerMs'){
   abox.insertAdjacentHTML('beforeend',
    '<div class=row><span>'+s.n+'</span><b id="'+s.k+'v">-</b></div>'
    +'<div class=rline><input type=range id="'+s.k+'" min='+s.min+' max='+s.max+' step='+s.st+'>'
    +'<button type=button id=rtbtn class=tbtn>Retrig ON</button></div>');
  }else{
   abox.insertAdjacentHTML('beforeend',
    '<div class=row><span>'+s.n+'</span><b id="'+s.k+'v">-</b></div>'
    +'<input type=range id="'+s.k+'" min='+s.min+' max='+s.max+' step='+s.st+'>');
  }
  var el=$(s.k);
  el.addEventListener('input',function(){labels();sendLayer(s.k,el.value);});
  el.addEventListener('change',flush);
 });
 $('rtbtn').onclick=function(){
  RETRIG=RETRIG?0:1;setRetrig();labels();sendLayer('retrigger',RETRIG);flush();
 };

 /* ---- calibration (physical) ---- */
 var cbox=$('calbox');
 cbox.innerHTML='<div class=ptitle id=ctitle>Strip calibration<small id=calhint>lights only the end LED while adjusting</small></div>'
  +'<div class=row><span>LEDs on this physical strip</span><b id="ledCountv">-</b></div>'
  +'<input type=range id="ledCount" min=1 max=1000 step=1>'
  +'<div class=rline style="margin-top:6px">'
  +'<button type=button id=lcdec10 class=stepbtn>-10</button>'
  +'<button type=button id=lcdec class=stepbtn>&larr;</button>'
  +'<button type=button id=lcinc class=stepbtn>&rarr;</button>'
  +'<button type=button id=lcinc10 class=stepbtn>+10</button>'
  +'<button type=button id=calbtn class=tbtn style="margin-left:auto">Calibrate</button>'
  +'</div>';
 $('ledCount').addEventListener('input',function(){labels();sendPhys('ledCount',$('ledCount').value);});
 $('ledCount').addEventListener('change',flush);
 function stepLeds(delta){
  var el=$('ledCount'),v=Math.max(1,Math.min(1000,(+el.value)+delta));
  el.value=v;labels();sendPhys('ledCount',v);flush();
 }
 $('lcdec10').onclick=function(){stepLeds(-10);};
 $('lcdec').onclick=function(){stepLeds(-1);};
 $('lcinc').onclick=function(){stepLeds(1);};
 $('lcinc10').onclick=function(){stepLeds(10);};
 $('calbtn').onclick=function(){CALIB=CALIB?0:1;setCalib();sendPhys('calib',CALIB);flush();};

 /* ---- strip-level controls ---- */
 $('pwr').addEventListener('change',function(){
  var c=cur();if(c)c.enabled=$('pwr').checked?1:0;
  sendLayer('enabled',$('pwr').checked?1:0);flush();refreshTitle();
 });
 $('addlayer').onclick=function(){
  var n=layerCount();
  if(n>=4){flashMsg('Maximum 4 layers');return;}
  // The new layer is a copy of the one you are looking at, in another colour
  // and at 60% weight, so it shows up as a second beam straight away.
  sendPhys('layerAdd',L);
  syncState(function(){L=layerCount()-1;fillInputs();live();});
 };
 $('remlayer').onclick=function(){
  var n=layerCount();
  if(n<=1){flashMsg('One layer must stay (power it off instead)');return;}
  sendPhys('layerRemove',L);
  syncState(function(){if(L>layerCount()-1)L=layerCount()-1;fillInputs();live();});
 };
 $('solobtn').onclick=function(){
  SOLO=(SOLO===L)?-1:L;
  sendPhys('solo',SOLO);flush();setSolo();refreshTitle();
 };
 $('openpads').onclick=function(){$('padscreen').classList.add('open');};
 $('closepads').onclick=function(){$('padscreen').classList.remove('open');};
}

function dirName(r){return r===2?'ping-pong':(r===1?'reverse':'forward');}
function modeName(m){return m===2?'manual':(m===1?'auto beat':'threshold');}
function effName(e){return e===4?'whip':(e===3?'sparkle':(e===2?'burst':(e===1?'flash':'jet')))};

function refreshTitle(){
 var c=cur(),p=phys();
 if(!c||!p)return;
 var col=colOf(c);
 ['ptitle','ttitle','dtitle','atitle','ctitle'].forEach(function(id){if($(id))$(id).style.borderLeftColor=col;});
 $('colv').textContent=col;
 var tl=[];
 if(TRIG&TB.thr)tl.push(c.thresholdPct+'% thr');
 if(TRIG&TB.beat)tl.push('beat '+$('beatSensitivityPct').value+'%');
 if(TRIG&TB.tap)tl.push('tap');
 if(TRIG&TB.accel)tl.push('accel');
 if(TRIG&TB.gyro)tl.push('gyro');
 if(TRIG&TB.lfo)tl.push('lfo');
 var trig=tl.length?tl.join('+'):'no trigger';
 $('ptitle').innerHTML=nameOf(S)+' &middot; Layer '+(L+1)+'<small>'
  +(c.enabled?'on':'off')+' &middot; '+trig+' &middot; '+effName(EFF)
  +' &middot; '+c.intensityPct+'% weight &middot; '+$('speed').value+' LED/s</small>';
 // lcv is written by buildLayerTabs(), so the count and the tabs never disagree.
 $('tabhint').textContent=p.ledCount+' LEDs · '+p.brightnessPct+'% output';
 paintLayerTabs();}

function labels(){
 [TRIGSPEC,ANIMSPEC].forEach(function(list){
  list.forEach(function(s){
   if(!$(s.k+'v'))return;
   if(s.k==='centerHz'||s.k==='widthHz')return;
   if(s.k==='retriggerMs')return;
   $(s.k+'v').textContent=$(s.k).value+s.u;
  });
 });
 if($('retriggerMsv'))$('retriggerMsv').textContent=RETRIG?($('retriggerMs').value+' ms'):'single only';
 if($('centerHzv')){
  var cc=+$('centerHz').value,w=+$('widthHz').value;
  $('centerHzv').textContent=cc+' Hz';
  $('widthHzv').textContent=w+' Hz  ('+Math.max(65,Math.round(cc-w/2))+'-'+Math.min(7600,Math.round(cc+w/2))+')';
 }
 if($('beatSensitivityPctv'))$('beatSensitivityPctv').textContent=$('beatSensitivityPct').value+'%';
 if($('motionThresholdPctv'))$('motionThresholdPctv').textContent=$('motionThresholdPct').value+'%';
 if($('intensityPctv'))$('intensityPctv').textContent=$('intensityPct').value+'%';
 if($('lfoPeriodMsv'))$('lfoPeriodMsv').textContent=$('lfoPeriodMs').value+' ms';
 if($('attackMsv'))$('attackMsv').textContent=$('attackMs').value+' ms';
 if($('releaseMsv'))$('releaseMsv').textContent=$('releaseMs').value+' ms';
 if($('brightnessPctv'))$('brightnessPctv').textContent=$('brightnessPct').value+'%';
 if($('softnessPctv'))$('softnessPctv').textContent=$('softnessPct').value+'%';
 if($('brightnessPctv'))$('brightnessPctv').textContent=$('brightnessPct').value+'%';
 if($('ledCountv'))$('ledCountv').textContent=$('ledCount').value;
 refreshTitle();
}

function setTriggers(){
 ['thr','beat','tap','accel','gyro','lfo'].forEach(function(k){
  var b=$('trig_'+k);
  if(!b)return;
  var on=(TRIG&TB[k])?1:0;
  b.className='chip'+(on?' on':'');
  b.textContent=(on?'\u2713 ':'')+({thr:'Threshold',beat:'Auto beat',tap:'Tap pads',accel:'Accelerometer',gyro:'Gyroscope',lfo:'LFO'})[k];
 });
 var hasThr=(TRIG&TB.thr)?1:0, hasBeat=(TRIG&TB.beat)?1:0;
 var hasMotion=(TRIG&(TB.accel|TB.gyro))?1:0;
 $('bar0').style.display=hasThr?'':'none';
 if($('mtlbl'))$('mtlbl').style.display=hasThr?'':'none';
 if($('mt0'))$('mt0').style.display=hasThr?'':'none';
 $('mu0').style.display=hasThr?'':'none';
 $('bar0').style.cursor=hasThr?'ew-resize':'default';
 $('beatpanel').style.display=hasBeat?'':'none';
 $('trig_motion').style.display=hasMotion?'':'none';
 $('trig_lfopanel').style.display=(TRIG&TB.lfo)?'':'none';
 $('trig_freq').style.display=(hasThr||hasBeat)?'':'none';
 if($('thint')){
  var n=0;
  for(var k in TB)if(TRIG&TB[k])n++;
  if(n){
   $('thint').textContent=n+' source'+(n>1?'s':'')+' armed — fires on whichever happens first'
 
  }else{
   $('thint').textContent='nothing armed: this layer is silent until you tick a source';
  }
 }
}
function setEff(){
 for(var i=0;i<5;i++)if($('ef'+i))$('ef'+i).className='chip'+(EFF===i?' on':'');
 // A flash's attack/release are what turn it into a strobe or into a glow.
 if($('flashgroup'))$('flashgroup').style.display=EFF===1?'':'none';
}
function setDir(){$('d0').className=REV===0?'sel':'';$('d1').className=REV===1?'sel':'';$('d2').className=REV===2?'sel':'';}
function setLoop(){$('lp1').className=LOOP?'sel':'';$('lp0').className=LOOP?'':'sel';}
function setRetrig(){
 var b=$('rtbtn'),sl=$('retriggerMs');
 if(!b)return;
 b.className='tbtn '+(RETRIG?'on':'off');
 b.textContent=RETRIG?'Retrig ON':'Single';
 sl.disabled=!RETRIG;sl.style.opacity=RETRIG?'1':'.35';
}
function setCalib(){
 var b=$('calbtn'),h=$('calhint');
 if(!b)return;
 b.className='tbtn '+(CALIB?'on':'');
 b.textContent=CALIB?'Exit calib':'Calibrate';
 if(h)h.textContent=CALIB?'CALIBRATING: only the end LED is lit':'lights only the end LED while adjusting';
}
function setSolo(){
 var b=$('solobtn');
 if(!b)return;
 b.className='tbtn '+(SOLO===L?'solo':'');
 b.textContent=SOLO===L?'Solo ON':'Solo';
}
function setLayerSel(){
 var n=layerCount();
 for(var i=0;i<4;i++){
  var b=$('lt'+i);
  if(b)b.className=(i===L?'sel ':'')+(i<n?'':'');
 }
}

function fillInputs(){
 if(!ST||!BUILT)return;
 var c=cur(),p=phys();
 if(!c||!p)return;
 [TRIGSPEC,ANIMSPEC].forEach(function(list){
  list.forEach(function(s){if($(s.k)&&c[s.k]!==undefined)$(s.k).value=c[s.k];});
 });
 $('beatSensitivityPct').value=c.beatSensitivityPct;
 $('motionThresholdPct').value=c.motionThresholdPct;
 $('lfoPeriodMs').value=c.lfoPeriodMs;
 $('attackMs').value=c.attackMs;
 $('releaseMs').value=c.releaseMs;
 $('intensityPct').value=c.intensityPct;
 $('softnessPct').value=c.softnessPct;
 $('brightnessPct').value=p.brightnessPct;

 $('ledCount').value=p.ledCount;
 $('pwr').checked=!!c.enabled;
 $('gainDb').value=ST.gainDb;
 AGC=ST.agc?1:0;
 setAgc();
 TRIG=c.triggers||0;EFF=c.effect||0;REV=c.reverse||0;LOOP=c.loop?1:0;
 RETRIG=c.retrigger!==undefined?(c.retrigger?1:0):1;
 CALIB=p.calib?1:0;
 SOLO=(p.solo===undefined)?-1:p.solo;
 buildLayerTabs();
 setLayerSel();setTriggers();setEff();setDir();setLoop();setRetrig();setCalib();setSolo();
 setColorHex(colOf(c));
 labels();
}

function flashMsg(t){
 $('pmsg').textContent=t;
 if(MSGTMR)clearTimeout(MSGTMR);
 MSGTMR=setTimeout(function(){MSGTMR=0;if(ST)$('pmsg').textContent=ST.unsaved?'auto-saving…':'auto-saved';},2200);
}

function live(){
 if(!ST)return;
 for(var i=0;i<ST.strips.length;i++){
  var st=ST.strips[i],anyOn=false;
  for(var lv=0;lv<st.layerCount;lv++)if(st.layers[lv].enabled)anyOn=true;
  $('sel'+i).className=(S===i?'sel ':'')+(anyOn?'on':'off');
 }
 setLayerSel();
 var c=cur(),p=phys();
 if(c){
  var col=colOf(c);
  $('mb0').style.background=col;
  var lvl=(TRIG&TB.beat)?Math.max(c.beatPct||0,c.levelPct||0):c.levelPct;
  $('mb0').style.width=lvl+'%';
  $('mu0').style.left='calc('+c.thresholdPct+'% - 2px)';
  $('mt0').textContent='lvl '+c.levelPct+'%'
   +((TRIG&TB.beat)?(' · '+(c.bpm>0?c.bpm+' BPM':'listening')+' · beat '+(c.beatPct||0)+'%'):'')
   +' · '+c.blobs+' active';
  CALIB=p.calib?1:0;setCalib();setSolo();
 }
 for(var q=0;q<ST.strips.length;q++){
  var sq=ST.strips[q];
  for(var lq=0;lq<sq.layerCount;lq++){
   var ly=sq.layers[lq],g=$('pgrp'+q+'_'+lq);
   if(!g)continue;
   g.style.setProperty('--c','#'+(ly.color||'808080'));
   var stx=layModeName(ly)+' · out '+(ly.brightPct||0)+'%';
   $('pgst'+q+'_'+lq).textContent=stx;
   $('pgb'+q+'_'+lq).style.width=(ly.brightPct||0)+'%';
  }
 }
 if(ST.ssid)$('host').textContent=ST.ssid+' · '+location.host+(ST.ver?(' · '+ST.ver):'');
 $('sLvl').textContent=ST.levelPct+'%';
 $('sDb').textContent=ST.dbfs;
 $('sPk').textContent=ST.peakHz+' Hz';
 if(!MSGTMR)$('pmsg').textContent=ST.unsaved?'auto-saving…':'auto-saved';
 $('gainDbv').textContent=(ST.gainDb>0?'+':'')+ST.gainDb+' dB';
 $('ld1').disabled=!ST.hasPreset0;
 $('ld2').disabled=!ST.hasPreset1;
 $('ld3').disabled=!ST.hasPreset2;
}
function layModeName(l){
 if(!l)return '-';
 if(!l.enabled)return 'off';
 var t=l.triggers||0;
 if(t&TB.beat)return l.bpm>0?(l.bpm+' BPM'):'beat';
 if(t&(TB.accel|TB.gyro))return 'motion';
 if(t&TB.lfo)return 'lfo';
 if(t&TB.thr)return 'thr '+l.thresholdPct+'%';
 return 'tap';
}

var PAD_SIG='';
function padSig(s){
 var t='';
 for(var i=0;i<s.strips.length;i++){
  t+=s.strips[i].layerCount+':';
  for(var l=0;l<s.strips[i].layers.length;l++)t+=s.strips[i].layers[l].color+',';
 }
 return t;
}
/* Layer counts only: the thing that decides how many tabs must exist. */
function structSig(s){
 var t='';
 for(var i=0;i<s.strips.length;i++)t+=s.strips[i].layerCount+',';
 return t;
}
var STRUCT='';
function apply(s){
 ST=s;
 if(!BUILT&&ST.strips.length){
  BUILT=true;
  buildSelectors(ST.strips.length);
  build();
 }
 var sig=padSig(s);
 if(sig!==PAD_SIG){PAD_SIG=sig;buildPads();}
 // The board can change the layer structure without this page asking: a preset
 // load, a factory reset, or the other phone. Rebuilding only from the add and
 // remove buttons left the tabs showing layers that no longer existed (and hid
 // layers that did), so the structure is derived from the state itself.
 var restructured=structSig(s)!==STRUCT;
 if(restructured)STRUCT=structSig(s);
 if(!SEEDED){SEEDED=true;fillInputs();}
 else if(restructured){
  if(L>layerCount()-1)L=layerCount()-1;
  fillInputs();
 }
 else if(!editing()&&!LAST&&Date.now()-USER_TS>900){
  // Nothing is being touched, so make the controls say exactly what the board
  // holds. Whatever drifted - a dropped POST, the other phone, AGC nudging the
  // gain, the board's own buttons - is corrected within one poll instead of
  // staying on screen and being sent back on the next edit.
  fillInputs();
 }
 live();
}
function post(u,refresh){
 // Goes through the same single-socket rule as everything else: a preset POST
 // racing the poll is what used to make one of them hang.
 INFLIGHT=true;
 var opt=abortAfter(4000);
 opt.method='POST';opt.cache='no-store';
 return fetch(u,opt).then(function(r){INFLIGHT=false;return r.json();}).then(function(s){
  apply(s);if(refresh)fillInputs();return s;
 },function(){INFLIGHT=false;flashMsg('request failed');});
}
$('host').textContent=location.host;
function setAgc(){
 var b=$('agcbtn'),sl=$('gainDb'),h=$('agchint');
 if(!b)return;
 b.className='tbtn'+(AGC?' on':'');
 b.textContent=AGC?'AGC ON':'AGC';
 sl.disabled=!!AGC;
 sl.style.opacity=AGC?'0.35':'1';
 if(h)h.textContent=AGC
  ?'automatic: the firmware nudges the gain to keep the level in range (move the slider to take over)'
  :'negative attenuates: use it when the room is too loud';
}
$('agcbtn').onclick=function(){
 AGC=AGC?0:1;
 setAgc();
 sendRaw('agc='+AGC);
 flush();
};
$('gainDb').addEventListener('input',function(){
 var v=+$('gainDb').value;
 $('gainDbv').textContent=(v>0?'+':'')+v+' dB';
});
$('gainDb').addEventListener('change',function(){sendRaw('gainDb='+$('gainDb').value);flush();});
[1,2,3].forEach(function(n){
 $('sp'+n).onclick=function(){flush();post('/api/preset?slot='+n+'&action=save',false).then(function(){flashMsg('Preset '+n+' saved');});};
 $('ld'+n).onclick=function(){flush();post('/api/preset?slot='+n+'&action=load',true).then(function(){flashMsg('Preset '+n+' loaded');});};
});
$('rst').onclick=function(){
 if(confirm('Reset current parameters to factory defaults? (Saved presets are kept.)')){
  post('/api/reset',true).then(function(){flashMsg('Factory defaults restored');});
 }
};
function poll(){
 if(busySocket()||editing()||LAST)return;
 // A poll that lands just after a knob moved would overwrite what the user is
 // setting with a value the board has not seen yet.
 if(Date.now()-USER_TS<900)return;
 POLL_BUSY=true;
 fetchState()
  .then(function(s){POLL_BUSY=false;if(!editing()&&!LAST)apply(s);})
  .catch(function(){POLL_BUSY=false;flashMsg('sync lost - reload the page');});
}
fetch('/api/state',{cache:'no-store'}).then(function(r){return r.json();}).then(apply).catch(function(){});
setInterval(poll,600);
</script>
</body>
</html>)PAGE";

void appendf(char *buf, size_t cap, size_t &n, const char *fmt, ...) {
  if (n + 1 >= cap) {
    return;
  }
  va_list ap;
  va_start(ap, fmt);
  const int written = vsnprintf(buf + n, cap - n, fmt, ap);
  va_end(ap);
  if (written > 0) {
    n += (size_t)written;
    if (n > cap - 1) {
      n = cap - 1;
    }
  }
}

} // namespace

void WebUi::setup(Settings *settings, SettingsStore *store) {
  settings_ = settings;
  store_ = store;
}

bool WebUi::begin() { return setEnabled(true); }

bool WebUi::setEnabled(bool on) {
  if (on == enabled_) {
    return true;
  }

  if (!on) {
    server_.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    enabled_ = false;
    Serial.println("[web] access point off");
    return true;
  }

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(kSsid, kPassword)) {
    Serial.println("[web] softAP failed");
    return false;
  }
  WiFi.setSleep(false); // prevent ESP32-S3 modem-sleep stalls while serving AP

  bindRoutes();
  server_.enableDelay(false);
  server_.begin();
  enabled_ = true;
  Serial.printf("[web] \"%s\" / \"%s\" -> http://%s\n", kSsid, kPassword,
                ip().c_str());
  return true;
}

void WebUi::bindRoutes() {
  if (routesBound_) {
    return;
  }
  routesBound_ = true;

  server_.on("/", [this] { handleRoot(); });
  server_.on("/favicon.ico", [this] { server_.send(204, "text/plain", ""); });
  server_.on("/api/state", [this] { handleState(); });
  server_.on("/api/params", HTTP_POST, [this] { handleParams(); });
  server_.on("/api/tap", HTTP_POST, [this] { handleTap(); });
  server_.on("/api/preset", HTTP_POST, [this] { handlePreset(); });
  server_.on("/api/reset", HTTP_POST, [this] { handleReset(); });
  server_.onNotFound([this] { server_.send(204, "text/plain", ""); });
}

String WebUi::ip() const {
  return enabled_ ? WiFi.softAPIP().toString() : String("0.0.0.0");
}

String WebUi::stateJson() const {
  // Sized for the worst case (see kStateJsonMaxBytes). Static rather than on the
  // stack: 4 KB in a loop task with an 8 KB stack is not worth risking, and this
  // is only ever called from the single-threaded web server.
  static char buf[kStateJsonMaxBytes];
  size_t n = 0;
  buf[0] = '\0';
  const uint32_t nowMs = millis();

  appendf(buf, sizeof(buf), n,
          "{\"ssid\":\"%s\",\"ver\":\"%s\",\"gainDb\":%d,\"agc\":%u,"
          "\"strips\":[",
          kSsid, kFirmwareVersion, (int)settings_->gainDb,
          (unsigned)settings_->agc);

  for (size_t i = 0; i < kStripCount; i++) {
    const PhysicalStripSettings &p = settings_->phys[i];
    const PhysicalTelemetry &pt = tele_.strip[i];
    appendf(buf, sizeof(buf), n,
            "%s{\"ledCount\":%u,\"brightnessPct\":%u,\"layerCount\":%u,"
            "\"brightPct\":%u,\"solo\":%d,\"calib\":%u,\"layers\":[",
            i == 0 ? "" : ",", (unsigned)p.ledCount, (unsigned)p.brightnessPct,
            (unsigned)p.layerCount, (unsigned)pt.brightPct, (int)soloLayer(i),
            isCalibrating(i, nowMs) ? 1u : 0u);

    for (size_t l = 0; l < kMaxLayersPerStrip; l++) {
      const LayerSettings &s = settings_->layer[i][l];
      const LayerTelemetry &lt = pt.layer[l];
      appendf(buf, sizeof(buf), n,
              "%s{\"triggers\":%u,\"effect\":%u,\"enabled\":%u,"
              "\"intensityPct\":%u,\"softnessPct\":%u,\"centerHz\":%u,"
              "\"widthHz\":%u,"
              "\"thresholdPct\":%u,\"beatSensitivityPct\":%u,"
              "\"motionThresholdPct\":%u,\"lfoPeriodMs\":%u,\"attackMs\":%u,"
              "\"releaseMs\":%u,\"retrigger\":%u,\"retriggerMs\":%u,"
              "\"blobLeds\":%u,\"speed\":%u,\"fadeMs\":%u,\"reverse\":%u,"
              "\"loop\":%u,\"color\":\"%02x%02x%02x\",\"levelPct\":%u,"
              "\"brightPct\":%u,\"blobs\":%u,\"beatPct\":%u,\"bpm\":%u}",
              l == 0 ? "" : ",", (unsigned)s.triggers, (unsigned)s.effect,
              (unsigned)s.enabled, (unsigned)s.intensityPct,
              (unsigned)s.centerHz, (unsigned)s.widthHz,
              (unsigned)s.thresholdPct, (unsigned)s.beatSensitivityPct,
              (unsigned)s.motionThresholdPct, (unsigned)s.lfoPeriodMs,
              (unsigned)s.attackMs, (unsigned)s.releaseMs, (unsigned)s.retrigger,
              (unsigned)s.retriggerMs,
              (unsigned)s.blobLeds, (unsigned)s.speed, (unsigned)s.fadeMs,
              (unsigned)s.reverse, (unsigned)s.loop, (unsigned)s.r,
              (unsigned)s.g, (unsigned)s.b, (unsigned)lt.levelPct,
              (unsigned)lt.brightPct, (unsigned)lt.blobs,
              (unsigned)lt.beatPct, (unsigned)lt.bpm);
    }
    appendf(buf, sizeof(buf), n, "]}");
  }

  appendf(buf, sizeof(buf), n,
          "],\"levelPct\":%u,\"dbfs\":%d,\"peakHz\":%u,\"unsaved\":%u,"
          "\"hasPreset0\":%u,\"hasPreset1\":%u,\"hasPreset2\":%u}",
          (unsigned)tele_.levelPct, (int)tele_.dbfs, (unsigned)tele_.peakHz,
          unsaved_ ? 1u : 0u, store_ && store_->hasPreset(0) ? 1u : 0u,
          store_ && store_->hasPreset(1) ? 1u : 0u,
          store_ && store_->hasPreset(2) ? 1u : 0u);

  return String(buf);
}

void WebUi::handleRoot() {
  server_.sendHeader("Cache-Control", "no-store");
  server_.send_P(200, "text/html", kPage);
}

void WebUi::handleState() {
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(200, "application/json", stateJson());
}

bool WebUi::readIndex(const char *name, size_t limit, size_t &out) {
  if (!server_.hasArg(name)) {
    return false;
  }
  const int v = server_.arg(name).toInt();
  if (v < 0 || v >= (int)limit) {
    return false;
  }
  out = (size_t)v;
  return true;
}

void WebUi::applyArg(const char *name, uint8_t *dst) {
  if (!server_.hasArg(name)) {
    return;
  }
  *dst = (uint8_t)constrain(server_.arg(name).toInt(), 0L, 255L);
}

void WebUi::applyArg(const char *name, int8_t *dst) {
  if (!server_.hasArg(name)) {
    return;
  }
  *dst = (int8_t)constrain(server_.arg(name).toInt(), (long)kMinGainDb,
                           (long)kMaxGainDb);
}

void WebUi::applyArg(const char *name, uint16_t *dst) {
  if (!server_.hasArg(name)) {
    return;
  }
  *dst = (uint16_t)constrain(server_.arg(name).toInt(), 0L, 65535L);
}

void WebUi::addLayer(size_t p, uint8_t from) {
  PhysicalStripSettings &ps = settings_->phys[p];
  if (ps.layerCount >= kMaxLayersPerStrip) {
    return;
  }
  const uint8_t srcIndex = from < ps.layerCount ? from : 0;
  LayerSettings &dst = settings_->layer[p][ps.layerCount];
  dst = settings_->layer[p][srcIndex];
  // A copy in another colour at a lower weight reads as a second beam rather
  // than as a doubling of the first one.
  const uint8_t r = dst.r;
  dst.r = dst.b;
  dst.b = dst.g;
  dst.g = r;
  dst.intensityPct = (uint8_t)((dst.intensityPct * 6u) / 10u);
  edit_[p] = ps.layerCount;
  ps.layerCount++;
  clampSettings(*settings_);
}

void WebUi::removeLayer(size_t p, uint8_t index) {
  PhysicalStripSettings &ps = settings_->phys[p];
  if (ps.layerCount <= 1) {
    return;
  }
  const uint8_t last = (uint8_t)(ps.layerCount - 1);
  if (index > last) {
    index = last;
  }
  for (uint8_t i = index; i < last; i++) {
    settings_->layer[p][i] = settings_->layer[p][i + 1];
  }
  LayerSettings &freed = settings_->layer[p][last];
  freed = LayerSettings();
  clampLayer(freed);
  ps.layerCount = last;
  // Solo and the pad's edit target are indices, so they have to follow the shift.
  if (solo_[p] == (int8_t)index) {
    solo_[p] = -1;
  } else if (solo_[p] > (int8_t)index) {
    solo_[p]--;
  }
  if (edit_[p] > (uint8_t)(last - 1)) {
    edit_[p] = (uint8_t)(last - 1);
  }
}

void WebUi::handleParams() {
  size_t p = 0;
  if (readIndex("strip", kStripCount, p)) {
    // Runtime-only: solo is a preview tool, so it is not persisted.
    if (server_.hasArg("solo")) {
      const int v = server_.arg("solo").toInt();
      solo_[p] =
          (v < 0 || v >= (int)kMaxLayersPerStrip) ? (int8_t)-1 : (int8_t)v;
    }

    // The physical container.
    PhysicalStripSettings &ps = settings_->phys[p];
    applyArg("ledCount", &ps.ledCount);
    applyArg("brightnessPct", &ps.brightnessPct);
    applyArg("layerCount", &ps.layerCount);

    // Structure edits are explicit requests rather than arithmetic on the
    // count, so the page and the board cannot disagree about which slot is
    // being created or destroyed.
    if (server_.hasArg("layerAdd")) {
      const long from = constrain(server_.arg("layerAdd").toInt(), 0L,
                                  (long)kMaxLayersPerStrip - 1);
      addLayer(p, (uint8_t)from);
    }
    if (server_.hasArg("layerRemove")) {
      const long idx = constrain(server_.arg("layerRemove").toInt(), 0L,
                                 (long)kMaxLayersPerStrip - 1);
      removeLayer(p, (uint8_t)idx);
    }

    // Changing ledCount auto-shows the calibration dot for 3.5 s so the tip of
    // the strip is visible; touching anything else leaves calibration mode.
    if (server_.hasArg("ledCount")) {
      calibUntilMs_[p] = millis() + 3500;
    } else if (!server_.hasArg("calib")) {
      calibPinned_[p] = false;
      calibUntilMs_[p] = 0;
    }
    if (server_.hasArg("calib")) {
      const bool on = server_.arg("calib").toInt() != 0;
      calibPinned_[p] = on;
      if (!on) {
        calibUntilMs_[p] = 0;
      }
    }

    // One layer of that container.
    size_t l = 0;
    if (readIndex("layer", kMaxLayersPerStrip, l)) {
      LayerSettings &s = settings_->layer[p][l];
      edit_[p] = (uint8_t)l;
      applyArg("triggers", &s.triggers);
      applyArg("effect", &s.effect);
      applyArg("motionThresholdPct", &s.motionThresholdPct);
      applyArg("lfoPeriodMs", &s.lfoPeriodMs);
      applyArg("attackMs", &s.attackMs);
      applyArg("releaseMs", &s.releaseMs);
      applyArg("enabled", &s.enabled);
      applyArg("intensityPct", &s.intensityPct);
      applyArg("softnessPct", &s.softnessPct);
      applyArg("centerHz", &s.centerHz);
      applyArg("widthHz", &s.widthHz);
      applyArg("thresholdPct", &s.thresholdPct);
      applyArg("beatSensitivityPct", &s.beatSensitivityPct);
      applyArg("retrigger", &s.retrigger);
      applyArg("retriggerMs", &s.retriggerMs);
      applyArg("blobLeds", &s.blobLeds);
      applyArg("speed", &s.speed);
      applyArg("fadeMs", &s.fadeMs);
      applyArg("reverse", &s.reverse);
      applyArg("loop", &s.loop);

      if (server_.hasArg("color")) {
        String v = server_.arg("color");
        if (v.startsWith("#")) {
          v = v.substring(1);
        }
        const uint32_t rgb = (uint32_t)strtoul(v.c_str(), nullptr, 16);
        s.r = (uint8_t)((rgb >> 16) & 0xFF);
        s.g = (uint8_t)((rgb >> 8) & 0xFF);
        s.b = (uint8_t)(rgb & 0xFF);
      }
    }
  }

  // Touching the slider means "I want manual control", so it turns AGC off.
  if (server_.hasArg("gainDb") && !server_.hasArg("agc")) {
    settings_->agc = 0;
  }
  applyArg("agc", &settings_->agc);
  applyArg("gainDb", &settings_->gainDb);
  clampSettings(*settings_);
  changed_ = true;
  unsaved_ = true;

  // Lightweight 204 reply: avoids heap String allocation and a multi-KB JSON
  // response on every 50 ms slider / colour-wheel drag tick.
  server_.send(204, "text/plain", "");
}

void WebUi::handleTap() {
  size_t p = 0;
  size_t l = 0;
  if (readIndex("strip", kStripCount, p) &&
      readIndex("layer", kMaxLayersPerStrip, l)) {
    const uint8_t eff =
        clampU8((server_.hasArg("effect") ? server_.arg("effect").toInt() : 0),
                LayerSettings::kEffectJet, LayerSettings::kEffectSparkle);
    const bool down =
        !server_.hasArg("down") || server_.arg("down").toInt() != 0;

    calibPinned_[p] = false;
    calibUntilMs_[p] = 0;
    edit_[p] = (uint8_t)l;

    if (down) {
      // Only a fresh press re-fires the effect; the 900 ms keepalive requests
      // just refresh the hold.
      if (!padDown_[p][l] || holdEffect_[p][l] != eff) {
        tapPending_[p][l] = true;
        tapEffect_[p][l] = eff;
      }
      padDown_[p][l] = true;
      holdState_[p][l] = 1;
      holdEffect_[p][l] = eff;
    } else {
      padDown_[p][l] = false;
      holdState_[p][l] = 0;
      holdEffect_[p][l] = eff;
    }
  }
  server_.send(204, "text/plain", "");
}

void WebUi::handlePreset() {
  const int slot = server_.hasArg("slot") ? server_.arg("slot").toInt() : 0;
  const String action =
      server_.hasArg("action") ? server_.arg("action") : String();

  // The API is 1-based so the numbers match the buttons on the page.
  const uint8_t index =
      (slot >= 1 && slot <= SettingsStore::kPresetCount) ? (uint8_t)(slot - 1)
                                                         : 0xFF;

  bool ok = false;
  if (index != 0xFF && store_ != nullptr) {
    if (action == "save") {
      ok = store_->savePreset(index, *settings_);
    } else if (action == "load") {
      Settings loaded;
      if (store_->loadPreset(index, loaded)) {
        *settings_ = loaded;
        for (size_t i = 0; i < kStripCount; i++) {
          calibPinned_[i] = false;
          calibUntilMs_[i] = 0;
          solo_[i] = -1;
        }
        changed_ = true;
        unsaved_ = true;
        ok = true;
      }
    }
  }

  Serial.printf("[web] preset %d %s -> %s\n", slot, action.c_str(),
                ok ? "ok" : "failed");

  server_.sendHeader("Cache-Control", "no-store");
  server_.send(ok ? 200 : 400, "application/json", stateJson());
}

void WebUi::handleReset() {
  *settings_ = defaultSettings();
  for (size_t i = 0; i < kStripCount; i++) {
    calibPinned_[i] = false;
    calibUntilMs_[i] = 0;
    solo_[i] = -1;
  }
  // A reset that did not survive a reboot would be confusing, so this one is
  // written straight away.
  const bool ok = store_ != nullptr && store_->saveLive(*settings_);
  changed_ = true;
  unsaved_ = !ok;

  Serial.println("[web] factory reset");

  server_.sendHeader("Cache-Control", "no-store");
  server_.send(200, "application/json", stateJson());
}
