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
h1{margin:0 0 10px;height:30px;overflow:hidden;font-size:1.15rem;font-weight:600;display:flex;justify-content:space-between;align-items:center;gap:10px}
h1 span{font-size:.72rem;color:#8b949e;font-weight:400;display:flex;gap:8px;align-items:center;justify-content:flex-end;min-width:0;overflow:hidden}
#host{min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.loopms{display:none;font-style:normal;font-size:.62rem;font-weight:700;color:#ffd580;background:#3a2f12;border-radius:8px;padding:3px 7px;min-width:82px;text-align:center;white-space:nowrap;font-variant-numeric:tabular-nums}
/* ---- text that changes while you watch ----
   A sentence that wraps to two lines one second and one line the next lifts
   every control below it, so a button moves out from under the finger that was
   aiming at it. Anything whose content is rewritten at runtime is given a box
   that cannot change size, and long lines ellipsise instead of wrapping. */
.dyn{height:35px;overflow:hidden}                          /* exactly two .76rem lines */
.dyn1{height:18px;overflow:hidden;white-space:nowrap;text-overflow:ellipsis}
.dynb{height:21px;overflow:hidden;white-space:nowrap;text-overflow:ellipsis;font-variant-numeric:tabular-nums}
.linkdot{font-style:normal;font-size:.62rem;font-weight:700;color:#0f1115;background:#484f58;border-radius:8px;padding:2px 7px;letter-spacing:.03em}
.padopen{width:100%;padding:12px 14px;margin-bottom:10px;border-radius:12px;border:2px solid #58a6ff;background:linear-gradient(135deg,#16233a,#171b21);color:#f0f6fc;font-size:1rem;font-weight:700;display:flex;justify-content:space-between;align-items:center;cursor:pointer;touch-action:manipulation}
.padopen small{font-weight:500;font-size:.76rem;color:#8b949e}
.card{background:#171b21;border:1px solid #262c36;border-radius:12px;padding:12px 14px;margin-bottom:10px}
.row{display:flex;justify-content:space-between;align-items:baseline;margin:9px 0 2px;font-size:.9rem;color:#9da7b3}
.row.first{margin-top:0}
.row b{color:#e6edf3;font-variant-numeric:tabular-nums;font-weight:600}
.hint{color:#8b949e;font-size:.76rem;margin:2px 0 4px}
/* Band map: a log frequency axis with one stacked row per layer, the shape of
   the TFT display, plus the live spectrum behind it as a faint silhouette. */
#bandmap{width:100%;height:150px;display:block;border-radius:8px;background:#0f1319;touch-action:none}
.bmlbl{position:relative;height:14px;margin-top:2px;font-size:.62rem;color:#6e7681;font-variant-numeric:tabular-nums}
.bmlbl span{position:absolute;transform:translateX(-50%);white-space:nowrap}
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
/* Environment dB scale: quiet / good / hot, with a live needle. -55..-18 dBFS is
   the range the analyser maps to 0..100 %, so the zones are drawn from that. */
.dbscale{position:relative;height:22px;margin:6px 0 2px;border-radius:6px;overflow:hidden;background:#21262e;border:1px solid #30363d}
.dbscale span{position:absolute;top:0;bottom:0;display:block}
.dbscale .zq{left:0;width:40%;background:#1d2733}
.dbscale .zg{left:40%;width:40%;background:#17301e}
.dbscale .zh{left:80%;width:20%;background:#2b1d1d}
.dbscale u{position:absolute;top:-2px;bottom:-2px;width:4px;margin-left:-2px;background:#f0f6fc;box-shadow:0 0 6px rgba(0,0,0,.9);border-radius:2px}
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
/* Layer list. A tab row was doing two jobs at once - "which layer am I editing"
   and "which layers are on" - and the only way to read the second was to select
   each tab in turn. Now they are separate controls on one row: the switch is
   power, the highlighted row is what the cards below are editing, and the colour
   chip is the beam's colour. */
.layerlist{display:flex;flex-direction:column;gap:6px;margin-top:6px}
.lrow{display:flex;align-items:center;gap:9px;padding:8px 10px;border-radius:10px;border:2px solid #262c36;background:#14181f;color:#c9d1d9;font-size:.86rem;touch-action:manipulation}
.lrow .dot{width:14px;height:14px;border-radius:50%;flex:none;box-shadow:0 0 8px -2px currentColor;border:1px solid rgba(0,0,0,.5)}
.lrow .nm{font-weight:700;flex:none;min-width:52px}
.lrow .bd{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-size:.74rem;color:#8b949e;font-variant-numeric:tabular-nums}
.lrow .st{flex:none;font-size:.62rem;font-weight:700;letter-spacing:.04em;text-transform:uppercase;color:#3fb950}
.lrow.off .nm,.lrow.off .bd{color:#565d68}
.lrow.off .st{color:#6b7280}
.lrow.off .dot{opacity:.35;box-shadow:none}
/* Selection is an edge + arrow, never a colour fill: the dot keeps saying what
   colour the layer is, whatever row you happen to be editing. */
.lrow.sel{border-color:#58a6ff;background:#182231}
.lrow.sel:before{content:"\25B8";color:#58a6ff;margin-left:-4px;font-weight:700}
.lrow.sel .nm{color:#f0f6fc}
.lrow .pw{flex:none}
/* Preset slots: one chip per slot showing what it actually holds (the colours
   of its layers), so "is this slot used, and with what" needs no guessing. */
.slots{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-top:6px}
.slot{position:relative;padding:8px 6px 7px;border-radius:10px;border:2px solid #3a424d;background:#161a20;color:#8b949e;font-size:.82rem;font-weight:600;text-align:left;min-height:56px;touch-action:manipulation}
.slot b{display:block;color:#3a424d;font-size:.72rem;letter-spacing:.06em;text-transform:uppercase;font-weight:700}
.slot .dots{display:flex;gap:3px;margin-top:5px;height:12px}
.slot .dots i{width:12px;height:12px;border-radius:50%;display:block;border:1px solid rgba(0,0,0,.5)}
.slot .lay{display:block;font-size:.66rem;font-weight:500;color:#8b949e;margin-top:3px}
.slot.used{border-color:#30363d;color:#e6edf3}
.slot.sel{border-color:#58a6ff;box-shadow:0 0 0 3px rgba(88,166,255,.25)}
.slot.empty b{color:#484f58}
.slot.empty .lay{color:#484f58}
/* A dangerous action asks twice and looks different while it is asking. */
.btn.armed{background:#3a2411;border-color:#d29922;color:#ffd580}
.btn:disabled{opacity:.3;cursor:not-allowed}
#pmsg{color:#8b949e;font-size:.78rem;font-weight:400;max-width:60%;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
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
<h1>Light Rocks <span><i id=loopms class=loopms></i> <i id=link class=linkdot>idle</i> <b id=host></span></h1>
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
<div class="row first"><span>Room level (before any gain)</span><b id=sDb>-</b></div>
<div class=dbscale><u id=dbnow></u><span class=zq></span><span class=zg></span><span class=zh></span></div>
<div class="hint dyn" id=dbhint>the microphone's own level: the green band is the range that suits the detectors</div>
<div class="row"><span>Microphone gain</span><b id=gainDbv>-</b></div>
<div class=rline>
<input type=range id=gainDb min=-24 max=18 step=3>
<button type=button id=agcbtn class=tbtn>AGC</button>
</div>
<div class="hint dyn" id=agchint>negative attenuates: use it when the room is too loud</div>
<div class="hint dyn" id=agcwhat></div>
<div class=stats>
<div><span>Input now</span><b id=sLvl>0%</b></div>
<div><span>Peak heard</span><b id=sAgcPk>-</b></div>
<div><span>Peak freq</span><b id=sPk>-</b></div>
</div>
</div>

<div class=card>
<div class="row first"><span>Physical strip</span><b id=tabhint style="color:#8b949e;font-weight:400;font-size:.76rem"></b></div>
<div class=seg id=pick></div>
<div class=row style="margin-top:10px"><span>Strip brightness</span><b id=brightnessPctv>-</b></div>
<input type=range id=brightnessPct min=1 max=100 step=1>
<div class=row><span>LEDs on this strip</span><b id=ledCountv>-</b></div>
<input type=range id=ledCount min=1 max=1000 step=1>
<div class=rline style="margin-top:6px">
<button type=button id=lcdec10 class=stepbtn>-10</button>
<button type=button id=lcdec class=stepbtn>&larr;</button>
<button type=button id=lcinc class=stepbtn>&rarr;</button>
<button type=button id=lcinc10 class=stepbtn>+10</button>
<button type=button id=calbtn class=tbtn style="margin-left:auto">Calibrate</button>
</div>
<div class="hint dyn" id=calhint>calibrate lights only the end LED while you adjust the count</div>
<div class="row" style="margin-top:12px"><span>Light layers inside</span><b id=lcv>-</b></div>
<div class=layerlist id=layerlist></div>
<div class=rline style="margin-top:8px">
<button type=button id=addlayer class=tbtn>+ Add layer</button>
<button type=button id=remlayer class=tbtn>Remove</button>
<button type=button id=solobtn class=tbtn style="margin-left:auto">Solo</button>
</div>
<div class=ptitle id=ptitle style="margin:12px 0 8px"></div>
<div class=row><span>Layer power</span><label class=sw><input type=checkbox id=pwr><i></i></label></div>
<div id=ipanel></div>
</div>

<div class=card>
<div class=ptitle id=bmtitle>Band map<small id=bmhint>where each layer of this strip listens</small></div>
<canvas id=bandmap width=520 height=150></canvas>
<div class=bmlbl id=bmlbl></div>
</div>

<div class=card id=trigbox></div>
<div class=card id=dispbox></div>
<div class=card id=animbox></div>

<div class=card>
<div class="row first"><span>Preset slots</span><b class=dynb id=pmsg></b></div>
<div class=slots id=slots></div>
<div class=rline style="margin-top:8px">
<button id=ldbtn class="btn alt" style="flex:1">Recall</button>
<button id=spbtn class=btn style="flex:1">Save here</button>
</div>
<div class="hint dyn" id=sthint>pick a slot, then Recall or Save. Saving over a used slot asks twice.</div>
<button id=rst class="btn warn wide" style="margin-top:10px">Reset live settings</button>
</div>

<div class=hint style="text-align:center">BtnA: cycle triggers &middot; BtnB: next strip &middot; hold A: Wi-Fi &middot; hold B: save</div>

<script>
var S=0,L=0,ST=null,BUILT=false,SEEDED=false,TRIG=1,EFF=0,REV=0,LOOP=1,RETRIG=1,CALIB=0,SOLO=-1;
var TB={thr:1,beat:2,tap:4,accel:8,gyro:16,lfo:32};
var CUR_COL='#ff6a00',WHEEL_BG=null,WX=76,WY=76,WDRAG=false,TDRAG=-1,MSGTMR=0;
var INFLIGHT=false,POLL_BUSY=false,PTMR=0,AFTER=null,HOLD_TMRS={},AGC=0;
var USER_TS=0,SENT_AT=0,DRAG_TS=0;
/* Desired writes that the board has not confirmed yet, and the send queue. */
var DES={},Q=[];

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
/* A gesture whose end never arrived (a scroll claimed the pointer, the page was
   left mid-drag, the tab lost focus) must not latch: polls stop while a drag is
   in progress, so a latched flag made the whole page stop updating until a
   reload. Anything older than a few seconds is stale by definition. */
function dragWatchdog(){
 if((WDRAG||TDRAG>=0||DRAGGING>0)&&Date.now()-DRAG_TS>4000){
  WDRAG=false;DRAGGING=0;TDRAG=-1;
 }
}
document.addEventListener('pointermove',function(){DRAG_TS=Date.now();},true);
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

/* ---- writes: a queue plus a desired-state map ----
   The board serves one socket at a time, so a slider drag and a poll can collide
   and one of them is dropped. A dropped write used to be invisible: the page had
   already drawn the new value, the next poll overwrote it with the board's old
   one, and the control looked "stuck" (the threshold marker was the worst case
   because it is a single tick that jumps back).

   Now every write is also recorded as a desire. The request queue sends them one
   at a time; a state read clears a desire when the board agrees and re-queues it
   when it does not. Losing a request costs a retry, not the edit. */
function abortAfter(ms){
 if(!window.AbortController)return {};
 var ctl=new AbortController();
 setTimeout(function(){try{ctl.abort();}catch(e){}},ms);
 return {signal:ctl.signal};
}
function busySocket(){return INFLIGHT||POLL_BUSY;}
function scopeId(p,l,k){return p+'|'+(l===null||l===undefined?'-':l)+'|'+k;}
/* Value of a parameter as the board reports it, so a desire can be checked. */
function boardVal(s,p,l,k){
 if(!s||!s.strips||!s.strips[p])return undefined;
 if(k==='gainDb'||k==='agc')return s[k];
 var st=s.strips[p];
 if(l===null||l===undefined)return st[k];
 if(!st.layers||!st.layers[l])return undefined;
 return st.layers[l][k];
}
/* Is this exact parameter still waiting to be confirmed by the board? */
function inFlight(p,l,k){
 return DES[scopeId(p,l,k)]!==undefined;
}
function want(p,l,k,v,once){
 var id=scopeId(p,l,k);
 var e=DES[id];
 if(!e){e={p:p,l:l,k:k,v:v,tries:0,ts:Date.now(),due:0,once:!!once};DES[id]=e;}
 else{e.v=v;e.ts=Date.now();e.due=0;e.once=!!once;}
 if(Q.indexOf(id)<0)Q.push(id);
 USER_TS=Date.now();
 if(!PTMR&&!busySocket()){PTMR=setTimeout(function(){PTMR=0;flush();},45);}
}
/* The three write scopes. A global parameter (mic gain, AGC) is stored on the
   board outside the strip, so it is keyed on strip 0 with no layer. */
function sendLayer(k,v){want(S,L,k,v);}
function sendPhys(k,v){want(S,null,k,v);}
function sendGlobal(k,v){want(0,null,k,v);}
/* Structural writes are not values, they are events: "add one layer" twice is two
   layers. They therefore never become a *desire* - a desire is re-sent until the
   board's state agrees with it, and the state can never agree with an event. */
function sendOnce(k,v){want(S,null,k,v,true);}
var STRUCT_AT=0;
/* One structural request at a time: either one is still in flight, or the last
   one answered so recently that this is the second half of a double tap. */
function structBusy(){
 for(var id in DES)if(DES[id].once)return true;
 return Date.now()-STRUCT_AT<500;
}
function urlFor(e){
 return 'strip='+e.p+(e.l===null||e.l===undefined?'':'&layer='+e.l)
  +'&'+e.k+'='+encodeURIComponent(e.v);
}
function flush(){
 if(PTMR){clearTimeout(PTMR);PTMR=0;}
 // Watchdog: a request that has been "in flight" far longer than its deadline
 // means the abort did not settle either. Never leave the page wedged on it.
 if(INFLIGHT&&Date.now()-SENT_AT>3500){INFLIGHT=false;}
 if(busySocket()){
  if(!PTMR)PTMR=setTimeout(function(){PTMR=0;flush();},60);
  return;
 }
 // Skip entries still backing off after a failed attempt, and remember the
 // earliest one that is due, so a board that is briefly unreachable is retried
 // over seconds rather than burning every attempt in a burst.
 var dueId=null,dueAt=0;
 var now=Date.now();
 for(var i=0;i<Q.length;){
  var qid=Q[i];
  if(!DES[qid]){Q.splice(i,1);continue;}
  var due=DES[qid].due||0;
  if(due<=now){dueId=qid;break;}
  if(!dueAt||due<dueAt)dueAt=due;
  i++;
 }
 if(!dueId&&dueAt){
  if(!PTMR)PTMR=setTimeout(function(){PTMR=0;flush();},Math.max(30,dueAt-Date.now()));
  return;
 }
 if(dueId){var at=Q.indexOf(dueId);if(at>0)Q.splice(at,1),Q.unshift(dueId);}
 if(!Q.length){
  // Nothing to send: run a pending state refresh now, otherwise it waits for a
  // request that may never come and the page keeps a stale view of the board.
  var f=AFTER;AFTER=null;if(f)f();
  return;
 }
 var id=dueId;if(!id){id=Q.shift();}
 if(!id||!DES[id]){return;}
 if(Q[0]===id)Q.shift();
 var e=DES[id];
 INFLIGHT=true;SENT_AT=Date.now();
 var done=function(ok){
  INFLIGHT=false;
  if(e.once){
   // Sent once, never retried: a retried "add layer" is a second layer.
   delete DES[id];
   if(!ok)flashMsg('no answer, nothing changed');
  }
  if(!ok&&!e.once&&DES[id]){
   e.tries++;
   // 150 ms, 300, 600, 1.2 s, then every 2 s: a board that is unreachable for a
   // few seconds still ends up with the value, without flooding it.
   var back=Math.min(2000,150*Math.pow(2,Math.min(e.tries,5)));
   e.due=Date.now()+back;
   if(e.tries<=10&&Q.indexOf(id)<0)Q.push(id);
   if(e.tries>10){delete DES[id];}
  }
  if(Q.length){if(!PTMR)PTMR=setTimeout(function(){PTMR=0;flush();},40);return;}
  if(AFTER){var f=AFTER;AFTER=null;f();}
 };
 var opt=abortAfter(2500);
 opt.method='POST';
 fetch('/api/params?'+urlFor(e),opt)
  .then(function(r){done(r.ok);},function(){done(false);});
}
function fetchState(){
 var opt=abortAfter(2500);
 opt.cache='no-store';
 return fetch('/api/state',opt).then(function(r){
  if(!r.ok)throw new Error('http '+r.status);
  return r.json();
 });
}
/* A state read is the truth: drop the desires the board already satisfies and
   re-queue the ones it does not. */
function reconcile(s){
 var now=Date.now();
 for(var id in DES){
  var e=DES[id];
  // An event has no matching value in the state, so comparing it would keep it
  // forever and re-fire it on every poll. It is done when its request returns.
  if(e.once)continue;
  if(String(boardVal(s,e.p,e.l,e.k))===String(e.v)){delete DES[id];continue;}
  if(now-e.ts>700&&e.tries<10&&(e.due||0)<=now){
   e.tries++;e.ts=now;
   e.due=now+Math.min(2000,200*Math.pow(2,Math.min(e.tries,4)));
   if(Q.indexOf(id)<0)Q.push(id);
  }else if(e.tries>=10){
   // The board will not take this value: accept its answer instead of lying.
   delete DES[id];
  }
 }
}
function syncState(then){
 AFTER=function(){
  fetchState().then(function(s){apply(s);if(then)then();})
   .catch(function(){flashMsg('sync lost, reload');});
 };
 flush();
}
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
var LIST_SIG='';
/* The layer list.
   One row per live layer, carrying three separate facts that used to be crammed
   into one tab:
     - the dot is the beam's COLOUR (always visible, whether or not you are on it)
     - the switch is POWER (toggles that layer alone)
     - the row highlight and the arrow mean "this is what the cards below edit"
   Tapping a row selects it; tapping the switch toggles it and does not select. */
function listSig(){
 var p=phys();
 if(!p||!p.layers)return '';
 var t='sel'+L+'|n'+p.layerCount;
 for(var i=0;i<p.layerCount;i++){
  var ly=p.layers[i];
  t+='|'+ly.color+(ly.enabled?'1':'0')+ly.centerHz+'/'+ly.widthHz+(ly.triggers||0);
 }
 return t;
}
function buildLayerList(){
 var p=phys();
 if(!p||!p.layers)return;
 var n=Math.max(1,p.layerCount);
 var sig=listSig();
 if(sig!==LIST_SIG){
  LIST_SIG=sig;
  var html='';
  for(var i=0;i<n;i++){
   var ly=p.layers[i],on=!!ly.enabled;
   var band=ly.centerHz+(ly.widthHz?(' \u00b1'+Math.round(ly.widthHz/2)):'');
   html+='<div class="lrow'+(i===L?' sel':'')+(on?'':' off')+'" id=lr'+i+' data-i='+i+'>'
    +'<i class=d进而></i>'
    +'<b class=nm>Layer '+(i+1)+'</b>'
    +'<span class=bd>'+band+' Hz</span>'
    +'<span class=st>'+(on?'on':'off')+'</span>'
    +'<label class="sw pw"><input type=checkbox id=lp'+i+(on?' checked':'')+'><i></i></label>'
    +'</div>';
  }
  $('layerlist').innerHTML=html.replace(/d进而/g,'dot');
  for(var j=0;j<n;j++){(function(k){
   var row=$('lr'+k);
   row.style.setProperty('--c','#'+(p.layers[k].color||'808080'));
   row.querySelector('.dot').style.background='#'+(p.layers[k].color||'808080');
   row.querySelector('.dot').style.color='#'+(p.layers[k].color||'808080');
   row.onclick=function(ev){
    if(ev&&ev.target&&ev.target.closest&&ev.target.closest('.pw'))return; // switch
    flush();L=k;fillInputs();live();
   };
   $('lp'+k).onchange=function(ev){
    ev.stopPropagation();
    var onv=this.checked?1:0;
    var lay=phys().layers[k];
    if(lay)lay.enabled=onv;      // optimistic, reconcile() will confirm
    sendLayerAt(k,'enabled',onv);
    flush();refreshTitle();
    LIST_SIG='';                 // repaint the row's on/off state
    buildLayerList();
   };
  })(j);}
 }
 if($('lcv'))$('lcv').textContent=n>1?(n+' of 4'):'1 of 4 \u00b7 single beam';
}

/* Writes that belong to a layer which is not the one being edited (the row
   switch), so they use that layer's index explicitly. */
function sendLayerAt(l,k,v){want(S,l,k,v);}

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
  e.preventDefault();TDRAG=S;DRAG_TS=Date.now();
  if(bar.setPointerCapture){try{bar.setPointerCapture(e.pointerId);}catch(_){}}
  pickThr(e,false);
 });
 bar.addEventListener('pointermove',function(e){
  if(TDRAG<0||!(TRIG&TB.thr))return;
  e.preventDefault();pickThr(e,false);
 });
 bar.addEventListener('pointerup',function(e){if(TDRAG<0)return;TDRAG=-1;pickThr(e,true);});
 bar.addEventListener('pointercancel',function(){TDRAG=-1;flush();});
 bar.addEventListener('lostpointercapture',function(){if(TDRAG>=0){TDRAG=-1;flush();}});
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
  e.preventDefault();WDRAG=true;DRAG_TS=Date.now();
  if(wh.setPointerCapture){try{wh.setPointerCapture(e.pointerId);}catch(_){}}
  pickWheel(e,false);
 });
 wh.addEventListener('pointermove',function(e){
  if(!WDRAG)return;
  e.preventDefault();pickWheel(e,false);
 });
 wh.addEventListener('pointerup',function(e){if(!WDRAG)return;WDRAG=false;pickWheel(e,true);});
 wh.addEventListener('pointercancel',function(){WDRAG=false;flush();});
 // A capture that is lost without a pointerup used to leave the drag flag set,
 // and then ANY pointer move over the wheel recoloured the layer by itself.
 wh.addEventListener('lostpointercapture',function(){if(WDRAG){WDRAG=false;flush();}});
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

 /* ---- physical: LED count + calibration, in the strip card ---- */
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
  if(structBusy())return;
  STRUCT_AT=Date.now();
  // A new layer arrives switched OFF, with its own colour and band, so adding
  // one never changes the light that is already on the strip.
  sendOnce('layerAdd',1);
  syncState(function(){L=layerCount()-1;fillInputs();live();
   flashMsg('Layer '+(L+1)+' added (off)');});
 };
 $('remlayer').onclick=function(){
  var n=layerCount();
  if(n<=1){flashMsg('one layer must stay');return;}
  if(structBusy())return;
  STRUCT_AT=Date.now();
  sendOnce('layerRemove',L);
  syncState(function(){
   if(L>layerCount()-1)L=layerCount()-1;
   fillInputs();live();
   flashMsg('Layer '+(L+1)+' removed');});
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
 ['ptitle','ttitle','dtitle','atitle'].forEach(function(id){if($(id))$(id).style.borderLeftColor=col;});
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
 // lcv is written by buildLayerList(), so the count and the list never disagree.
 $('tabhint').textContent=p.ledCount+' LEDs · '+p.brightnessPct+'% output';
 buildLayerList();}

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
 if(h)h.textContent=CALIB?'CALIBRATING: only the end LED is lit':'calibrate lights only the end LED while you adjust the count';
}
function setSolo(){
 var b=$('solobtn');
 if(!b)return;
 b.className='tbtn '+(SOLO===L?'solo':'');
 b.textContent=SOLO===L?'Solo ON':'Solo';
}
function setLayerSel(){ buildLayerList(); }

function fillInputs(){
 if(!ST||!BUILT)return;
 var c=cur(),p=phys();
 if(!c||!p)return;
 // A control with a write still in flight keeps what the user just set: the
 // value on screen is newer than the one in the state read, and overwriting it
 // is exactly what made the threshold look stuck.
 var lay=function(k){return !inFlight(S,L,k);};
 var phy=function(k){return !inFlight(S,null,k);};
 [TRIGSPEC,ANIMSPEC].forEach(function(list){
  list.forEach(function(s){
   if($(s.k)&&c[s.k]!==undefined&&lay(s.k))$(s.k).value=c[s.k];
  });
 });
 [['beatSensitivityPct','beatSensitivityPct'],['motionThresholdPct','motionThresholdPct'],
  ['lfoPeriodMs','lfoPeriodMs'],['attackMs','attackMs'],['releaseMs','releaseMs'],
  ['intensityPct','intensityPct'],['softnessPct','softnessPct']].forEach(function(pair){
   if($(pair[0])&&lay(pair[1]))$(pair[0]).value=c[pair[1]];
  });
 if(phy('brightnessPct'))$('brightnessPct').value=p.brightnessPct;
 if(phy('ledCount'))$('ledCount').value=p.ledCount;
 if(lay('enabled'))$('pwr').checked=!!c.enabled;
 if(!inFlight(0,null,'gainDb'))$('gainDb').value=ST.gainDb;
 if(!inFlight(0,null,'agc')){AGC=ST.agc?1:0;setAgc();}
 if(lay('triggers'))TRIG=c.triggers||0;
 if(lay('effect'))EFF=c.effect||0;
 if(lay('reverse'))REV=c.reverse||0;
 if(lay('loop'))LOOP=c.loop?1:0;
 if(lay('retrigger'))RETRIG=c.retrigger!==undefined?(c.retrigger?1:0):1;
 if(phy('calib'))CALIB=p.calib?1:0;
 if(phy('solo'))SOLO=(p.solo===undefined)?-1:p.solo;
 buildLayerList();
 setLayerSel();setTriggers();setEff();setDir();setLoop();setRetrig();setCalib();setSolo();
 if(lay('color'))setColorHex(colOf(c));
 labels();
}
function flashMsg(t){
 $('pmsg').textContent=t;
 if(MSGTMR)clearTimeout(MSGTMR);
 MSGTMR=setTimeout(function(){MSGTMR=0;if(ST)$('pmsg').textContent=ST.unsaved?'auto-saving…':'auto-saved';},2200);
}

/* ---- live data: the binary telemetry frame ----
   Everything that changes continuously arrives in one 83-byte frame, 25 times a
   second, over a WebSocket; settings arrive as JSON only when they change or
   after a write. Keeping the two apart is what lets the meters be smooth without
   re-rendering the page. Offsets match src/telemetry.h. */
var TEL={seq:0,flags:0,level:0,dbfs:-99,peakHz:0,gain:0,agcPeak:0,agcAct:0,
         stripBright:[0,0],level2:[],beat:[],bpm:[],conf:[],blobs:[],spec:[]};
function parseFrame(b){
 // Per (strip, layer) arrays are indexed i*4+l so the page can look them up flat.
 var o=0, t={};
 t.ver=b[0]; t.flags=b[1];
 t.seq=b[2]|(b[3]<<8); o=4;
 t.level=b[o++]; t.dbfs=(b[o++]<<24)>>24; t.peakHz=b[o]|(b[o+1]<<8); o+=2;
 t.gain=(b[o++]<<24)>>24; t.agcPeak=b[o++];
 t.agcAct=(b[o++]<<24)>>24;
 t.stripBright=[b[o],b[o+1]]; o+=2;
 t.level2=[];for(var i=0;i<8;i++)t.level2.push(b[o++]);
 t.beat=[];for(var i=0;i<8;i++)t.beat.push(b[o++]);
 t.bpm=[];for(var i=0;i<8;i++){t.bpm.push(b[o]|(b[o+1]<<8));o+=2;}
 t.conf=[];for(var i=0;i<8;i++)t.conf.push(b[o++]);
 t.blobs=[];for(var i=0;i<8;i++)t.blobs.push(b[o++]);
 t.spec=[];for(var i=0;i<30;i++)t.spec.push(b[o++]);
 t.loopMax=b.length>o?b[o]:0;
 t.agc=!!(t.flags&1); t.unsaved=!!(t.flags&2);
 return t;
}

/* Paints the live half of the UI. Cheap on purpose: text is only written when it
   changed, so a 25 Hz update costs a few style writes rather than a DOM rebuild. */
function paintTelemetry(){
 if(!ST||!BUILT)return;
 var t=TEL;
 var c=cur(),p=phys();
 if(c&&p){
  var col=colOf(c);
  var lvl=(TRIG&TB.beat)?Math.max(t.beat[S*4+L]||0,t.level2[S*4+L]||0):(t.level2[S*4+L]||0);
  lvl=Math.max(0,Math.min(100,lvl));
  $('mb0').style.background=col;
  $('mb0').style.width=lvl+'%';
  $('mu0').style.left='calc('+c.thresholdPct+'% - 2px)';
  var bpm=t.bpm[S*4+L]||0, conf=t.conf[S*4+L]||0, beat=t.beat[S*4+L]||0;
  var line='lvl '+(t.level2[S*4+L]||0)+'%';
  if(TRIG&TB.beat)line+=' \u00b7 '+(bpm>0?(bpm+' BPM '+conf+'%'):'listening')+' \u00b7 beat '+beat+'%';
  line+=' \u00b7 '+(t.blobs[S*4+L]||0)+' active';
  setText($('mt0'),line);
 }
 // pads
 for(var q=0;q<ST.strips.length;q++){
  var sq=ST.strips[q];
  for(var lq=0;lq<sq.layerCount;lq++){
   var g=$('pgrp'+q+'_'+lq);
   if(!g)continue;
   // Clamped: the bar is a percentage of the strip, and a corrupt or unexpected
   // frame must not paint a fill wider than the bar it lives in.
   var out=Math.max(0,Math.min(100,Math.max(t.level2[q*4+lq]||0, t.beat[q*4+lq]||0)));
   setText($('pgst'+q+'_'+lq),layModeName(sq.layers[lq])+' \u00b7 '+(t.blobs[q*4+lq]||0)+' \u00b7 lvl '+out+'%');
   $('pgb'+q+'_'+lq).style.width=out+'%';
  }
 }
 // microphone
 setText($('sLvl'),t.level+'%');
 setText($('sDb'),t.dbfs+' dBFS');
 setText($('sPk'),t.peakHz+' Hz');
 if($('sAgcPk'))setText($('sAgcPk'),t.agcPeak+'%');
 drawDbScale();
 explainAgc();
 drawBandMap();
 // A stalling render loop is the explanation for stuttering LEDs and for a
 // metronome that seems to skip; show it only when it is actually bad.
 if($('loopms')){
  if(t.loopMax>40){
   $('loopms').style.display='inline-block';
   $('loopms').textContent='loop '+t.loopMax+' ms';
   $('loopms').style.background=t.loopMax>90?'#5d1a1a':'#3a2f12';
   $('loopms').style.color=t.loopMax>90?'#ffb3ad':'#ffd580';
  }else{
   $('loopms').style.display='none';
  }
 }
 if(!MSGTMR)setText($('pmsg'),t.unsaved?'auto-saving\u2026':'auto-saved');
 if($('agcbtn')&&!inFlight(0,null,'agc')){AGC=t.agc?1:0;setAgc();}
 $('gainDbv').textContent=(t.gain>0?'+':'')+t.gain+' dB';
}
/* Blink-free text update: touching textContent every frame would re-layout. */
function setText(el,v){
 if(!el||v===null||v===undefined)return;
 if(el.textContent!==v)el.textContent=v;
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
 if(c&&p){
  CALIB=p.calib?1:0;setCalib();setSolo();
  $('mb0').style.background=colOf(c);
 }
 for(var q=0;q<ST.strips.length;q++){
  var sq=ST.strips[q];
  for(var lq=0;lq<sq.layerCount;lq++){
   var g=$('pgrp'+q+'_'+lq);
   if(g)g.style.setProperty('--c','#'+(sq.layers[lq].color||'808080'));
  }
 }
 if(ST.ssid)$('host').textContent=ST.ssid+' \u00b7 '+location.host+(ST.ver?(' \u00b7 '+ST.ver):'');
 $('gainDbv').textContent=(ST.gainDb>0?'+':'')+ST.gainDb+' dB';
 if($('slots')&&(ST.presets||[]).length)buildSlots();
 paintTelemetry();
 ensureLink();
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
 reconcile(s);
 // The live link's port comes from the board; learn it once and connect. This
 // happens at the end of apply(), after SEEDED, because the link must not start
 // against a page whose controls are not built yet.
 if(!WS_PORT&&s.wsPort){WS_PORT=s.wsPort;}
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
 else if(!editing()&&Date.now()-USER_TS>600){
  // Nothing is being touched, so make the controls say exactly what the board
  // holds. Whatever drifted - the other phone, AGC moving the gain, the board's
  // own buttons - is corrected within one poll. Controls with a write still in
  // flight are skipped, so an edit is never overwritten by a stale read.
  fillInputs();
 }
 live();
}
/* Returns the new state, or null when the board refused or did not answer - so a
   caller can only report success when there was some. */
function post(u,refresh){
 INFLIGHT=true;SENT_AT=Date.now();
 var opt=abortAfter(4000);
 opt.method='POST';opt.cache='no-store';
 return fetch(u,opt).then(function(r){
  INFLIGHT=false;
  if(!r.ok)return null;
  return r.json();
 },function(){INFLIGHT=false;return null;}).then(function(s){
  if(!s){flashMsg('board refused it');return null;}
  apply(s);if(refresh)fillInputs();buildSlots();
  return s;
 });
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
  ?'automatic: the gain is nudged so peaks land in the centre of the green band'
  :'negative attenuates: use it when the room is too loud';
}
/* The needle sits on the same scale the firmware uses: -55 dBFS is 0 %, -18 is
   100 %. The zones are where the beat and threshold detectors like to work, so
   it is obvious at a glance whether the gain has room to help. */
function drawDbScale(){
 if(!$('dbnow'))return;
 var lo=-55, hi=-18, db=TEL&&TEL.dbfs!==undefined?TEL.dbfs:lo;
 var pct=Math.round(Math.max(0,Math.min(1,(db-lo)/(hi-lo)))*100);
 $('dbnow').style.left=pct+'%';
 var z='quiet room';
 if(pct>=80)z='loud: the detectors may saturate';
 else if(pct>=40)z='good range';
 if($('dbhint'))$('dbhint').textContent='room '+db+' dBFS \u00b7 '+z;
}
function explainAgc(){
 var h=$('agcwhat');
 if(!h)return;
 if(!AGC){h.textContent='';return;}
 var pk=TEL.agcPeak||0, act=TEL.agcAct, g=TEL.gain;
 var t;
 if(act===1)t='raising the gain: peaks only reach '+pk+'% of the range';
 else if(act===-1)t='lowering the gain: peaks reach '+pk+'%, too close to clipping';
 else if(act===2)t='holding: peaks at '+pk+'% are in the good band';
 else t='idle: nothing loud enough to judge ('+pk+'%)';
 h.textContent='AGC '+t+' \u00b7 now at '+(g>0?'+':'')+g+' dB';
}
$('agcbtn').onclick=function(){
 AGC=AGC?0:1;
 setAgc();
 sendGlobal('agc',AGC);
 flush();
};
$('gainDb').addEventListener('input',function(){
 var v=+$('gainDb').value;
 $('gainDbv').textContent=(v>0?'+':'')+v+' dB';
});
$('gainDb').addEventListener('change',function(){sendGlobal('gainDb',$('gainDb').value);flush();});
/* ---- band map ----
   One stacked row per layer of the selected strip, on a log frequency axis, the
   same picture the TFT shows. The band, its centre and the live level of that
   band are enough to see whether two layers are fighting for the same part of the
   spectrum - which is the point. The faint silhouette behind is the live
   spectrum, so the bands can be compared against the music. */
var BM_LO=65, BM_HI=7600;
function bmX(hz,w){
 var lo=Math.log(BM_LO), hi=Math.log(BM_HI);
 var t=(Math.log(Math.max(BM_LO,Math.min(BM_HI,hz)))-lo)/(hi-lo);
 return Math.round(t*w);
}
function drawBandMap(){
 var cv=$('bandmap');
 if(!cv||!ST||!phys())return;
 // Size the backing store to the CSS box so the axis is not stretched: the
 // canvas element is 100 % wide on a phone, whatever that turns out to be.
 var dpr=window.devicePixelRatio||1;
 var cssW=Math.max(220,Math.round(cv.clientWidth||cv.width));
 var cssH=Math.round(cv.clientHeight||cv.height);
 var wantW=Math.round(cssW*dpr), wantH=Math.round(cssH*dpr);
 if(cv.width!==wantW||cv.height!==wantH){cv.width=wantW;cv.height=wantH;}
 var w=cv.width,h=cv.height,ctx=cv.getContext('2d');
 var sc=dpr;
 var p=phys(),n=Math.max(1,p.layerCount);
 var padT=6*sc, padB=16*sc;
 var rowH=Math.floor((h-padT-padB)/4);      // four slots, so the axis never jumps
 ctx.clearRect(0,0,w,h);

 // gridlines at the octaves
 ctx.strokeStyle='#1b2230';ctx.fillStyle='#4d5560';
 ctx.font=Math.round(10*sc)+'px system-ui,sans-serif';ctx.textAlign='center';
 var ticks=[65,125,250,500,1000,2000,4000,8000];
 ticks.forEach(function(f){
  if(f>BM_HI)return;
  var x=bmX(f,w);
  ctx.beginPath();ctx.moveTo(x,padT);ctx.lineTo(x,h-padB);ctx.stroke();
 });

 // live spectrum silhouette, faint, behind everything
 var spec=TEL.spec||[];
 if(spec.length){
  ctx.beginPath();ctx.moveTo(0,h-padB);
  for(var i=0;i<spec.length;i++){
   var hz=BM_LO*Math.pow(BM_HI/BM_LO,(i+0.5)/spec.length);
   ctx.lineTo(bmX(hz,w),h-padB-(spec[i]/100)*(h-padT-padB));
  }
  ctx.lineTo(w,h-padB);ctx.closePath();
  ctx.fillStyle='rgba(88,166,255,.11)';ctx.fill();
 }

 // one row per layer
 for(var l=0;l<n;l++){
  var ly=p.layers[l];
  var y=padT+l*rowH;
  var lo=ly.centerHz-ly.widthHz/2, hi=ly.centerHz+ly.widthHz/2;
  var x0=bmX(lo,w), x1=bmX(hi,w);
  if(x1<x0+3*sc)x1=x0+3*sc;
  var col='#'+(ly.color||'808080');
  var on=ly.enabled;
  // the band
  ctx.globalAlpha=on?0.20:0.08;
  ctx.fillStyle=col;
  ctx.fillRect(x0,y+2,x1-x0,rowH-5);
  // how much of that band is there right now
  var lvl=Math.max(0,Math.min(100,TEL.level2[S*4+l]||0));
  ctx.globalAlpha=on?0.85:0.25;
  ctx.fillRect(x0,y+2*sc,Math.round((x1-x0)*lvl/100),rowH-5*sc);
  // edges + centre
  ctx.globalAlpha=on?1:0.4;
  ctx.strokeStyle=col;ctx.lineWidth=1.5*sc;
  ctx.beginPath();ctx.moveTo(x0,y+sc);ctx.lineTo(x0,y+rowH-3*sc);
  ctx.moveTo(x1,y+sc);ctx.lineTo(x1,y+rowH-3*sc);ctx.stroke();
  var cx=bmX(ly.centerHz,w);
  ctx.beginPath();ctx.moveTo(cx,y);ctx.lineTo(cx,y+rowH-2*sc);ctx.stroke();
  // label
  ctx.globalAlpha=1;
  ctx.textAlign='left';
  ctx.font='bold '+Math.round(11*sc)+'px system-ui,sans-serif';
  ctx.fillStyle=on?col:'#5a6270';
  ctx.fillText('L'+(l+1),4*sc,y+rowH-6*sc);
  ctx.font=Math.round(10*sc)+'px system-ui,sans-serif';
  ctx.fillStyle=on?'#c9d1d9':'#5a6270';
  var txt=ly.centerHz+' Hz ±'+Math.round(ly.widthHz/2);
  ctx.textAlign='right';
  ctx.fillText(txt,w-4*sc,y+rowH-6*sc);
  ctx.textAlign='left';
 }
 ctx.globalAlpha=1;

 // axis labels
 var lbl='';
 ticks.forEach(function(f){
  if(f>BM_HI)return;
  lbl+='<span style="left:'+(bmX(f,w)/w*100)+'%">'+(f>=1000?(f/1000)+'k':f)+'</span>';
 });
 $('bmlbl').innerHTML=lbl;
 $('bmhint').textContent=p.layerCount+' layer'+(p.layerCount>1?'s':'')
  +' \u00b7 '+(TEL.level||0)+'% input now';
}
/* ---- presets ----
   A slot chip shows what the slot holds (its layer colours), so it is obvious
   whether a slot is used. Recall and Save act on the selected slot, report what
   happened, and ask twice before destroying anything: a second tap within four
   seconds confirms. */
var SLOT=0, ARMED='';
function presetOf(i){return ST&&ST.presets&&ST.presets[i]?ST.presets[i]:{has:0,layers:0,cols:''};}
function arm(id,label){
 ARMED=id;
 var b=$(id);b.classList.add('armed');b.textContent=label;
 setTimeout(disarm,4000);
}
function disarm(){
 if(ARMED){var b=$(ARMED);if(b){b.classList.remove('armed');}
  if(ARMED==='spbtn')$('spbtn').textContent='Save here';
  if(ARMED==='ldbtn')$('ldbtn').textContent='Recall';}
 ARMED='';
}
var SLOT_SIG='';
/* Signature of what the chips show. Rebuilding the innerHTML on every state read
   detaches the node under the user's finger - a tap aimed at "slot 2" lands on
   nothing - so the chips are only rebuilt when their content actually changes. */
function slotsSig(){
 var t='sel'+SLOT;
 for(var i=0;i<3;i++){var p=presetOf(i);t+='|'+p.has+'/'+p.layers+'/'+(p.cols||'');}
 return t;
}
function buildSlots(){
 var sig=slotsSig();
 if(sig!==SLOT_SIG){
  SLOT_SIG=sig;
  var html='';
  for(var i=0;i<3;i++){
   var p=presetOf(i),dots='';
   var cols=(p.cols||'').split(' ').filter(function(c){return c.length===6;});
   cols.forEach(function(c){dots+='<i style="background:#'+c+'"></i>';});
   var n=p.layers||1;
   html+='<button type=button class="slot'+(p.has?' used':' empty')+(i===SLOT?' sel':'')+'" id=slot'+i+'>'
    +'<b>Slot '+(i+1)+'</b>'
    +(p.has?('<span class=dots>'+dots+'</span><span class=lay>'
      +n+' layer'+(n>1?'s':'')+'</span>'):'<span class=lay>empty</span>')
    +'</button>';
  }
  $('slots').innerHTML=html;
  for(var j=0;j<3;j++){(function(n){$('slot'+n).onclick=function(){
   SLOT=n;disarm();buildSlots();
   var p=presetOf(n);
   $('sthint').textContent=p.has
    ?('Slot '+(n+1)+': '+(p.layers||1)+' layer'+((p.layers||1)>1?'s':'')+'. Recall replaces the live settings.')
    :('Slot '+(n+1)+' is empty: nothing to recall, Save writes here.');
  };}) (j);}
 }
 updatePresetButtons();
}
function updatePresetButtons(){
 var p=presetOf(SLOT);
 $('ldbtn').disabled=!p.has;
 if(ARMED!=='ldbtn')$('ldbtn').textContent='Recall';
 if(ARMED!=='spbtn')$('spbtn').textContent=p.has?'Save here (overwrite)':'Save here';
}
$('ldbtn').onclick=function(){
 if(ARMED==='ldbtn'){
  disarm();
  post('/api/preset?slot='+(SLOT+1)+'&action=load',true).then(function(s){
   if(!s)return;
   flashMsg('Recalled slot '+(SLOT+1));
   $('sthint').textContent='Recall of slot '+(SLOT+1)+' applied to the live settings.';
  });
  return;
 }
 if(!presetOf(SLOT).has){flashMsg('Slot '+(SLOT+1)+' is empty');return;}
 arm('ldbtn','Tap again: replace live');
};
$('spbtn').onclick=function(){
 var p=presetOf(SLOT);
 if(p.has&&ARMED!=='spbtn'){
  arm('spbtn','Tap again: overwrite');
  return;
 }
 disarm();
 post('/api/preset?slot='+(SLOT+1)+'&action=save',true).then(function(s){
  if(!s)return;
  flashMsg('Saved to slot '+(SLOT+1));
  $('sthint').textContent='Slot '+(SLOT+1)+' now holds these settings.';
 });
};
$('rst').onclick=function(){
 if(ARMED==='rst'){
  disarm();
  post('/api/reset',true).then(function(s){if(s)flashMsg('Live settings reset');});
  return;
 }
 arm('rst','Tap again: reset live');
};

/* ---- live link ----
   Preferred: a WebSocket on port 81 pushes an 83/91-byte frame at 25 Hz. If that
   cannot be established (older firmware, a network that blocks it), the page
   falls back to fetching the very same frame from /api/telemetry, four times a
   second. Both paths end in the identical decode, so nothing else has to care
   which one is in use. */
var WS=null, WS_LIVE=false, WS_TRY=0, WS_TIMER=0, FB_TIMER=0, LINK='off';
var WS_PORT=0; // learned from the board's state
var TEL_MIN=83; // prefix of the frame the page can decode
var lastFrameAt=0;

function setLink(kind){
 if(kind===LINK)return;
 LINK=kind;
 var d=$('link');
 if(!d)return;
 var map={live:['#3fb950','live 25 Hz'],poll:['#d29922','polling'],
          lost:['#f85149','no link'],off:['#484f58','idle']};
 var m=map[kind]||map.off;
 d.style.background=m[0];
 d.textContent=m[1];
 d.title=kind==='live'?'WebSocket on port '+WS_PORT
   :(kind==='poll'?'WebSocket unavailable: using the slower HTTP fallback':'');
}

function applyFrame(buf){
 var b=new Uint8Array(buf);
 if(b.length<TEL_MIN||b[0]!==1)return;
 TEL=parseFrame(b);
 lastFrameAt=Date.now();
 paintTelemetry();
}

function stopFallback(){
 if(FB_TIMER){clearTimeout(FB_TIMER);FB_TIMER=0;}
}
function startFallback(){
 if(FB_TIMER||document.hidden)return;
 var tick=function(){
  FB_TIMER=0;
  if(document.hidden)return;
  fetch('/api/telemetry',abortAfter(1500)).then(function(r){
   if(!r.ok)throw 0;
   return r.arrayBuffer();
  }).then(function(buf){
   applyFrame(buf);
   if(!WS_LIVE)setLink('poll');
  }).catch(function(){
   if(!WS_LIVE)setLink('lost');
  }).then(function(){
   if(!WS_LIVE&&!document.hidden)FB_TIMER=setTimeout(tick,250);
  });
 };
 FB_TIMER=setTimeout(tick,60);
}

function wsClose(){
 WS_LIVE=false;
 if(WS){try{WS.onclose=null;WS.close();}catch(e){}WS=null;}
 stopFallback();
}
/* Start whichever live link is possible, once, and only when there is nothing
   running: a socket, a healthy fallback, or a pending attempt. */
function ensureLink(){
 if(document.hidden||WS||WS_LIVE||FB_TIMER)return;
 if(!SEEDED)return; // wait for the first state read: it carries the port
 if(!WS_PORT){startFallback();setLink('poll');return;} // no socket advertised
 connectWs();
}
function connectWs(){
 if(document.hidden||WS)return;
 if(!WS_PORT){startFallback();setLink('poll');return;}
 var url='ws://'+location.hostname+':'+WS_PORT+'/ws';
 var sock;
 try{sock=new WebSocket(url);}catch(e){startFallback();setLink('poll');return;}
 sock.binaryType='arraybuffer';
 WS=sock;
 sock.onopen=function(){
  WS_LIVE=true;WS_TRY=0;setLink('live');stopFallback();
 };
 sock.onmessage=function(ev){
  if(ev.data instanceof ArrayBuffer){applyFrame(ev.data);}
  if(!WS_LIVE){WS_LIVE=true;setLink('live');stopFallback();}
 };
 sock.onclose=function(){
  WS=null;WS_LIVE=false;
  if(document.hidden)return;
  setLink('lost');
  startFallback();
  WS_TRY++;
  var back=Math.min(8000,400*Math.pow(2,Math.min(WS_TRY,4)));
  if(WS_TIMER)clearTimeout(WS_TIMER);
  WS_TIMER=setTimeout(function(){WS_TIMER=0;connectWs();},back);
 };
 sock.onerror=function(){try{sock.close();}catch(e){}};
}

/* Nothing arriving for two seconds means the link is stale even if the socket
   still claims to be open; treat it as lost so the fallback can take over. */
function linkWatchdog(){
 if(LINK==='live'&&Date.now()-lastFrameAt>2500){
  // Half-open socket: keep the fallback alive so the page stays truthful, and
  // try a fresh connection in the background.
  setLink('lost');
  wsClose();
  startFallback();
  if(!WS_TIMER)WS_TIMER=setTimeout(function(){WS_TIMER=0;ensureLink();},1200);
 }
}

document.addEventListener('visibilitychange',function(){
 if(document.hidden){wsClose();stopFallback();setLink('off');}
 else{ensureLink();} // whichever link applies: socket if possible, else polling
});

function poll(){
 dragWatchdog();
 if(busySocket()||editing())return;
 // A poll that lands just after a knob moved would overwrite what the user is
 // setting with a value the board has not seen yet.
 if(Date.now()-USER_TS<900)return;
 POLL_BUSY=true;
 fetchState()
  .then(function(s){POLL_BUSY=false;if(!editing())apply(s);})
  .catch(function(){POLL_BUSY=false;flashMsg('sync lost, reload');});
}
fetch('/api/state',{cache:'no-store'}).then(function(r){return r.json();}).then(apply).catch(function(){});
// Settings: rarely, they change only when someone touches something. The live
// numbers come from the socket.
setInterval(poll,2000);
setInterval(linkWatchdog,500);
// The socket port comes with the first state read (apply() connects), so the
// fallback only starts if that read says there is no socket to be had.
</script>
</body>
</html>)PAGE";

// printf-checked on purpose: the state JSON is written with positional
// arguments, and one missing value silently shifts every field after it (which
// made the whole control page show its neighbour's numbers). With this
// attribute the compiler verifies every call site instead.
#if defined(__GNUC__)
__attribute__((format(printf, 4, 5)))
#endif
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
  refreshPresets();
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
  server_.on("/api/telemetry", [this] { handleTelemetry(); });
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

  // One key per call, with its value right beside it. A single long format
  // string with positional arguments means one missing value silently shifts
  // every field after it - which is exactly what happened once: the page showed
  // each control's neighbour, the colour came from telemetry (so it danced) and
  // the threshold could not be moved. Named calls cannot shift.
  const auto kvu = [&](const char *key, unsigned v) {
    appendf(buf, sizeof(buf), n, "\"%s\":%u", key, v);
  };
  const auto kvd = [&](const char *key, int v) {
    appendf(buf, sizeof(buf), n, "\"%s\":%d", key, v);
  };
  const auto comma = [&]() { appendf(buf, sizeof(buf), n, ","); };

  appendf(buf, sizeof(buf), n, "{\"ssid\":\"%s\",\"ver\":\"%s\",", kSsid,
          kFirmwareVersion);
  // The page must not hardcode this: the port is the board's to decide, and a
  // page that guesses wrong silently loses the live link.
  kvu("wsPort", stream_ != nullptr && stream_->running() ? stream_->port() : 0u);
  comma();
  kvd("gainDb", (int)settings_->gainDb);
  comma();
  kvu("agc", (unsigned)settings_->agc);
  appendf(buf, sizeof(buf), n, ",\"strips\":[");

  for (size_t i = 0; i < kStripCount; i++) {
    const PhysicalStripSettings &p = settings_->phys[i];
    appendf(buf, sizeof(buf), n, "%s{", i == 0 ? "" : ",");
    kvu("ledCount", p.ledCount);
    comma();
    kvu("brightnessPct", p.brightnessPct);
    comma();
    kvu("layerCount", p.layerCount);
    comma();
    kvd("solo", (int)soloLayer(i));
    comma();
    kvu("calib", isCalibrating(i, nowMs) ? 1u : 0u);
    appendf(buf, sizeof(buf), n, ",\"layers\":[");

    for (size_t l = 0; l < kMaxLayersPerStrip; l++) {
      const LayerSettings &s = settings_->layer[i][l];
      appendf(buf, sizeof(buf), n, "%s{", l == 0 ? "" : ",");
      kvu("triggers", s.triggers);
      comma();
      kvu("effect", s.effect);
      comma();
      kvu("enabled", s.enabled);
      comma();
      kvu("intensityPct", s.intensityPct);
      comma();
      kvu("softnessPct", s.softnessPct);
      comma();
      kvu("centerHz", s.centerHz);
      comma();
      kvu("widthHz", s.widthHz);
      comma();
      kvu("thresholdPct", s.thresholdPct);
      comma();
      kvu("beatSensitivityPct", s.beatSensitivityPct);
      comma();
      kvu("motionThresholdPct", s.motionThresholdPct);
      comma();
      kvu("lfoPeriodMs", s.lfoPeriodMs);
      comma();
      kvu("attackMs", s.attackMs);
      comma();
      kvu("releaseMs", s.releaseMs);
      comma();
      kvu("retrigger", s.retrigger);
      comma();
      kvu("retriggerMs", s.retriggerMs);
      comma();
      kvu("blobLeds", s.blobLeds);
      comma();
      kvu("speed", s.speed);
      comma();
      kvu("fadeMs", s.fadeMs);
      comma();
      kvu("reverse", s.reverse);
      comma();
      kvu("loop", s.loop);
      appendf(buf, sizeof(buf), n, ",\"color\":\"%02x%02x%02x\"",
              (unsigned)s.r, (unsigned)s.g, (unsigned)s.b);
      appendf(buf, sizeof(buf), n, "}");
    }
    appendf(buf, sizeof(buf), n, "]}");
  }

  // Settings only. Everything that changes continuously - levels, dBFS, the
  // spectrum, per-layer pulses and tempi - travels in the telemetry frame
  // (WebSocket, or /api/telemetry), which is why this document can be polled
  // rarely and, in future, cached and rebuilt only when it changes.
  appendf(buf, sizeof(buf), n, "],");
  kvu("unsaved", unsaved_ ? 1u : 0u);
  appendf(buf, sizeof(buf), n, ",\"presets\":[");
  for (uint8_t i = 0; i < SettingsStore::kPresetCount; i++) {
    const PresetSummary &ps = preset_[i];
    appendf(buf, sizeof(buf), n,
            "%s{\"has\":%u,\"layers\":%u,\"cols\":\"", i == 0 ? "" : ",",
            ps.valid ? 1u : 0u, (unsigned)ps.layers);
    for (uint8_t c = 0; c < ps.colorCount; c++) {
      appendf(buf, sizeof(buf), n, "%s%02x%02x%02x", c == 0 ? "" : " ",
              (unsigned)ps.r[c], (unsigned)ps.g[c], (unsigned)ps.b[c]);
    }
    appendf(buf, sizeof(buf), n, "\"}");
  }
  appendf(buf, sizeof(buf), n, "]}");

  // Telemetry travels in a fixed-size binary frame. Keep the two in step:
  // src/telemetry.h lists the offsets.
  return String(buf);
}

void WebUi::handleRoot() {
  server_.sendHeader("Cache-Control", "no-store");
  server_.send_P(200, "text/html", kPage);
}

// The same binary frame the WebSocket pushes, for when a client cannot use one.
void WebUi::handleTelemetry() {
  if (stream_ == nullptr) {
    server_.send(503, "text/plain", "");
    return;
  }
  static uint8_t frame[TelemetryStream::kFrameSize];
  const size_t n = stream_->buildFrame(frame, sizeof(frame));
  server_.sendHeader("Cache-Control", "no-store");
  server_.send_P(200, "application/octet-stream", (const char *)frame,
                 n);
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

void WebUi::refreshPresets() {
  for (uint8_t i = 0; i < SettingsStore::kPresetCount; i++) {
    if (store_ == nullptr || !store_->presetSummary(i, preset_[i])) {
      preset_[i] = PresetSummary();
    }
  }
}

void WebUi::addLayer(size_t p) {
  PhysicalStripSettings &ps = settings_->phys[p];
  if (ps.layerCount >= kMaxLayersPerStrip) {
    return;
  }
  // A fresh slot, not a copy: adding a layer must not change the light that is
  // already on the strip. It arrives switched off, in its own colour and on its
  // own band, so switching it on gives a second beam rather than a clone.
  // Every slot from the new one upwards is reset, so a count raised by more than
  // one step can never expose a half-remembered layer.
  for (uint8_t l = (uint8_t)ps.layerCount; l < kMaxLayersPerStrip; l++) {
    settings_->layer[p][l] = defaultLayerFor(l);
  }
  edit_[p] = (uint8_t)ps.layerCount;
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
  // The freed slot goes back to the switched-off default, so a stale layer can
  // never sit there enabled if the count is raised again.
  settings_->layer[p][last] = defaultLayerFor(last);
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

    // Structure edits are explicit requests rather than arithmetic on the
    // count, so the page and the board cannot disagree about which slot is
    // being created or destroyed. A structural request owns the count: setting
    // it as well (older pages did) applied the change twice, once by the count
    // and once by the edit.
    const bool structural =
        server_.hasArg("layerAdd") || server_.hasArg("layerRemove");
    if (!structural) {
      applyArg("layerCount", &ps.layerCount);
    }
    if (server_.hasArg("layerAdd")) {
      addLayer(p);
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

  refreshPresets();
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

  refreshPresets();
  Serial.println("[web] factory reset");

  server_.sendHeader("Cache-Control", "no-store");
  server_.send(200, "application/json", stateJson());
}
