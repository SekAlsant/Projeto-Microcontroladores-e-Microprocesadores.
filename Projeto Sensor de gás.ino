#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "ScioSense_ENS160.h"
#include <Adafruit_AHTX0.h>

const char* SSID = "Firefox";
const char* PASS = "87654321";

WebServer server(80);
ScioSense_ENS160 ens160(ENS160_I2CADDR_0);
Adafruit_AHTX0 aht;

const int PIN_MQ2 = 32, PIN_MQ135 = 33, PIN_MQ7 = 34;
const int PIN_LED_G = 25, PIN_LED_R = 27;
const int PIN_BUZ1 = 14, PIN_BUZ2 = 12;

bool ahtOk = false, ensOk = false, ensWarm = false;
bool mq2Ok = false, mq135Ok = false, mq7Ok = false;
int base2 = 0, base135 = 0, base7 = 0;
int ppm2 = 0, ppm135 = 0, ppm7 = 0;
int pct2 = 0, pct135 = 0, pct7 = 0, pctTvoc = 0, pctEco2 = 0, pctTemp = 0;
float tempC = 0, umidPct = 0;
uint8_t aqi = 0;
uint16_t tvoc = 0, eco2 = 400;
bool atencao = false, grave = false;

unsigned long tLeitura = 0, tWifi = 0, tBuz = 0;
int passoBuz = 0;

const char HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="pt-BR"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Monitor Lab Química</title>
<style>
body{font-family:Arial,sans-serif;background:#f1f5f9;color:#0f172a;margin:0;padding:12px}
.box{max-width:960px;margin:auto}
.top{background:#0f172a;color:#fff;padding:14px;border-radius:10px;margin-bottom:12px}
.top h2{margin:0;font-size:18px}.top small{color:#94a3b8}
.ban{padding:12px;border-radius:10px;font-weight:bold;margin-bottom:12px;display:flex;justify-content:space-between;flex-wrap:wrap;gap:8px}
.ok{background:#dcfce7;color:#14532d}.at{background:#fef3c7;color:#78350f}.gr{background:#dc2626;color:#fff}
.card{background:#fff;padding:14px;border-radius:10px;margin-bottom:12px;border:1px solid #e2e8f0}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:8px}
.item{background:#f8fafc;padding:10px;border-radius:8px;border:1px solid #e2e8f0;display:flex;justify-content:space-between;align-items:center}
.pill{padding:3px 10px;border-radius:20px;color:#fff;font-size:12px;font-weight:bold}
.on{background:#16a34a}.off{background:#dc2626}
.al{padding:10px;border-radius:8px;margin-bottom:6px;background:#fef3c7;border-left:4px solid #d97706}
.al.g{background:#fee2e2;border-left-color:#dc2626;color:#7f1d1d}
table{width:100%;border-collapse:collapse;font-size:13px}
th,td{padding:8px 6px;border-bottom:1px solid #e2e8f0;text-align:left}
.bar{background:#e2e8f0;height:9px;border-radius:9px;overflow:hidden;width:90px;display:inline-block;vertical-align:middle;margin-right:6px}
.fill{height:100%}
canvas{width:100%;height:180px;display:block}
</style></head><body><div class="box">
<div class="top"><h2>Laboratório de Química — Monitor Ambiental e Qualidade do Ar</h2><small id="ip">Conectando...</small></div>
<div id="ban" class="ban ok"><span id="btxt">AMBIENTE SEGURO</span><span id="qar">Qualidade do Ar: Avaliando...</span></div>
<div class="card"><b>Status dos Sensores</b><div class="grid" style="margin-top:8px">
<div class="item"><span>ENS160</span><span id="s-ens" class="pill off">Desativado</span></div>
<div class="item"><span>AHT2X</span><span id="s-aht" class="pill off">Desativado</span></div>
<div class="item"><span>MQ-2</span><span id="s-m2" class="pill off">Desativado</span></div>
<div class="item"><span>MQ-135</span><span id="s-m135" class="pill off">Desativado</span></div>
<div class="item"><span>MQ-7</span><span id="s-m7" class="pill off">Desativado</span></div>
</div></div>
<div class="card"><b>Detecção de Substâncias e Qualidade do Ar</b><div id="als" style="margin-top:8px">Nenhuma alteração detectada.</div></div>
<div class="card"><b>Leituras (ppm e Escala 0% a 100%)</b>
<table><thead><tr><th>Sensor</th><th>Parâmetro / Substância</th><th>Valor</th><th>Escala (0-100%)</th></tr></thead>
<tbody id="tb"></tbody></table></div>
<div class="card"><b>Gráfico em Tempo Real (0% a 100%)</b>
<div style="font-size:12px;margin:6px 0">
<span style="color:#dc2626">■ CO (MQ-7)</span> &nbsp;
<span style="color:#7c3aed">■ Poluentes Gerais (MQ-135)</span> &nbsp;
<span style="color:#ea580c">■ Inflamável (MQ-2)</span> &nbsp;
<span style="color:#0284c7">■ Qualidade Ar/TVOC</span> &nbsp;
<span style="color:#0d9488">■ Umidade</span>
</div><canvas id="cv"></canvas></div></div>
<script>
const H={co:[],nh:[],lp:[],vo:[],hu:[]};
const AQI_TXT=['Indisponível','Excelente (1/5)','Boa (2/5)','Moderada (3/5)','Ruim (4/5)','Muito Ruim (5/5)'];
function pill(id,ok,w){const e=document.getElementById(id);e.className='pill '+(ok?'on':'off');e.textContent=ok?(w?'Ativado (Aquec.)':'Ativado'):'Desativado';}
function row(s,p,v,pc,ok,cl){
 const c=!ok?'#94a3b8':(cl?'#0284c7':(pc>=70?'#dc2626':(pc>=36?'#d97706':'#16a34a')));
 const w=ok?Math.min(100,Math.max(0,pc)):0;
 return `<tr><td><b>${s}</b></td><td>${p}</td><td>${ok?v:'—'}</td><td><div class="bar"><div class="fill" style="width:${w}%;background:${c}"></div></div><b>${ok?w+'%':'—'}</b></td></tr>`;
}
function draw(){
 const c=document.getElementById('cv'),ctx=c.getContext('2d');
 c.width=c.clientWidth;c.height=180;const W=c.width,Y=180;
 ctx.clearRect(0,0,W,Y);ctx.strokeStyle='#e2e8f0';
 [0,35,70,100].forEach(v=>{const y=Y-(v/100)*(Y-20)-10;ctx.beginPath();ctx.moveTo(28,y);ctx.lineTo(W,y);ctx.stroke();ctx.fillStyle='#64748b';ctx.fillText(v+'%',2,y+3);});
 const ser=[[H.hu,'#0d9488'],[H.vo,'#0284c7'],[H.lp,'#ea580c'],[H.nh,'#7c3aed'],[H.co,'#dc2626']];
 ser.forEach(([a,col])=>{if(a.length<2)return;ctx.strokeStyle=col;ctx.lineWidth=2;ctx.beginPath();
 a.forEach((v,i)=>{const x=28+(i/29)*(W-34),y=Y-(v/100)*(Y-20)-10;i?ctx.lineTo(x,y):ctx.moveTo(x,y);});ctx.stroke();});
}
async function up(){
 try{
  const d=await(await fetch('/dados')).json();
  document.getElementById('ip').textContent='Wi-Fi: Firefox | IP: '+d.ip;
  const er=d.ens[0]&&!d.ens[1];
  const aqiVal=d.ens[2];
  let qLabel='Sem sensores ativos';
  if(er){qLabel=AQI_TXT[aqiVal]||'Boa';}
  else if(d.m135[0]||d.m2[0]||d.m7[0]){
    qLabel=d.gr?'Muito Ruim (Crítica)':(d.at?'Moderada / Alterada':'Boa (Ar Limpo)');
  }
  document.getElementById('qar').textContent='Qualidade do Ar: '+qLabel;

  const b=document.getElementById('ban');
  b.className='ban '+(d.gr?'gr':(d.at?'at':'ok'));
  document.getElementById('btxt').textContent=d.gr?'PERIGO GRAVE: Concentração crítica detectada!':(d.at?'ATENÇÃO: Alteração detectada no laboratório':'AMBIENTE SEGURO — Condições normais');

  pill('s-ens',d.ens[0],d.ens[1]);pill('s-aht',d.aht[0],0);pill('s-m2',d.m2[0],0);pill('s-m135',d.m135[0],0);pill('s-m7',d.m7[0],0);

  const als=[];
  const chk=(ok,pc,s,sub,val)=>{if(ok&&pc>=36)als.push(`<div class="al ${pc>=70?'g':''}">Sensor <b>${s}</b> detectou ${pc>=70?'alta':'elevada'} concentração de <b>${sub}</b>: ${val} (${pc}%)</div>`);};
  chk(d.m7[0],d.m7[2],'MQ-7','Monóxido de Carbono (CO)',d.m7[1]+' ppm');
  chk(d.m135[0],d.m135[2],'MQ-135','Poluentes Gerais (Amônia / Solventes / Fumaça — sem identificação isolada)',d.m135[1]+' ppm');
  chk(d.m2[0],d.m2[2],'MQ-2','Gás Inflamável / Fumaça',d.m2[1]+' ppm');
  chk(er,d.ens[5],'ENS160','Compostos Orgânicos Voláteis (TVOC)',d.ens[3]+' ppb');
  chk(er,d.ens[6],'ENS160','CO₂ Equivalente (eCO₂)',d.ens[4]+' ppm');
  document.getElementById('als').innerHTML=als.length?als.join(''):'Nenhuma substância nociva detectada. Qualidade do ar: <b>'+qLabel+'</b>.';

  const aqiPct=er?Math.min(100,Math.max(0,(aqiVal-1)*25)):0;
  document.getElementById('tb').innerHTML=
   row('ENS160','Qualidade Geral do Ar (AQI)',er?(AQI_TXT[aqiVal]||'—'):'Aquecendo...',aqiPct,er,0)+
   row('MQ-135','Poluentes Gerais (NH₃, Solventes, Fumaça)',d.m135[1]+' ppm (est.)',d.m135[2],d.m135[0],0)+
   row('MQ-7','Monóxido de Carbono (CO)',d.m7[1]+' ppm (est.)',d.m7[2],d.m7[0],0)+
   row('MQ-2','Gás Inflamável / Fumaça',d.m2[1]+' ppm (est.)',d.m2[2],d.m2[0],0)+
   row('ENS160','TVOC (Compostos Orgânicos)',er?d.ens[3]+' ppb':'Aquecendo...',d.ens[5],er,0)+
   row('ENS160','eCO₂ (CO₂ Equivalente)',er?d.ens[4]+' ppm':'Aquecendo...',d.ens[6],er,0)+
   row('AHT2X','Umidade Relativa',d.aht[2]+' %',d.aht[2],d.aht[0],1)+
   row('AHT2X','Temperatura',d.aht[1]+' °C',d.aht[3],d.aht[0],1);

  H.co.push(d.m7[0]?d.m7[2]:0);H.nh.push(d.m135[0]?d.m135[2]:0);H.lp.push(d.m2[0]?d.m2[2]:0);
  H.vo.push(er?Math.max(d.ens[5],d.ens[6]):0);H.hu.push(d.aht[0]?d.aht[2]:0);
  Object.values(H).forEach(a=>{if(a.length>30)a.shift();});draw();
 }catch(e){}
}
up();setInterval(up,2000);
</script></body></html>
)rawliteral";

bool pingI2C(uint8_t addr) {
  Wire.beginTransmission(addr);
  return (Wire.endTransmission() == 0);
}

// Retorna true apenas se o módulo MQ estiver fisicamente conectado e estável
bool lerSensorMQ(int pino, int &mediaOut) {
  int minV = 4095, maxV = 0;
  long soma = 0;
  for (int i = 0; i < 20; i++) {
    int v = analogRead(pino);
    if (v < minV) minV = v;
    if (v > maxV) maxV = v;
    soma += v;
    delay(1);
  }
  mediaOut = soma / 20;
  int oscilacao = maxV - minV;
  // Pino desconectado cai para ~0 (pull-down) ou oscila com ruído 60Hz (>150)
  if (mediaOut < 60 || mediaOut > 4090 || oscilacao > 150) {
    return false;
  }
  return true;
}

int calcPct(int val, int base, bool ok) {
  if (!ok || base <= 0 || val - base <= 45) return 0;
  return constrain(((val - base) * 100) / max(450, 4095 - base), 1, 100);
}

int calcPPM(int pct, int seg, int grv, int maxP) {
  if (pct <= 0) return 0;
  if (pct <= 35) return map(pct, 1, 35, 1, seg);
  if (pct <= 69) return map(pct, 36, 69, seg + 1, grv);
  return map(pct, 70, 100, grv + 1, maxP);
}

void lerSensores() {
  // 1. Verifica presença real dos sensores I2C em tempo real
  bool ahtAgora = pingI2C(0x38);
  if (ahtAgora && !ahtOk) aht.begin();
  ahtOk = ahtAgora;

  if (ahtOk) {
    sensors_event_t h, t;
    if (aht.getEvent(&h, &t)) {
      tempC = t.temperature;
      umidPct = constrain(h.relative_humidity, 0.0f, 100.0f);
      pctTemp = constrain((int)((tempC - 10.0f) * 2.5f), 0, 100);
    } else ahtOk = false;
  }

  bool ensAgora = pingI2C(0x53);
  if (ensAgora && !ensOk) {
    if (ens160.begin()) ens160.setMode(ENS160_OPMODE_STD);
  }
  ensOk = ensAgora;

  if (ensOk) {
    if (ahtOk) ens160.set_envdata(tempC, umidPct);
    ens160.measure(true);
    aqi = ens160.getAQI();
    tvoc = ens160.getTVOC();
    eco2 = ens160.geteCO2();
    ensWarm = (aqi == 255 || tvoc == 65535 || eco2 == 65535);
    pctTvoc = ensWarm ? 0 : constrain(map(tvoc, 0, 1500, 0, 100), 0, 100);
    pctEco2 = ensWarm ? 0 : constrain(map(eco2, 400, 2500, 0, 100), 0, 100);
    if (!ensWarm && aqi >= 5) pctTvoc = max(pctTvoc, 75);
  } else {
    ensWarm = false; aqi = 0; tvoc = 0; eco2 = 400; pctTvoc = 0; pctEco2 = 0;
  }

  // 2. Verifica presença real dos sensores MQ e calcula leituras
  int r2 = 0, r135 = 0, r7 = 0;
  mq2Ok   = lerSensorMQ(PIN_MQ2, r2);
  mq135Ok = lerSensorMQ(PIN_MQ135, r135);
  mq7Ok   = lerSensorMQ(PIN_MQ7, r7);

  if (mq2Ok && base2 == 0) base2 = r2;
  if (!mq2Ok) base2 = 0;

  if (mq135Ok && base135 == 0) base135 = r135;
  if (!mq135Ok) base135 = 0;

  if (mq7Ok && base7 == 0) base7 = r7;
  if (!mq7Ok) base7 = 0;

  pct2   = calcPct(r2, base2, mq2Ok);
  pct135 = calcPct(r135, base135, mq135Ok);
  pct7   = calcPct(r7, base7, mq7Ok);

  ppm2   = mq2Ok   ? calcPPM(pct2, 300, 1500, 10000) : 0;
  ppm135 = mq135Ok ? calcPPM(pct135, 25, 80, 300)    : 0;
  ppm7   = mq7Ok   ? calcPPM(pct7, 35, 100, 2000)    : 0;

  int maxP = max(max(pct2, pct135), pct7);
  if (ensOk && !ensWarm) maxP = max(maxP, max(pctTvoc, pctEco2));

  grave = (maxP >= 70);
  atencao = (maxP >= 36 && maxP < 70);

  digitalWrite(PIN_LED_R, (atencao || grave) ? HIGH : LOW);
  digitalWrite(PIN_LED_G, (atencao || grave) ? LOW : HIGH);
}

void tocarBuzzers() {
  if (!grave) {
    analogWrite(PIN_BUZ1, 0);
    analogWrite(PIN_BUZ2, 0);
    return;
  }
  if (millis() - tBuz >= 280) {
    tBuz = millis();
    passoBuz = (passoBuz + 1) % 4;
    analogWrite(PIN_BUZ1, passoBuz == 0 ? 25 : 0);
    analogWrite(PIN_BUZ2, passoBuz == 1 ? 25 : 0);
  }
}

void handleRoot() { server.send_P(200, "text/html", HTML); }

void handleDados() {
  char buf[460];
  snprintf(buf, sizeof(buf),
    "{\"ip\":\"%s\",\"at\":%d,\"gr\":%d,\"ens\":[%d,%d,%u,%u,%u,%d,%d],\"aht\":[%d,%.1f,%.1f,%d],\"m2\":[%d,%d,%d],\"m135\":[%d,%d,%d],\"m7\":[%d,%d,%d]}",
    WiFi.localIP().toString().c_str(), atencao, grave,
    ensOk, ensWarm, aqi, tvoc, eco2, pctTvoc, pctEco2,
    ahtOk, tempC, umidPct, pctTemp,
    mq2Ok, ppm2, pct2,
    mq135Ok, ppm135, pct135,
    mq7Ok, ppm7, pct7);
  server.send(200, "application/json", buf);
}

void setup() {
  Serial.begin(9600);
  pinMode(PIN_LED_G, OUTPUT); pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_BUZ1, OUTPUT);  pinMode(PIN_BUZ2, OUTPUT);
  digitalWrite(PIN_LED_G, HIGH); digitalWrite(PIN_LED_R, LOW);
  analogWrite(PIN_BUZ1, 0); analogWrite(PIN_BUZ2, 0);

  // Pull-down interno evita leitura fantasma quando MQ-2 ou MQ-135 são removidos
  pinMode(PIN_MQ2, INPUT_PULLDOWN);
  pinMode(PIN_MQ135, INPUT_PULLDOWN);
  pinMode(PIN_MQ7, INPUT);

  Wire.begin(21, 22);
  ahtOk = pingI2C(0x38) && aht.begin();
  ensOk = pingI2C(0x53) && ens160.begin();
  if (ensOk) ens160.setMode(ENS160_OPMODE_STD);

  Serial.println("Calibrando sensores MQ ativos (3s)...");
  long s2 = 0, s135 = 0, s7 = 0;
  int c2 = 0, c135 = 0, c7 = 0;
  for (int i = 0; i < 15; i++) {
    int v;
    if (lerSensorMQ(PIN_MQ2, v))   { s2 += v; c2++; }
    if (lerSensorMQ(PIN_MQ135, v)) { s135 += v; c135++; }
    if (lerSensorMQ(PIN_MQ7, v))   { s7 += v; c7++; }
    delay(150);
  }
  base2   = (c2 > 7)   ? (s2 / c2)     : 0;
  base135 = (c135 > 7) ? (s135 / c135) : 0;
  base7   = (c7 > 7)   ? (s7 / c7)     : 0;

  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASS);
  Serial.print("Conectando Wi-Fi");
  for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) { delay(500); Serial.print("."); }
  Serial.print("\nIP: http://"); Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/dados", handleDados);
  server.begin();
  lerSensores();
}

void loop() {
  server.handleClient();
  tocarBuzzers();
  unsigned long now = millis();
  if (now - tWifi >= 10000) {
    tWifi = now;
    if (WiFi.status() != WL_CONNECTED) WiFi.reconnect();
  }
  if (now - tLeitura >= 2000) {
    tLeitura = now;
    lerSensores();
    Serial.printf("CO:%dppm(%d%%) | Poluentes:%dppm(%d%%) | GLP:%dppm(%d%%) | %.1fC %.1f%%\n",
                  ppm7, pct7, ppm135, pct135, ppm2, pct2, tempC, umidPct);
  }
}
