#include "page_assets.hpp"

namespace setup_page::assets {

// Without script (no "js" class on <html>) every view shows, one after
// another, and fields that script would reveal on demand are just there.
const char* const kStyle = R"CSS(
:root{--bg:#eef1f5;--card:#fff;--fg:#17202b;--mut:#5f6c7d;--line:#dde3ea;--acc:#1e4f8a;--acc-fg:#fff;--acc-soft:#e3ecf7;--good:#23894a;--good-soft:#e2f3e8;--warn:#955f00;--warn-soft:#fff3d6;--bad:#c43535;--bad-soft:#fde8e8;--i-cal:#1e4f8a;--i-sun:#c27f0e;--i-img:#7a56a8;--i-disp:#2b8a64;--i-clock:#c4622d;--i-wifi:#3d7fc2;--i-gear:#5f6b7a;color-scheme:light}
@media (prefers-color-scheme:dark){:root{--bg:#0e1319;--card:#18202a;--fg:#e6eaef;--mut:#97a3b2;--line:#2a3440;--acc:#79a8e6;--acc-fg:#0e1319;--acc-soft:#1c2c42;--good:#5cc985;--good-soft:#163222;--warn:#e5b04a;--warn-soft:#3a2f12;--bad:#ef7676;--bad-soft:#3d1c1c;--i-cal:#2f63a6;--i-sun:#a86d0c;--i-img:#7350a0;--i-disp:#24785a;--i-clock:#ad5528;--i-wifi:#3572b0;--i-gear:#566270;color-scheme:dark}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.5 -apple-system,"PingFang TC","Noto Sans TC","Microsoft JhengHei",system-ui,sans-serif;-webkit-text-size-adjust:100%}
a{color:var(--acc)}
button,input,select{font:inherit;color:inherit}
:focus-visible{outline:2px solid var(--acc);outline-offset:2px}
[hidden]{display:none!important}
.in{max-width:560px;margin:0 auto;padding-left:max(16px,env(safe-area-inset-left));padding-right:max(16px,env(safe-area-inset-right))}
.bar{position:sticky;top:0;z-index:5;background:var(--card);border-bottom:1px solid var(--line)}
.bar .in{display:flex;align-items:center;gap:10px;min-height:56px;padding-top:8px;padding-bottom:8px}
.bar .sub,body.subview .bar .home{display:none}body.subview .bar .sub{display:flex}
.logo{width:32px;height:32px;flex:none}
.bt{flex:1;min-width:0}.bt b{display:block;font-size:17px;line-height:1.25}.bt small{color:var(--mut);font-size:12.5px}
.dot{width:10px;height:10px;border-radius:50%;background:var(--good);flex:none}.dot.w{background:var(--warn)}
.back{border:0;background:none;color:var(--acc);padding:6px 0;display:flex;align-items:center;gap:2px;cursor:pointer;min-width:84px;font-size:15px}
.back svg{width:18px;height:18px}
.vt{flex:1;text-align:center;font-weight:650;font-size:16px}.sp84{min-width:84px}
main{padding-top:12px;padding-bottom:130px}
.js .view:not(.on){display:none}
.vh{font-size:18px;margin:26px 4px 10px}.js .vh{display:none}
.js .later{display:none!important}
html:not(.js) .jsonly{display:none!important}
.js .jsvh{position:absolute;opacity:0;pointer-events:none;width:1px;height:1px;overflow:hidden}
.ban{display:flex;gap:10px;align-items:flex-start;background:var(--warn-soft);border-radius:14px;padding:11px 13px;margin-bottom:12px;font-size:14px;color:var(--fg);text-decoration:none}
.ban.e{background:var(--bad-soft)}
.ban>svg{width:18px;height:18px;flex:none;margin-top:2px;color:var(--warn)}.ban.e>svg{color:var(--bad)}
.ban ul{margin:4px 0 0;padding-left:18px}.ban .go{color:var(--acc);white-space:nowrap}
.tiles{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:6px}
.tile{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:10px 13px;min-width:0}
.tile .tl{color:var(--mut);font-size:13px;display:flex;align-items:center;gap:6px}
.tile .tl i{width:8px;height:8px;border-radius:50%;background:var(--good)}.tile .tl i.w{background:var(--warn)}.tile .tl i.n{background:var(--line)}
.tile b{display:block;font-size:26px;font-weight:650;line-height:1.25;font-variant-numeric:tabular-nums}
.tile b span{font-size:14px;font-weight:400;color:var(--mut);margin-left:3px}
.tile .ts{font-size:12.5px;color:var(--mut)}.tile .ts.w{color:var(--warn)}
.sec{color:var(--mut);font-size:12.5px;margin:18px 6px 6px;letter-spacing:.06em}
.list{background:var(--card);border:1px solid var(--line);border-radius:14px;overflow:hidden}
.row{display:flex;align-items:center;gap:12px;padding:11px 14px;border-top:1px solid var(--line);text-decoration:none;color:var(--fg);min-height:54px}
.row:first-child{border-top:0}.row:active{background:var(--acc-soft)}
.ico{width:30px;height:30px;border-radius:8px;display:grid;place-items:center;flex:none;color:#fff}.ico svg{width:18px;height:18px}
.row .lb{flex:1;min-width:0}
.row .val{color:var(--mut);font-size:14px;display:flex;align-items:center;gap:6px;min-width:0;max-width:55%;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.wd,.ch{width:8px;height:8px;border-radius:50%;flex:none;display:inline-block}.wd{background:var(--warn)}.ch{background:var(--acc)}
.chev{width:16px;height:16px;color:var(--mut);flex:none}
.foot{color:var(--mut);font-size:12.5px;text-align:center;margin-top:18px}
.intro{color:var(--mut);font-size:13.5px;margin:2px 4px 12px}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:4px 14px;margin-bottom:12px}
.card>h2{font-size:13px;font-weight:600;color:var(--mut);margin:12px 0 2px;letter-spacing:.04em;display:flex;align-items:center;gap:8px}
.card>h2 span{flex:1}.card>h2 small{font-weight:400}
.f{display:flex;align-items:center;gap:12px;padding:12px 0;border-top:1px solid var(--line);position:relative}
.card>h2+.f,.card>.f:first-child,.card>h2+.later+.f{border-top:0}
.f.col{display:block}
.f .lb,.f label.lb{flex:1;min-width:0;display:block}
.f .lb small,.help{display:block;color:var(--mut);font-size:12.5px;line-height:1.45;font-weight:400}
.help{margin:6px 0 0}
.f.col>.lb{margin-bottom:8px}
.f.dirty::before{content:"";position:absolute;left:-14px;top:10px;bottom:10px;width:3px;border-radius:0 3px 3px 0;background:var(--acc)}
input.t,select{width:100%;background:var(--bg);border:1px solid var(--line);border-radius:10px;padding:9px 11px;font-size:16px}
select.s{width:auto;min-width:104px}
input.t::placeholder{color:var(--mut)}
label.fl{display:block;font-size:13px;color:var(--mut);margin:0 0 4px}
.sw{appearance:none;-webkit-appearance:none;width:48px;height:28px;border-radius:14px;background:var(--line);position:relative;margin:0;flex:none;cursor:pointer;transition:background .15s}
.sw::after{content:"";position:absolute;top:3px;left:3px;width:22px;height:22px;border-radius:50%;background:#fff;box-shadow:0 1px 3px rgba(0,0,0,.25);transition:left .15s}
.sw:checked{background:var(--acc)}.sw:checked::after{left:23px}
.seg{display:flex;border:1px solid var(--line);border-radius:10px;overflow:hidden;background:var(--bg)}
.seg.in-row{display:inline-flex;flex:none}
.seg label{flex:1;position:relative;border-left:1px solid var(--line);cursor:pointer}.seg label:first-child{border-left:0}
.seg input{position:absolute;opacity:0;pointer-events:none}
.seg span{display:block;text-align:center;padding:8px 12px;font-size:14px;white-space:nowrap}
.seg.four span{padding:8px 2px;font-size:13.5px}
.seg input:checked+span{background:var(--acc);color:var(--acc-fg);font-weight:600}
.seg input:focus-visible+span{outline:2px solid var(--acc);outline-offset:-3px}
.btn{display:inline-block;border:1px solid var(--line);background:var(--bg);border-radius:10px;padding:8px 13px;font-size:14px;cursor:pointer;white-space:nowrap;text-decoration:none;color:var(--fg);text-align:center}
.btn.pri{background:var(--acc);border-color:var(--acc);color:var(--acc-fg);font-weight:600}
.btn:disabled{opacity:.45;cursor:default}
.btn.dan{color:var(--bad);border-color:var(--bad);background:none}.btn.dan.solid{background:var(--bad);color:#fff}
.btn.add{width:100%;border-style:dashed;color:var(--acc);background:none;padding:10px}
.btn.wide{width:100%;padding:12px;font-size:16px}
.chip{font-size:12px;padding:2px 9px;border-radius:99px;white-space:nowrap;flex:none;background:var(--bg);color:var(--mut);border:1px solid var(--line)}
.chip.ok{background:var(--good-soft);color:var(--good);border-color:transparent}.chip.w{background:var(--warn-soft);color:var(--warn);border-color:transparent}
.num{width:28px;height:28px;border-radius:8px;border:1px solid var(--line);display:grid;place-items:center;color:var(--mut);font-size:13px;flex:none;background:var(--bg)}
.hd{display:flex;align-items:center;gap:12px}
.host{display:block;color:var(--mut);font-size:12.5px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.url{margin-top:10px}
.acts{display:flex;gap:8px;margin-top:10px;flex-wrap:wrap}.acts:empty{display:none}
.rm{display:inline-flex;gap:6px;align-items:center;margin-top:8px;font-size:14px}
details.howto{margin:0 4px 14px;font-size:13.5px;color:var(--mut)}details.howto summary{color:var(--acc);cursor:pointer}details.howto ol{margin:6px 0 0;padding-left:20px}
.savebar{position:fixed;left:0;right:0;bottom:0;z-index:6;background:var(--card);border-top:1px solid var(--line);transition:transform .18s ease-out}
.js .savebar:not(.show){transform:translateY(110%)}
.savebar .in{display:flex;align-items:center;gap:12px;padding-top:10px;padding-bottom:calc(12px + env(safe-area-inset-bottom))}
.savebar .msg{flex:1;font-size:13.5px;color:var(--mut);min-width:0}.savebar .msg b{color:var(--fg);margin-right:2px}
.undo{border:0;background:none;color:var(--acc);font-size:13.5px;padding:4px;cursor:pointer}
.savebar.ap .in{display:block}.savebar.ap .help{margin:0 0 8px;text-align:center}
.steps{display:flex;gap:8px;margin:0 0 14px}
.steps span{flex:1;display:flex;align-items:center;gap:8px;font-size:13px;color:var(--mut);background:var(--card);border:1px solid var(--line);border-radius:10px;padding:8px 10px;min-width:0}
.steps b{width:22px;height:22px;border-radius:50%;display:grid;place-items:center;font-size:12px;background:var(--bg);border:1px solid var(--line);flex:none}
.steps .on{color:var(--fg);border-color:var(--acc)}.steps .on b{background:var(--acc);border-color:var(--acc);color:var(--acc-fg)}
.net{display:flex;align-items:center;gap:12px;padding:12px 0;border:0;border-top:1px solid var(--line);width:100%;background:none;text-align:left;cursor:pointer;min-height:52px}
.card>h2+.net{border-top:0}
.net .lb{flex:1;min-width:0;overflow-wrap:anywhere}.net .lb small{display:block;color:var(--mut);font-size:12.5px}
.net .tick{width:18px;flex:none;color:var(--acc);font-weight:700;text-align:center}.net.on .tick::before{content:"✓"}
.net svg.lock{width:14px;height:14px;color:var(--mut);flex:none}
.bars{display:flex;align-items:flex-end;gap:2px;height:14px;flex:none}
.bars i{width:3px;border-radius:1px;background:var(--line)}.bars i.on{background:var(--fg)}
.bars i:nth-child(1){height:5px}.bars i:nth-child(2){height:8px}.bars i:nth-child(3){height:11px}.bars i:nth-child(4){height:14px}
.netform{padding:0 0 12px 30px;display:grid;gap:10px}
html:not(.js) .netform{padding-left:0;border-top:1px solid var(--line);padding-top:12px}
.pw{display:flex;gap:8px}.pw input{flex:1;min-width:0}
.center{text-align:center}
.quiet{color:var(--mut);font-size:13px}
.done{text-align:center;padding:22px 4px 4px}.done h1{font-size:21px;margin:12px 0 6px}.done>p{color:var(--mut);margin:0 0 18px}
.qr{width:120px;height:120px;margin:0 auto;border-radius:12px;background:var(--card);border:1px solid var(--line);display:grid;place-items:center}.qr svg{width:84px;height:84px}
ol.next{text-align:left;margin:0;padding:0;list-style:none;display:grid;gap:10px;counter-reset:n}
ol.next li{display:flex;gap:12px;align-items:flex-start;background:var(--card);border:1px solid var(--line);border-radius:12px;padding:12px;counter-increment:n}
ol.next li::before{content:counter(n);width:24px;height:24px;border-radius:50%;background:var(--acc);color:var(--acc-fg);display:grid;place-items:center;font-size:13px;font-weight:700;flex:none}
.page h1{font-size:21px;margin:8px 4px 10px}
.thumbs{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin:8px 0 12px}
.thumb canvas{width:100%;display:block;border-radius:8px;border:1px solid var(--line);background:var(--bg)}
.thumb.other canvas{opacity:.35}.thumb .shape{font-size:12.5px;color:var(--mut);margin:4px 0}.thumb .btn{width:100%}
#preview{width:100%;border-radius:8px;border:1px solid var(--line);touch-action:none;cursor:grab;display:block;margin:10px 0}
input[type=range]{width:100%;accent-color:var(--acc)}
.file{position:relative;overflow:hidden;display:block;margin:10px 0 12px}.file input{position:absolute;inset:0;opacity:0;cursor:pointer}
.file:has(input:disabled){opacity:.45}
#status{font-weight:600;color:var(--fg);font-size:13.5px}
)CSS";

const char* const kIcons = R"SVG(<svg width="0" height="0" style="position:absolute" aria-hidden="true">
<symbol id="i-chev" viewBox="0 0 16 16"><path d="M6 3l5 5-5 5" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/></symbol>
<symbol id="i-back" viewBox="0 0 18 18"><path d="M11 3L5 9l6 6" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></symbol>
<symbol id="i-cal" viewBox="0 0 18 18"><rect x="2.5" y="3.5" width="13" height="12" rx="2" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="M2.5 7.5h13M6 2v3M12 2v3" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"/></symbol>
<symbol id="i-sun" viewBox="0 0 18 18"><circle cx="9" cy="9" r="3.4" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="M9 1.5v2M9 14.5v2M1.5 9h2M14.5 9h2M3.7 3.7l1.4 1.4M12.9 12.9l1.4 1.4M3.7 14.3l1.4-1.4M12.9 5.1l1.4-1.4" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"/></symbol>
<symbol id="i-img" viewBox="0 0 18 18"><rect x="2" y="3.5" width="14" height="11" rx="1.8" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="M3 13l4-4.5 3 3 2-2 3 3.5" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round"/></symbol>
<symbol id="i-disp" viewBox="0 0 18 18"><rect x="2" y="4" width="14" height="9" rx="1.5" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="M6 15.5h6" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"/></symbol>
<symbol id="i-clock" viewBox="0 0 18 18"><circle cx="9" cy="9.5" r="6.5" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="M9 6v3.8l2.5 1.6" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"/></symbol>
<symbol id="i-wifi" viewBox="0 0 18 18"><path d="M2 7a10 10 0 0 1 14 0M4.5 9.8a6.5 6.5 0 0 1 9 0M7 12.5a3 3 0 0 1 4 0" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"/><circle cx="9" cy="14.8" r="1" fill="currentColor"/></symbol>
<symbol id="i-gear" viewBox="0 0 18 18"><circle cx="9" cy="9" r="2.6" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="M9 1.8v2.1M9 14.1v2.1M1.8 9h2.1M14.1 9h2.1M3.9 3.9l1.5 1.5M12.6 12.6l1.5 1.5M3.9 14.1l1.5-1.5M12.6 5.4l1.5-1.5" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"/></symbol>
<symbol id="i-warn" viewBox="0 0 18 18"><path d="M9 2l7.5 13h-15z" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round"/><path d="M9 7v3.5" stroke="currentColor" stroke-width="1.7" stroke-linecap="round"/><circle cx="9" cy="12.8" r="1" fill="currentColor"/></symbol>
<symbol id="i-lock" viewBox="0 0 14 14"><rect x="2.5" y="6" width="9" height="6.5" rx="1.5" fill="currentColor"/><path d="M4.5 6V4.5a2.5 2.5 0 0 1 5 0V6" fill="none" stroke="currentColor" stroke-width="1.5"/></symbol>
<symbol id="i-qr" viewBox="0 0 21 21"><path fill="currentColor" d="M0 0h7v7H0zM2 2v3h3V2zM14 0h7v7h-7zM16 2v3h3V2zM0 14h7v7H0zM2 16v3h3v-3zM9 0h2v2H9zM9 4h3v3H9zM9 9h3v2H9zM14 9h2v3h-2zM18 9h3v2h-3zM9 13h2v3H9zM13 14h3v2h-3zM18 13h3v3h-3zM12 18h2v3h-2zM16 18h5v3h-5zM9 18h2v3H9z"/></symbol>
</svg>
)SVG";

const char* const kLogo = R"SVG(<svg class="logo" viewBox="0 0 32 32" aria-hidden="true"><rect x="2" y="5" width="28" height="22" rx="4" fill="var(--acc)"/><rect x="5" y="8.5" width="22" height="15" rx="1.5" fill="var(--card)"/><path d="M6.5 22l6-7.5 4 4.2 3-3.2 6 6.5z" fill="var(--acc)"/><circle cx="21.5" cy="12.5" r="1.7" fill="var(--acc)"/></svg>)SVG";

const char* const kPrelude = R"JS(
const $=s=>document.querySelector(s),$$=s=>[...document.querySelectorAll(s)];
const esc=t=>String(t).replace(/[&<>"']/g,c=>'&#'+c.charCodeAt(0)+';');
const btn=(a,t,c)=>'<button type="button" class="btn'+(c?' '+c:'')+'" data-a="'+a+'">'+t+'</button>';
)JS";

const char* const kPicker = R"JS(
function picker(changed){
  const nf=$('#nf');if(!nf)return()=>{};
  const ssid=$('#ssid'),pw=$('#pw'),rows=$$('.net'),other=rows.find(r=>r.dataset.other);
  const passSet=nf.dataset.passSet==='1',current=nf.dataset.current;
  let otherChosen=false;
  function place(){
    const v=ssid.value;
    let row=otherChosen?null:rows.find(r=>!r.dataset.other&&r.dataset.ssid===v);
    if(!row&&(v||otherChosen))row=other;
    rows.forEach(r=>r.classList.toggle('on',r===row));
    if(row){if(nf.previousElementSibling!==row)row.after(nf);nf.classList.remove('later')}else nf.classList.add('later');
    const listed=row&&row!==other,open=listed&&row.dataset.open==='1';
    $('#ssidrow').classList.toggle('later',!!listed);
    $('#pwrow').classList.toggle('later',!!open);
    $('#openrow').classList.toggle('later',!open);
    if(open)pw.value='';
    pw.placeholder=v===current&&passSet?'已設定（留空表示不修改）':'密碼（開放網路留空）';
    changed&&changed();
  }
  rows.forEach(r=>r.onclick=()=>{
    if(r===other){otherChosen=true;if(rows.some(x=>x!==other&&x.dataset.ssid===ssid.value))ssid.value='';place();ssid.focus()}
    else{otherChosen=false;if(ssid.value!==r.dataset.ssid){ssid.value=r.dataset.ssid;pw.value=''}place();if(r.dataset.open!=='1')pw.focus()}
    ssid.dispatchEvent(new Event('input',{bubbles:true}));
  });
  ssid.addEventListener('input',()=>changed&&changed());
  pw.addEventListener('input',()=>changed&&changed());
  $('#eye').onclick=()=>{const show=pw.type==='password';pw.type=show?'text':'password';$('#eye').textContent=show?'隱藏':'顯示'};
  place();
  return place;
}
)JS";

const char* const kWifiScript = R"JS(
const go=$('#go'),hint=$('#hint');
function check(){
  const v=$('#ssid').value.trim(),p=$('#pw').value,row=$('.net.on'),nf=$('#nf');
  const needsPass=row&&!row.dataset.other&&row.dataset.open!=='1'&&!(v===nf.dataset.current&&nf.dataset.passSet==='1');
  let m='';
  if(!v)m='先選一個網路';
  else if(p&&(p.length<8||p.length>63))m='WiFi 密碼要 8 到 63 個字元';
  else if(needsPass&&!p)m='輸入「'+v+'」的密碼';
  go.disabled=!!m;
  hint.textContent=m||'儲存後裝置會重新啟動，連上「'+v+'」';
}
picker(check);
$('#f').addEventListener('submit',()=>{go.disabled=true;go.textContent='儲存中…'});
)JS";

const char* const kFormScript = R"JS(
const form=$('#f'),home=$('#v-home'),views=$$('.view');
let cur=null,homeY=0,submitting=false,dirty=0;

function show(){
  const id=location.hash.slice(1),el=(id&&document.getElementById('v-'+id))||home;
  if(cur===home&&el!==home)homeY=scrollY;
  const sub=el!==home,was=cur;
  views.forEach(v=>v.classList.toggle('on',v===el));
  document.body.classList.toggle('subview',sub);
  if(sub){$('#vt').textContent=el.dataset.title;const p=el.dataset.parent;$('#backlabel').textContent=p?document.getElementById('v-'+p).dataset.title:'Glance'}
  cur=el;
  scrollTo(0,sub?0:(was&&was!==home?homeY:scrollY));
  if(el.id==='v-photos'&&!window.glancePhotos){window.glancePhotos=1;const s=document.createElement('script');s.src='/photos.js';document.body.append(s)}
}
addEventListener('hashchange',show);
addEventListener('popstate',show);
// Views are opened with pushState, never by following the #links: iOS
// Safari (27) loads the whole page again for those, from the device, which
// flashes and drops unsaved changes. pushState never loads anything.
function go(id){if(location.hash.slice(1)!==id){history.pushState(null,'','#'+id);show()}}
document.addEventListener('click',e=>{
  const a=e.target.closest('a[href^="#"]');if(!a)return;
  const id=a.getAttribute('href').slice(1);if(id&&!document.getElementById('v-'+id))return;
  e.preventDefault();go(id);
});
// Back: straight to the parent view. Not history.back(): that can land on
// an earlier load of the page, which loads it again.
$('#back').onclick=()=>go(cur.dataset.parent||'');

// Stored secrets (.secret): replaced, removed or kept; empty calendar slots: added.
function sec(c,st){
  c.dataset.state=st;
  const url=c.querySelector('.url'),chip=c.querySelector('.chip'),gone=c.querySelector('.gone'),rm=c.querySelector('.rm input'),acts=c.querySelector('.acts');
  if(c.dataset.set==='1'){
    url.classList.toggle('later',st!=='edit');
    if(chip)chip.classList.toggle('later',st==='removed');
    if(gone)gone.classList.toggle('later',st!=='removed');
    acts.innerHTML=st==='edit'?btn('keep','取消，保留原本的'):st==='removed'?btn('restore','復原'):btn('edit',c.dataset.edit||'更換')+(rm?btn('del','刪除','dan'):'');
  }else{
    c.classList.toggle('later',st==='hidden');
    acts.innerHTML=st==='hidden'?'':btn('cancel','不加了');
  }
}
function initSecrets(){
  $$('.secret').forEach(c=>{
    const i=c.querySelector('.url input'),rm=c.querySelector('.rm input');
    if(c.dataset.set==='1')sec(c,rm&&rm.checked?'removed':i.value?'edit':'idle');
    else sec(c,i.value?'open':'hidden');
  });
  calendars();
}
function calendars(){
  const add=$('#cal-add');if(!add)return;
  const slots=$$('.secret[data-slot]');
  add.disabled=!slots.some(c=>c.dataset.state==='hidden');
  $('#cal-n').textContent=slots.filter(c=>c.dataset.state==='idle'||c.dataset.state==='edit'||c.dataset.state==='open').length;
}

// Fields shown for one choice only: data-when="name=value|value".
function when(){
  $$('[data-when]').forEach(el=>{
    const [n,v]=el.dataset.when.split('='),r=form.querySelector('input[name="'+n+'"]:checked');
    el.classList.toggle('later',!r||!v.split('|').includes(r.value));
  });
}

// What changed since the page loaded: the save bar, marks on fields and rows.
const fields=[...form.elements].filter(e=>e.name&&e.type!=='hidden'&&e.type!=='submit');
const val=e=>e.type==='checkbox'||e.type==='radio'?e.checked:e.value;
const base=new Map(fields.map(e=>[e,val(e)]));
function update(){
  const groups=new Set(),touched=new Set();
  $$('.f.dirty').forEach(f=>f.classList.remove('dirty'));
  for(const e of fields){
    if(val(e)===base.get(e))continue;
    groups.add(e.dataset.g||e.name);
    const f=e.closest('.f');if(f)f.classList.add('dirty');
    const v=e.closest('.view');if(v){touched.add(v.id.slice(2));if(v.dataset.parent)touched.add(v.dataset.parent)}
  }
  $$('[data-dirty]').forEach(r=>r.hidden=!touched.has(r.dataset.dirty));
  dirty=groups.size;
  $('#savebar').classList.toggle('show',dirty>0||document.body.dataset.unsaved==='1');
  $('#savemsg').innerHTML=dirty?'<b>'+dirty+' 項變更</b>還沒儲存':'還沒儲存';
  $('#undo').hidden=!dirty;
  keepDraft();
}

// Unsaved changes, kept in this tab (sessionStorage) so they survive the
// page being reloaded: iOS reloads it when the phone comes back from the
// app a calendar URL was copied in. Only for the settings the page loaded
// with (`sig`); gone once saved, undone or the tab is closed.
const sig=fields.map(e=>e.name+'='+base.get(e)).join('\n'),draftKey='glance-draft';
function keepDraft(){
  try{
    if(!dirty||submitting){sessionStorage.removeItem(draftKey);return}
    const changed={};
    fields.forEach((e,i)=>{if(val(e)!==base.get(e))changed[i]=[e.name,val(e)]});
    sessionStorage.setItem(draftKey,JSON.stringify({sig,changed}));
  }catch(e){}
}
function restoreDraft(){
  let d=null;
  try{d=JSON.parse(sessionStorage.getItem(draftKey))}catch(e){}
  if(!d||d.sig!==sig)return false;
  for(const [i,[name,v]] of Object.entries(d.changed)){
    const e=fields[i];if(!e||e.name!==name)continue;
    if(e.type==='checkbox'||e.type==='radio')e.checked=v;else e.value=v;
  }
  return true;
}
form.addEventListener('input',()=>{when();update()});
form.addEventListener('change',()=>{when();update()});
form.addEventListener('submit',()=>{submitting=true;keepDraft();$('#save').disabled=true;$('#save').textContent='儲存中…'});
addEventListener('beforeunload',e=>{if(dirty&&!submitting){e.preventDefault();e.returnValue=''}});
const place=picker(null);
$('#undo').onclick=()=>{
  for(const e of fields){if(e.type==='checkbox'||e.type==='radio')e.checked=base.get(e);else e.value=base.get(e)}
  initSecrets();when();place();update();
};

// Updates by hand (the advanced view).
const out=$('#upd-out'),msg=$('#upd-msg');
async function checkUpdate(){
  out.innerHTML='<span class="chip">檢查中…</span>';msg.textContent='';
  try{
    const r=await(await fetch('/update')).json();
    if(r.error){out.innerHTML=btn('check','再試一次');msg.textContent=r.error;return}
    if(!r.installable){out.innerHTML='<span class="chip ok">已是最新</span>';msg.textContent='最新的正式版是 '+r.latest+'。';return}
    out.dataset.v=r.latest;out.innerHTML=btn('install','安裝 '+esc(r.latest),'pri');
    msg.textContent='有新版本 '+r.latest+'。安裝時裝置會重新啟動，大約一兩分鐘。';
  }catch(e){out.innerHTML=btn('check','再試一次');msg.textContent='連不到裝置，請再試一次。'}
}
async function install(b){
  if(dirty){msg.textContent='先儲存或還原這頁的變更，再安裝更新。';return}
  b.disabled=true;
  try{
    const r=await(await fetch('/update',{method:'POST'})).json();
    if(r.error){msg.textContent=r.error;b.disabled=false;return}
    installing(out.dataset.v);
  }catch(e){msg.textContent='連不到裝置，請再試一次。';b.disabled=false}
}
// The whole page gives way to the update: the device takes no more changes
// (it would refuse them) and restarts into the new version, out of setup,
// so nothing here would ever answer again.
function installing(v){
  submitting=true;try{sessionStorage.removeItem(draftKey)}catch(e){}
  document.body.classList.remove('subview');
  $('main').innerHTML='<div class="done"><div class="qr"><svg aria-hidden="true"><use href="#i-gear"/></svg></div>'+
    '<h1>正在更新到 '+esc(v||'新版本')+'</h1><p>Glance 正在下載並安裝，大約一兩分鐘。裝好後會自動重新啟動、更新畫面。</p>'+
    '<p>這頁可以關了。</p><p class="help" style="margin-top:16px">幾分鐘後畫面都沒變的話，重新整理這頁再試一次。</p></div>';
  scrollTo(0,0);
}

form.addEventListener('click',e=>{
  const b=e.target.closest('[data-a]');if(!b)return;
  const c=b.closest('.secret'),i=c&&c.querySelector('.url input'),rm=c&&c.querySelector('.rm input');
  switch(b.dataset.a){
    case'edit':sec(c,'edit');i.focus();break;
    case'keep':i.value='';sec(c,'idle');break;
    case'del':i.value='';rm.checked=true;sec(c,'removed');break;
    case'restore':rm.checked=false;sec(c,'idle');break;
    case'cancel':i.value='';sec(c,'hidden');break;
    case'add':{const n=$('.secret[data-slot][data-state="hidden"]');if(n){sec(n,'open');n.querySelector('.url input').focus()}break}
    case'check':checkUpdate();return;
    case'install':install(b);return;
  }
  calendars();update();
});

// Photo counts, kept current by the photos view.
document.addEventListener('glance-photos',e=>{
  const d=e.detail;
  $$('[data-photos]').forEach(el=>{el.textContent=d[el.dataset.photos]});
});

const restored=restoreDraft();
initSecrets();when();place();update();show();
if(restored&&dirty)$('#savemsg').innerHTML='<b>已填回 '+dirty+' 項</b>還沒儲存的變更';
)JS";

}  // namespace setup_page::assets
