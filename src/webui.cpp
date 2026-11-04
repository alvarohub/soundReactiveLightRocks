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
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Light Rocks</title>
<style>
:root{color-scheme:dark}
*{box-sizing:border-box}
/* Right 1/6 of the viewport (16.7vw) is kept empty as a safe thumb-scroll gutter. */
body{margin:0;padding:14px 16.7vw 28px 12px;max-width:640px;background:#0f1115;color:#e6edf3;font:16px/1.45 system-ui,-apple-system,sans-serif;border-right:1px dashed #21262e}
h1{margin:0 0 12px;font-size:1.15rem;font-weight:600;display:flex;justify-content:space-between;align-items:baseline;gap:10px}
h1 span{font-size:.72rem;color:#8b949e;font-weight:400}
.card{background:#171b21;border:1px solid #262c36;border-radius:12px;padding:12px 14px;margin-bottom:10px}
.row{display:flex;justify-content:space-between;align-items:baseline;margin:9px 0 2px;font-size:.9rem;color:#9da7b3}
.row.first{margin-top:0}
.row b{color:#e6edf3;font-variant-numeric:tabular-nums;font-weight:600}
.row b.on{color:#7ee787}
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
.stepbtn{width:40px;height:34px;border-radius:8px;border:1px solid #3a424d;background:#21262e;color:#e6edf3;font-size:1rem;font-weight:700;flex:none;touch-action:manipulation}
.stepbtn:active{background:#30363d}
.seg{display:flex;gap:8px;margin-top:4px}
.seg button{flex:1;padding:11px 8px;font-size:.95rem;border-radius:9px;border:2px solid #3a424d;background:#21262e;color:#8b949e;font-weight:600}
.seg button.on{background:#17301e;border-color:#2ea043;color:#7ee787}
.seg button.off{background:#2b1d1d;border-color:#6b2b2b;color:#ff9d95}
.seg button.sel{outline:3px solid #58a6ff;outline-offset:2px}
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
.ptitle small{display:block;font-weight:400;font-size:.76rem;color:#8b949e;margin-top:2px}
</style>
</head>
<body>
<h1>Light Rocks <span id=host></span></h1>

<div class=card>
<div class=ptitle style="border-left-color:#3fb950">Microphone (global)<small>shared input for both strips</small></div>
<div class="row first"><span>Microphone gain</span><b id=gainDbv>-</b></div>
<input type=range id=gainDb min=0 max=18 step=6>
<div class=stats>
<div><span>Broadband</span><b id=sLvl>0%</b></div>
<div><span>dBFS</span><b id=sDb>-</b></div>
<div><span>Peak</span><b id=sPk>-</b></div>
</div>
</div>

<div class=card>
<div class="row first"><span>Select strip to edit</span><b id=tabhint style="color:#8b949e;font-weight:400;font-size:.76rem"></b></div>
<div class=seg id=pick></div>
<div class=ptitle id=ptitle style="margin-top:12px"></div>
<div class=row><span>Power</span><label class=sw><input type=checkbox id=pwr><i></i></label></div>
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

<div class=hint style="text-align:center">Short press A: test trigger &middot; hold A: Wi-Fi on/off &middot; B: mic gain</div>

<script>
var TRIGSPEC=[
{k:'centerHz',n:'Band centre',min:65,max:7600,st:5,u:' Hz'},
{k:'widthHz',n:'Band width',min:40,max:16000,st:10,u:' Hz'}
];
var DISPSPEC=[
{k:'brightnessPct',n:'Max brightness',min:1,max:100,st:1,u:'%'}
];
var ANIMSPEC=[
{k:'blobLeds',n:'Blob length',min:1,max:1000,st:1,u:' LEDs'},
{k:'speed',n:'Speed',min:1,max:500,st:1,u:' LEDs/s'},
{k:'fadeMs',n:'Fade time',min:50,max:10000,st:50,u:' ms'},
{k:'retriggerMs',n:'Retrigger gap',min:0,max:2000,st:10,u:' ms'}
];
var CALSPEC=[
{k:'ledCount',n:'LEDs on this strip',min:1,max:1000,st:1,u:''}
];
var ALLSPEC=TRIGSPEC.concat(DISPSPEC).concat(ANIMSPEC).concat(CALSPEC);
var SEL=0,ST=null,BUILT=false,SEEDED=false,PEND={},PTMR=0,REV=0,LOOP=1,RETRIG=1,CALIB=0;
var CUR_COL='#ff6a00',WHEEL_BG=null,WX=76,WY=76,WDRAG=false,TDRAG=-1,MSGTMR=0;
var INFLIGHT=false,POLL_BUSY=false;

function $(i){return document.getElementById(i);}
function colOf(s){return '#'+s.color;}
function nameOf(i){return 'Strip '+(i+1);}
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

function hasPending(){for(var k in PEND)return true;return false;}
function pending(){
 var q='',k;
 for(k in PEND){q+='&'+k+'='+encodeURIComponent(PEND[k]);}
 PEND={};
 return q;
}
function flush(){
 if(PTMR){clearTimeout(PTMR);PTMR=0;}
 if(INFLIGHT)return;
 var q=pending();
 if(!q)return;
 INFLIGHT=true;
 var done=function(){
  INFLIGHT=false;
  if(hasPending()&&!PTMR){PTMR=setTimeout(function(){PTMR=0;flush();},45);}
 };
 fetch('/api/params?strip='+SEL+q,{method:'POST'}).then(done,done);
}
function send(k,v,now){
 PEND[k]=v;
 if(ST&&ST.strips&&ST.strips[SEL]){
  ST.strips[SEL][k]=k==='color'?String(v):Number(v);
  ST.unsaved=1;
  live();
 }
 if(now){flush();return;}
 if(!PTMR&&!INFLIGHT){PTMR=setTimeout(function(){PTMR=0;flush();},55);}
}

function pickThr(idx,e,commit){
 var bar=$('bar0'),rect=bar.getBoundingClientRect();
 var pct=Math.round(((e.clientX-rect.left)/rect.width)*100);
 if(pct<0)pct=0;if(pct>100)pct=100;
 if(ST&&ST.strips[SEL])ST.strips[SEL].thresholdPct=pct;
 send('thresholdPct',pct,!!commit);
 refreshTitle();
}

function buildMeters(n){
 var p='';
 for(var i=0;i<n;i++){
  p+='<button id=sel'+i+'>Strip '+(i+1)+'</button>';
 }
 $('pick').innerHTML=p;
 for(var j=0;j<n;j++){
  (function(k){
   $('sel'+k).onclick=function(){
    if(CALIB){CALIB=0;send('calib',0,true);}
    flush();
    SEL=k;
    fillInputs();
    live();
   };
  })(j);
 }
}

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
 ctx.beginPath();
 ctx.arc(WX,WY,7,0,Math.PI*2);
 ctx.lineWidth=2.5;ctx.strokeStyle='#0f1115';ctx.stroke();
 ctx.beginPath();
 ctx.arc(WX,WY,5.5,0,Math.PI*2);
 ctx.lineWidth=2;ctx.strokeStyle='#fff';ctx.stroke();
}
function setColorHex(hex){
 CUR_COL=hex;
 $('cbox').style.background=hex;
 $('colv').textContent=hex;
 var r=parseInt(hex.substr(1,2),16)||0,g=parseInt(hex.substr(3,2),16)||0,b=parseInt(hex.substr(5,2),16)||0;
 var hs=rgbToHs(r,g,b),rad=76,ang=hs[0]*Math.PI/180,dist=hs[1]*(rad-3);
 WX=rad+Math.cos(ang)*dist;
 WY=rad+Math.sin(ang)*dist;
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
 if(ST&&ST.strips[SEL])ST.strips[SEL].color=CUR_COL.slice(1);
 send('color',CUR_COL.slice(1),!!commit);
}

function build(){
 var tbox=$('trigbox');
 tbox.innerHTML='<div class=ptitle id=ttitle>Triggering conditions<small>drag white mark on bar to set threshold</small></div>'
  +'<div class=row><span>Band level &amp; threshold</span><b id=mt0>0%</b></div>'
  +'<div class=bar id=bar0><i id=mb0></i><u id=mu0></u></div>';
 var bar=$('bar0');
 bar.addEventListener('pointerdown',function(e){
  e.preventDefault();
  TDRAG=SEL;
  if(bar.setPointerCapture){try{bar.setPointerCapture(e.pointerId);}catch(_){}}
  pickThr(SEL,e,false);
 });
 bar.addEventListener('pointermove',function(e){
  if(TDRAG<0)return;
  e.preventDefault();
  pickThr(SEL,e,false);
 });
 function endThr(e){if(TDRAG<0)return;TDRAG=-1;pickThr(SEL,e,true);}
 bar.addEventListener('pointerup',endThr);
 bar.addEventListener('pointercancel',function(){TDRAG=-1;flush();});
 TRIGSPEC.forEach(function(s){
  tbox.insertAdjacentHTML('beforeend',
   '<div class=row><span>'+s.n+'</span><b id="'+s.k+'v">-</b></div>'
   +'<input type=range id="'+s.k+'" min='+s.min+' max='+s.max+' step='+s.st+'>');
  var el=$(s.k);
  el.addEventListener('input',function(){labels();send(s.k,el.value,false);});
 });

 var dbox=$('dispbox');
 dbox.innerHTML='<div class=ptitle id=dtitle>Display parameters<small>colour &amp; max brightness</small></div>'
  +'<div class=row><span>Colour (drag wheel for real-time)</span><b id=colv></b></div>'
  +'<div class=cwrap><canvas id=wheel width=152 height=152></canvas><div class=cbox id=cbox></div></div>';
 initWheel();
 var wh=$('wheel');
 wh.addEventListener('pointerdown',function(e){
  e.preventDefault();
  WDRAG=true;
  if(wh.setPointerCapture){try{wh.setPointerCapture(e.pointerId);}catch(_){}}
  pickWheel(e,false);
 });
 wh.addEventListener('pointermove',function(e){
  if(!WDRAG)return;
  e.preventDefault();
  pickWheel(e,false);
 });
 function endWheel(e){if(!WDRAG)return;WDRAG=false;pickWheel(e,true);}
 wh.addEventListener('pointerup',endWheel);
 wh.addEventListener('pointercancel',function(){WDRAG=false;flush();});
 DISPSPEC.forEach(function(s){
  dbox.insertAdjacentHTML('beforeend',
   '<div class=row><span>'+s.n+'</span><b id="'+s.k+'v">-</b></div>'
   +'<input type=range id="'+s.k+'" min='+s.min+' max='+s.max+' step='+s.st+'>');
  var el=$(s.k);
  el.addEventListener('input',function(){labels();send(s.k,el.value,false);});
 });

 var abox=$('animbox');
 abox.innerHTML='<div class=ptitle id=atitle>Animation parameters<small>trajectory, speed, fade &amp; retrigger</small></div>'
  +'<div class=row><span>Motion</span><b></b></div>'
  +'<div class=seg><button id=lp1>Loop (ring)</button><button id=lp0>One-shot</button></div>'
  +'<div class=row><span>Direction</span><b></b></div>'
  +'<div class=seg><button id=d0>Forward</button><button id=d1>Reverse</button><button id=d2>Ping-pong</button></div>';
 ANIMSPEC.forEach(function(s){
  if(s.k==='retriggerMs'){
   abox.insertAdjacentHTML('beforeend',
    '<div class=row><span>'+s.n+'</span><b id="'+s.k+'v">-</b></div>'
    +'<div class=rline><input type=range id="'+s.k+'" min='+s.min+' max='+s.max+' step='+s.st+'><button type=button id=rtbtn class=tbtn>Retrig ON</button></div>');
  }else{
   abox.insertAdjacentHTML('beforeend',
    '<div class=row><span>'+s.n+'</span><b id="'+s.k+'v">-</b></div>'
    +'<input type=range id="'+s.k+'" min='+s.min+' max='+s.max+' step='+s.st+'>');
  }
  var el=$(s.k);
  el.addEventListener('input',function(){labels();send(s.k,el.value,false);});
 });
 $('rtbtn').onclick=function(){
  RETRIG=RETRIG?0:1;
  setRetrig();
  labels();
  send('retrigger',RETRIG,true);
 };

 var cbox=$('calbox'),cs=CALSPEC[0];
 cbox.innerHTML='<div class=ptitle id=ctitle>Strip calibration<small id=calhint>lights only the end LED while adjusting</small></div>'
  +'<div class=row><span>'+cs.n+'</span><b id="ledCountv">-</b></div>'
  +'<input type=range id="ledCount" min='+cs.min+' max='+cs.max+' step='+cs.st+'>'
  +'<div class=rline style="margin-top:6px">'
  +'<button type=button id=lcdec10 class=stepbtn>-10</button>'
  +'<button type=button id=lcdec class=stepbtn>&larr;</button>'
  +'<button type=button id=lcinc class=stepbtn>&rarr;</button>'
  +'<button type=button id=lcinc10 class=stepbtn>+10</button>'
  +'<button type=button id=calbtn class=tbtn style="margin-left:auto">Calibrate</button>'
  +'</div>';
 $('ledCount').addEventListener('input',function(){labels();send('ledCount',$('ledCount').value,false);});
 function stepLeds(delta){
  var el=$('ledCount'),v=Math.max(1,Math.min(1000,(+el.value)+delta));
  el.value=v;
  labels();
  send('ledCount',v,true);
 }
 $('lcdec10').onclick=function(){stepLeds(-10);};
 $('lcdec').onclick=function(){stepLeds(-1);};
 $('lcinc').onclick=function(){stepLeds(1);};
 $('lcinc10').onclick=function(){stepLeds(10);};
 $('calbtn').onclick=function(){
  CALIB=CALIB?0:1;
  setCalib();
  send('calib',CALIB,true);
 };
 $('pwr').addEventListener('change',function(){send('enabled',$('pwr').checked?1:0,true);});
 $('lp1').onclick=function(){LOOP=1;setLoop();refreshTitle();send('loop',1,true);};
 $('lp0').onclick=function(){LOOP=0;setLoop();refreshTitle();send('loop',0,true);};
 $('d0').onclick=function(){REV=0;setDir();refreshTitle();send('reverse',0,true);};
 $('d1').onclick=function(){REV=1;setDir();refreshTitle();send('reverse',1,true);};
 $('d2').onclick=function(){REV=2;setDir();refreshTitle();send('reverse',2,true);};
 $('pwr').addEventListener('change',refreshTitle);
}

function dirName(r){return r===2?'ping-pong':(r===1?'reverse':'forward');}

function refreshTitle(){
 var on=$('pwr').checked;
 var c=CUR_COL;
 var thr=ST&&ST.strips&&ST.strips[SEL]?ST.strips[SEL].thresholdPct:35;
 ['ptitle','ttitle','dtitle','atitle','ctitle'].forEach(function(id){
  if($(id))$(id).style.borderLeftColor=c;
 });
 $('colv').textContent=c;
 $('ptitle').innerHTML=nameOf(SEL)+'<small>'+(on?'powered on':'powered off')
  +' &middot; '+(LOOP?'loop':'one-shot')+' '+dirName(REV)
  +' &middot; '+thr+'% thr'
  +' &middot; '+$('speed').value+' LED/s'
  +' &middot; fade '+$('fadeMs').value+' ms</small>';
}

function labels(){
 ALLSPEC.forEach(function(p){
  if(p.k==='centerHz'||p.k==='widthHz'||p.k==='retriggerMs')return;
  $(p.k+'v').textContent=$(p.k).value+p.u;
 });
 $('retriggerMsv').textContent=RETRIG?($('retriggerMs').value+' ms'):'no retrigger (1 blob)';
 var c=+$('centerHz').value, w=+$('widthHz').value;
 var lo=Math.max(65,Math.round(c-w/2)), hi=Math.min(7600,Math.round(c+w/2));
 $('centerHzv').textContent=c+' Hz';
 $('widthHzv').textContent=w+' Hz  ('+lo+'-'+hi+')';
 refreshTitle();
}

function setDir(){
 $('d0').className=REV===0?'sel':'';
 $('d1').className=REV===1?'sel':'';
 $('d2').className=REV===2?'sel':'';
}
function setLoop(){$('lp1').className=LOOP?'sel':'';$('lp0').className=LOOP?'':'sel';}
function setRetrig(){
 var b=$('rtbtn'),sl=$('retriggerMs');
 b.className='tbtn '+(RETRIG?'on':'off');
 b.textContent=RETRIG?'Retrig ON':'Single';
 sl.disabled=!RETRIG;
 sl.style.opacity=RETRIG?'1':'.35';
}
function setCalib(){
 var b=$('calbtn'),h=$('calhint');
 b.className='tbtn '+(CALIB?'on':'');
 b.textContent=CALIB?'Exit calib':'Calibrate';
 if(h)h.textContent=CALIB?'CALIBRATING: only end LED is lit (tap Exit calib or adjust any other slider to resume)':'lights only the end LED while adjusting';
}

/* Only called when the values come from the board, never while dragging: a poll
   in the middle of a drag would otherwise fight the user's finger. */
function fillInputs(){
 if(!ST||!BUILT)return;
 var s=ST.strips[SEL];
 ALLSPEC.forEach(function(p){$(p.k).value=s[p.k];});
 $('pwr').checked=!!s.enabled;
 $('gainDb').value=ST.gainDb;
 REV=s.reverse||0;
 LOOP=s.loop?1:0;
 RETRIG=s.retrigger!==undefined?(s.retrigger?1:0):1;
 CALIB=s.calib?1:0;
 setDir();
 setLoop();
 setRetrig();
 setCalib();
 setColorHex(colOf(s));
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
  var si=ST.strips[i];
  $('sel'+i).className=(SEL===i?'sel ':'')+(si.enabled?'on':'off');
 }
 var s=ST.strips[SEL];
 if(s&&BUILT){
  var c=colOf(s);
  $('mb0').style.width=s.levelPct+'%';
  $('mb0').style.background=c;
  $('mu0').style.left='calc('+s.thresholdPct+'% - 2px)';
  $('mt0').textContent=s.enabled
   ?('thr '+s.thresholdPct+'% · lvl '+s.levelPct+'% · '+s.blobs+' blob'+(s.blobs===1?'':'s'))
   :'switched off';
  CALIB=s.calib?1:0;
  setCalib();
 }
 $('sLvl').textContent=ST.levelPct+'%';
 $('sDb').textContent=ST.dbfs;
 $('sPk').textContent=ST.peakHz+' Hz';
 if(!MSGTMR)$('pmsg').textContent=ST.unsaved?'auto-saving…':'auto-saved';
 $('gainDbv').textContent=ST.gainDb+' dB';
 $('ld1').disabled=!ST.hasPreset0;
 $('ld2').disabled=!ST.hasPreset1;
 $('ld3').disabled=!ST.hasPreset2;
}

function apply(s){
 ST=s;
 if(!BUILT&&ST.strips.length){
  BUILT=true;
  buildMeters(ST.strips.length);
  build();
 }
 if(!SEEDED){SEEDED=true;fillInputs();}
 live();
}
function post(u,refresh){
 return fetch(u,{method:'POST'}).then(function(r){return r.json();}).then(function(s){
  apply(s);
  if(refresh)fillInputs();
  return s;
 });
}
$('host').textContent=location.host;
$('gainDb').addEventListener('input',function(){
 $('gainDbv').textContent=$('gainDb').value+' dB';
});
$('gainDb').addEventListener('change',function(){
 fetch('/api/params?gainDb='+$('gainDb').value,{method:'POST'});
});
[1,2,3].forEach(function(n){
 $('sp'+n).onclick=function(){
  flush();
  post('/api/preset?slot='+n+'&action=save',false).then(function(){flashMsg('Preset '+n+' saved');});
 };
 $('ld'+n).onclick=function(){
  flush();
  post('/api/preset?slot='+n+'&action=load',true).then(function(){flashMsg('Preset '+n+' loaded');});
 };
});
$('rst').onclick=function(){
 if(confirm('Reset current parameters to factory defaults? (Saved presets are kept.)')){
  post('/api/reset',true).then(function(){flashMsg('Factory defaults restored');});
 }
};
function poll(){
 if(INFLIGHT||POLL_BUSY||WDRAG||TDRAG>=0||hasPending())return;
 POLL_BUSY=true;
 fetch('/api/state')
  .then(function(r){return r.json();})
  .then(function(s){POLL_BUSY=false;if(!WDRAG&&TDRAG<0&&!hasPending())apply(s);})
  .catch(function(){POLL_BUSY=false;});
}
poll();
setInterval(poll,400);
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
  server_.on("/api/preset", HTTP_POST, [this] { handlePreset(); });
  server_.on("/api/reset", HTTP_POST, [this] { handleReset(); });
  server_.onNotFound([this] { server_.send(204, "text/plain", ""); });
}

String WebUi::ip() const {
  return enabled_ ? WiFi.softAPIP().toString() : String("0.0.0.0");
}

String WebUi::stateJson() const {
  // A short buffer would silently truncate the JSON and break the page, so this
  // is sized for the worst case (see kStateJsonMaxBytes).
  char buf[kStateJsonMaxBytes];
  size_t n = 0;
  buf[0] = '\0';
  const uint32_t nowMs = millis();

  appendf(buf, sizeof(buf), n, "{\"gainDb\":%u,\"strips\":[",
          (unsigned)settings_->gainDb);

  for (size_t i = 0; i < kStripCount; i++) {
    const StripSettings &s = settings_->strip[i];
    appendf(buf, sizeof(buf), n,
            "%s{\"mode\":%u,\"enabled\":%u,\"brightnessPct\":%u,\"ledCount\":%u,"
            "\"centerHz\":%u,\"widthHz\":%u,\"thresholdPct\":%u,\"retrigger\":%u,"
            "\"retriggerMs\":%u,\"blobLeds\":%u,\"speed\":%u,\"fadeMs\":%u,"
            "\"reverse\":%u,\"loop\":%u,\"color\":\"%02x%02x%02x\","
            "\"levelPct\":%u,\"brightPct\":%u,\"blobs\":%u,\"calib\":%u}",
            i == 0 ? "" : ",", (unsigned)s.mode, (unsigned)s.enabled,
            (unsigned)s.brightnessPct, (unsigned)s.ledCount,
            (unsigned)s.centerHz, (unsigned)s.widthHz,
            (unsigned)s.thresholdPct, (unsigned)s.retrigger,
            (unsigned)s.retriggerMs, (unsigned)s.blobLeds, (unsigned)s.speed,
            (unsigned)s.fadeMs, (unsigned)s.reverse, (unsigned)s.loop,
            (unsigned)s.r, (unsigned)s.g, (unsigned)s.b,
            (unsigned)tele_.strip[i].levelPct,
            (unsigned)tele_.strip[i].brightPct, (unsigned)tele_.strip[i].blobs,
            isCalibrating(i, nowMs) ? 1u : 0u);
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

void WebUi::applyArg(const char *name, uint8_t *dst) {
  if (!server_.hasArg(name)) {
    return;
  }
  *dst = (uint8_t)constrain(server_.arg(name).toInt(), 0L, 255L);
}

void WebUi::applyArg(const char *name, uint16_t *dst) {
  if (!server_.hasArg(name)) {
    return;
  }
  *dst = (uint16_t)constrain(server_.arg(name).toInt(), 0L, 65535L);
}

void WebUi::handleParams() {
  // Strip parameters are addressed with ?strip=<index>; gain is global.
  const int index = server_.hasArg("strip") ? server_.arg("strip").toInt() : -1;
  if (index >= 0 && index < (int)kStripCount) {
    StripSettings &s = settings_->strip[index];
    applyArg("mode", &s.mode);
    applyArg("enabled", &s.enabled);
    applyArg("brightnessPct", &s.brightnessPct);
    applyArg("ledCount", &s.ledCount);
    applyArg("centerHz", &s.centerHz);
    applyArg("widthHz", &s.widthHz);
    applyArg("thresholdPct", &s.thresholdPct);
    applyArg("retrigger", &s.retrigger);
    applyArg("retriggerMs", &s.retriggerMs);
    applyArg("blobLeds", &s.blobLeds);
    applyArg("speed", &s.speed);
    applyArg("fadeMs", &s.fadeMs);
    applyArg("reverse", &s.reverse);
    applyArg("loop", &s.loop);

    // Changing ledCount automatically activates the single-LED calibration dot
    // for 3.5 seconds so the user can see the physical tip of the strip.
    if (server_.hasArg("ledCount")) {
      calibUntilMs_[index] = millis() + 3500;
    } else if (!server_.hasArg("calib")) {
      // Touching any other parameter exits calibration mode immediately.
      calibPinned_[index] = false;
      calibUntilMs_[index] = 0;
    }
    if (server_.hasArg("calib")) {
      const bool on = server_.arg("calib").toInt() != 0;
      calibPinned_[index] = on;
      if (!on) {
        calibUntilMs_[index] = 0;
      }
    }

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
  applyArg("gainDb", &settings_->gainDb);
  clampSettings(*settings_);
  changed_ = true;
  unsaved_ = true;

  // Lightweight 204 reply: avoids heap String allocation and 650-byte JSON
  // responses on every 50 ms slider / colour-wheel drag tick.
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
