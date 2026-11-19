#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <LiquidCrystal.h>
#include <sys/time.h>
#include <time.h>

const char* ssid = "Firefox";
const char* password = "87654321";

WebServer server(80);
Preferences prefs;
LiquidCrystal lcd(13, 12, 14, 27, 26, 25);

const int pinoRele = 33;
const int pinoBotao = 32;

unsigned long ultimoMillisRelogio = 0;
bool horaSincronizada = false;

struct Agendamento {
  int tipo;
  int diaSemana;
  int diaMes;
  int mes;
  int hora;
  int minuto;
  int duracao;
  int unidade;
};
Agendamento agendamentos[10];

unsigned long releDesligaEmMillis = 0;
bool releLigado = false;
bool modoManualContinuo = false;

void salvarNaMemoria() {
  prefs.begin("rele_app", false);
  for (int i = 0; i < 10; i++) {
    prefs.putInt(String("t" + String(i)).c_str(), agendamentos[i].tipo);
    prefs.putInt(String("w" + String(i)).c_str(), agendamentos[i].diaSemana);
    prefs.putInt(String("dm" + String(i)).c_str(), agendamentos[i].diaMes);
    prefs.putInt(String("ms" + String(i)).c_str(), agendamentos[i].mes);
    prefs.putInt(String("h" + String(i)).c_str(), agendamentos[i].hora);
    prefs.putInt(String("m" + String(i)).c_str(), agendamentos[i].minuto);
    prefs.putInt(String("d" + String(i)).c_str(), agendamentos[i].duracao);
    prefs.putInt(String("u" + String(i)).c_str(), agendamentos[i].unidade);
  }
  prefs.end();
}

void carregarDaMemoria() {
  prefs.begin("rele_app", true);
  for (int i = 0; i < 10; i++) {
    agendamentos[i].tipo = prefs.getInt(String("t" + String(i)).c_str(), 0);
    agendamentos[i].diaSemana = prefs.getInt(String("w" + String(i)).c_str(), 0);
    agendamentos[i].diaMes = prefs.getInt(String("dm" + String(i)).c_str(), 1);
    agendamentos[i].mes = prefs.getInt(String("ms" + String(i)).c_str(), 1);
    agendamentos[i].hora = prefs.getInt(String("h" + String(i)).c_str(), 0);
    agendamentos[i].minuto = prefs.getInt(String("m" + String(i)).c_str(), 0);
    agendamentos[i].duracao = prefs.getInt(String("d" + String(i)).c_str(), 0);
    agendamentos[i].unidade = prefs.getInt(String("u" + String(i)).c_str(), 1);
  }
  prefs.end();
}

void renderizarPaginaHTML() {
  const char* diasNomes[7] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sab"};
  String html = "<!DOCTYPE html><html lang='pt-BR'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Smart Rele</title><style>";
  html += "*{box-sizing:border-box;margin:0;padding:0;}";
  html += "body{font-family:'Segoe UI',system-ui,sans-serif;background:#07090e;color:#e2e8f0;min-height:100vh;padding:16px;overflow-x:hidden;position:relative;}";
  html += ".bg-orb{position:fixed;border-radius:50%;filter:blur(90px);z-index:-1;opacity:0.45;animation:floatOrb 12s infinite alternate ease-in-out;}";
  html += ".orb1{width:320px;height:320px;background:#00d2ff;top:-80px;left:-80px;}";
  html += ".orb2{width:360px;height:360px;background:#1e3a8a;bottom:-100px;right:-80px;animation-delay:-6s;}";
  html += "@keyframes floatOrb{0%{transform:translate(0,0) scale(1);}100%{transform:translate(45px,35px) scale(1.15);}}";
  html += ".container{max-width:640px;margin:0 auto;}";
  html += ".header{text-align:center;margin-bottom:22px;padding:10px;}";
  html += "h1{font-size:26px;font-weight:700;letter-spacing:1.5px;text-transform:uppercase;background:linear-gradient(90deg,#ffffff,#00d2ff);-webkit-background-clip:text;-webkit-text-fill-color:transparent;}";
  html += ".status-pill{display:inline-flex;align-items:center;gap:8px;margin-top:10px;padding:6px 14px;border-radius:20px;background:rgba(15,23,42,0.8);border:1px solid rgba(0,210,255,0.25);font-size:13px;color:#94a3b8;}";
  html += ".dot{width:9px;height:9px;border-radius:50%;background:#475569;transition:0.3s;}";
  html += ".dot.on{background:#00e676;box-shadow:0 0 10px #00e676;}";
  html += ".card{background:rgba(14,18,28,0.82);backdrop-filter:blur(14px);border:1px solid rgba(255,255,255,0.08);border-radius:16px;padding:20px;margin-bottom:18px;box-shadow:0 12px 30px rgba(0,0,0,0.5);transition:border-color 0.3s,transform 0.2s;}";
  html += ".card:hover{border-color:rgba(0,210,255,0.3);}";
  html += "h2{font-size:17px;color:#f8fafc;margin-bottom:6px;letter-spacing:0.5px;}";
  html += ".sub{font-size:13px;color:#64748b;margin-bottom:16px;}";
  html += ".btn{border:none;padding:13px 20px;font-size:15px;border-radius:10px;cursor:pointer;color:#fff;font-weight:600;width:100%;transition:all 0.25s ease;letter-spacing:0.5px;}";
  html += ".btn:active{transform:scale(0.98);}";
  html += ".btn-on{background:linear-gradient(135deg,#00c853,#009624);box-shadow:0 4px 18px rgba(0,200,83,0.3);}";
  html += ".btn-off{background:linear-gradient(135deg,#ff3b5c,#c62828);box-shadow:0 4px 18px rgba(255,59,92,0.35);}";
  html += ".btn-timer{background:linear-gradient(135deg,#00d2ff,#0072ff);box-shadow:0 4px 18px rgba(0,114,255,0.3);margin-top:12px;}";
  html += ".btn-save{background:linear-gradient(135deg,#00d2ff,#0284c7);box-shadow:0 4px 18px rgba(0,210,255,0.25);margin-top:14px;}";
  html += "input,select{background:#090d16;color:#f1f5f9;border:1px solid #1e293b;border-radius:8px;padding:7px 6px;font-size:14px;font-weight:600;outline:none;transition:border-color 0.2s;}";
  html += "input:focus,select:focus{border-color:#00d2ff;}";
  html += "input[type=number]{width:48px;text-align:center;}";
  html += ".timer-row{display:flex;justify-content:center;align-items:center;gap:10px;}";
  html += ".slot{display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;gap:8px;padding:10px;margin-bottom:8px;background:rgba(9,13,22,0.75);border:1px solid #1e293b;border-radius:10px;}";
  html += ".slot-group{display:flex;align-items:center;gap:5px;}";
  html += ".tag-diario{font-size:12px;color:#00d2ff;padding:5px 8px;background:rgba(0,210,255,0.08);border-radius:6px;}";
  html += "</style></head><body>";
  html += "<div class='bg-orb orb1'></div><div class='bg-orb orb2'></div>";
  html += "<div class='container'><div class='header'><h1>Smart Control</h1>";
  html += "<div class='status-pill'><span id='dot' class='dot" + String(releLigado ? " on" : "") + "'></span><span id='msg'>Sincronizando sistema...</span></div></div>";

  html += "<div class='card'><h2>Modo Continuo</h2><p class='sub'>Acionamento direto sincronizado com o botao fisico.</p>";
  html += "<button id='btnToggle' class='btn " + String(releLigado ? "btn-off" : "btn-on") + "' onclick='toggleRele()'>" + String(releLigado ? "Desligar Rele" : "Ligar Rele") + "</button></div>";

  html += "<div class='card'><h2>Temporizador Rapido</h2><p class='sub'>Ativa a saida imediatamente pelo tempo definido.</p>";
  html += "<div class='timer-row'><input type='number' id='timerVal' value='10' min='1' style='width:75px;'>";
  html += "<select id='timerUnit'><option value='1'>Segundos</option><option value='60'>Minutos</option><option value='3600'>Horas</option></select></div>";
  html += "<button class='btn btn-timer' onclick='startTimer()'>Iniciar Temporizador</button></div>";

  html += "<div class='card'><h2>Agendamentos Automaticos</h2><p class='sub'>Deixe a duracao em 0 para desativar um slot.</p>";
  html += "<form action='/salvar' method='GET'>";

  for (int i = 0; i < 10; i++) {
    int t = agendamentos[i].tipo;
    html += "<div class='slot'><div class='slot-group'>";
    html += "<select id='t" + String(i) + "' name='t" + String(i) + "' onchange='mudarTipo(" + String(i) + ")'>";
    html += "<option value='0'" + String(t == 0 ? " selected" : "") + ">Diario</option>";
    html += "<option value='1'" + String(t == 1 ? " selected" : "") + ">Semanal</option>";
    html += "<option value='2'" + String(t == 2 ? " selected" : "") + ">Anual</option></select>";

    html += "<span id='d_dia" + String(i) + "' class='tag-diario' style='display:" + String(t == 0 ? "inline-block" : "none") + ";'>Todo dia</span>";

    html += "<select id='d_sem" + String(i) + "' name='w" + String(i) + "' style='display:" + String(t == 1 ? "inline-block" : "none") + ";'>";
    for (int d = 0; d < 7; d++) {
      html += "<option value='" + String(d) + "'" + String(agendamentos[i].diaSemana == d ? " selected" : "") + ">" + String(diasNomes[d]) + "</option>";
    }
    html += "</select>";

    html += "<span id='d_ano" + String(i) + "' style='display:" + String(t == 2 ? "inline-flex" : "none") + ";align-items:center;gap:3px;'>";
    html += "<input type='number' name='dm" + String(i) + "' min='1' max='31' value='" + String(agendamentos[i].diaMes) + "'>/";
    html += "<input type='number' name='ms" + String(i) + "' min='1' max='12' value='" + String(agendamentos[i].mes) + "'></span></div>";

    html += "<div class='slot-group'>";
    html += "<input type='number' name='h" + String(i) + "' min='0' max='23' value='" + String(agendamentos[i].hora) + "'>:";
    html += "<input type='number' name='m" + String(i) + "' min='0' max='59' value='" + String(agendamentos[i].minuto) + "'></div>";

    html += "<div class='slot-group'>";
    html += "<input type='number' name='d" + String(i) + "' min='0' max='999' value='" + String(agendamentos[i].duracao) + "'>";
    html += "<select name='u" + String(i) + "'>";
    html += "<option value='1'" + String(agendamentos[i].unidade == 1 ? " selected" : "") + ">S</option>";
    html += "<option value='60'" + String(agendamentos[i].unidade == 60 ? " selected" : "") + ">M</option>";
    html += "<option value='3600'" + String(agendamentos[i].unidade == 3600 ? " selected" : "") + ">H</option></select></div></div>";
  }

  html += "<button type='submit' class='btn btn-save'>Salvar Programacao</button></form></div></div>";

  html += "<script>";
  html += "function mudarTipo(i){let t=document.getElementById('t'+i).value;document.getElementById('d_dia'+i).style.display=(t=='0'?'inline-block':'none');document.getElementById('d_sem'+i).style.display=(t=='1'?'inline-block':'none');document.getElementById('d_ano'+i).style.display=(t=='2'?'inline-flex':'none');}";
  html += "function atualizarStatus(){fetch('/status').then(r=>r.text()).then(res=>{let p=res.split('|');let btn=document.getElementById('btnToggle');let dot=document.getElementById('dot');if(p[0]=='1'){btn.innerText='Desligar Rele';btn.className='btn btn-off';dot.className='dot on';}else{btn.innerText='Ligar Rele';btn.className='btn btn-on';dot.className='dot';}if(p[1])document.getElementById('msg').innerText=p[1];});}";
  html += "window.onload=function(){let d=new Date();fetch('/sync?y='+d.getFullYear()+'&mo='+(d.getMonth()+1)+'&da='+d.getDate()+'&h='+d.getHours()+'&m='+d.getMinutes()+'&s='+d.getSeconds()).then(()=>atualizarStatus());setInterval(atualizarStatus,1000);};";
  html += "function toggleRele(){fetch('/toggle').then(()=>atualizarStatus());}";
  html += "function startTimer(){let v=document.getElementById('timerVal').value;let u=document.getElementById('timerUnit').value;fetch('/timer?d='+(v*u)).then(()=>atualizarStatus());}";
  html += "</script></body></html>";

  server.send(200, "text/html", html);
}

void receberNovosAgendamentos() {
  for (int i = 0; i < 10; i++) {
    agendamentos[i].tipo = server.arg("t" + String(i)).toInt();
    agendamentos[i].diaSemana = server.arg("w" + String(i)).toInt();
    agendamentos[i].diaMes = server.arg("dm" + String(i)).toInt();
    agendamentos[i].mes = server.arg("ms" + String(i)).toInt();
    agendamentos[i].hora = server.arg("h" + String(i)).toInt();
    agendamentos[i].minuto = server.arg("m" + String(i)).toInt();
    agendamentos[i].duracao = server.arg("d" + String(i)).toInt();
    agendamentos[i].unidade = server.arg("u" + String(i)).toInt();
  }
  salvarNaMemoria();
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void sincronizarHora() {
  struct tm t = {0};
  t.tm_year = server.arg("y").toInt() - 1900;
  t.tm_mon = server.arg("mo").toInt() - 1;
  t.tm_mday = server.arg("da").toInt();
  t.tm_hour = server.arg("h").toInt();
  t.tm_min = server.arg("m").toInt();
  t.tm_sec = server.arg("s").toInt();
  time_t epoch = mktime(&t);
  struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
  settimeofday(&tv, NULL);
  horaSincronizada = true;
  server.send(200, "text/plain", "OK");
}

void enviarStatus() {
  const char* diasNomes[7] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sab"};
  String infoTempo = "Aguardando sincronizacao...";
  if (horaSincronizada) {
    time_t now;
    struct tm info;
    time(&now);
    localtime_r(&now, &info);
    char buf[32];
    sprintf(buf, "%s %02d/%02d - %02d:%02d:%02d", diasNomes[info.tm_wday], info.tm_mday, info.tm_mon + 1, info.tm_hour, info.tm_min, info.tm_sec);
    infoTempo = String(buf);
  }
  server.send(200, "text/plain", String(releLigado ? "1" : "0") + "|" + infoTempo);
}

void toggleManual() {
  if (releLigado) {
    releLigado = false;
    modoManualContinuo = false;
  } else {
    modoManualContinuo = true;
    releLigado = true;
  }
  server.send(200, "text/plain", releLigado ? "1" : "0");
}

void ativarTimer() {
  unsigned long totalSegundos = server.arg("d").toInt();
  modoManualContinuo = false;
  releLigado = true;
  releDesligaEmMillis = millis() + (totalSegundos * 1000UL);
  server.send(200, "text/plain", "OK");
}

void setup() {
  pinMode(pinoRele, OUTPUT);
  digitalWrite(pinoRele, LOW);
  pinMode(pinoBotao, INPUT_PULLUP);

  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("Conectando WiFi");

  carregarDaMemoria();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.begin(ssid, password);

  int tentativas = 0;
  lcd.setCursor(0, 1);
  while (WiFi.status() != WL_CONNECTED && tentativas < 30) {
    delay(500);
    lcd.print(".");
    tentativas++;
  }

  lcd.clear();
  if (WiFi.status() == WL_CONNECTED) {
    lcd.setCursor(0, 0);
    lcd.print("Acesse no PC:");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    delay(5000);
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Erro WiFi: " + String(WiFi.status()));
    lcd.setCursor(0, 1);
    lcd.print("Verifique 2.4GHz");
    delay(5000);
  }

  server.on("/", renderizarPaginaHTML);
  server.on("/salvar", receberNovosAgendamentos);
  server.on("/sync", sincronizarHora);
  server.on("/status", enviarStatus);
  server.on("/toggle", toggleManual);
  server.on("/timer", ativarTimer);
  server.begin();

  lcd.clear();
}

void loop() {
  server.handleClient();

  if (millis() - ultimoMillisRelogio >= 1000) {
    ultimoMillisRelogio = millis();

    lcd.setCursor(0, 0);
    if (!horaSincronizada) {
      lcd.print("Aguardando PC...");
    } else {
      time_t now;
      struct tm info;
      time(&now);
      localtime_r(&now, &info);

      char buffer[17];
      sprintf(buffer, "Hora: %02d:%02d:%02d", info.tm_hour, info.tm_min, info.tm_sec);
      lcd.print(buffer);

      if (info.tm_sec == 0 && !modoManualContinuo) {
        for (int i = 0; i < 10; i++) {
          if (agendamentos[i].duracao > 0 && agendamentos[i].hora == info.tm_hour && agendamentos[i].minuto == info.tm_min) {
            bool disparar = false;
            if (agendamentos[i].tipo == 0) disparar = true;
            else if (agendamentos[i].tipo == 1 && agendamentos[i].diaSemana == info.tm_wday) disparar = true;
            else if (agendamentos[i].tipo == 2 && agendamentos[i].diaMes == info.tm_mday && agendamentos[i].mes == (info.tm_mon + 1)) disparar = true;

            if (disparar) {
              releLigado = true;
              unsigned long duracaoMs = (unsigned long)agendamentos[i].duracao * (unsigned long)agendamentos[i].unidade * 1000UL;
              releDesligaEmMillis = millis() + duracaoMs;
            }
          }
        }
      }
    }
  }

  if (digitalRead(pinoBotao) == LOW) {
    if (releLigado) {
      releLigado = false;
      modoManualContinuo = false;
    } else {
      modoManualContinuo = true;
      releLigado = true;
    }
    delay(400);
  }

  if (!modoManualContinuo && releLigado && millis() > releDesligaEmMillis) {
    releLigado = false;
  }

  if (releLigado) {
    digitalWrite(pinoRele, HIGH);
    lcd.setCursor(0, 1);
    if (modoManualContinuo) lcd.print("Status: CONTINUO");
    else lcd.print("Status: LIGADO  ");
  } else {
    digitalWrite(pinoRele, LOW);
    lcd.setCursor(0, 1);
    lcd.print("Status: ESPERA  ");
  }
}
