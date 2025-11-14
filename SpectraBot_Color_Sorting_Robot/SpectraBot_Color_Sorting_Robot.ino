// =============================
// ESP32 Color Sorting Robot
// Web Dashboard + Calibration + Voice(TTS) + Buzzer + Servo Master Switch
//http://192.168.43.125/
// =============================

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h> 
#include <map>
#include <vector>

// ---------- Wi-Fi ----------
const char* ssid = "HUAWEI Y3";
const char* password = "12345678";

// ---------- Web server & WebSocket ----------
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ---------- Pins ----------
#define RED_LED_PIN   21
#define GREEN_LED_PIN 22
#define BLUE_LED_PIN  23

// TCS3200 pins (adjust if needed)
#define S0 27
#define S1 26
#define S2 25
#define S3 33
#define sensorOut 32

#define buzzerPin 14
#define resetButtonPin 15

// Servos
Servo topServo;
Servo bottomServo;
const int TOP_SERVO_PIN = 13;    // as in your earlier code
const int BOTTOM_SERVO_PIN = 12; // as in your earlier code

// LED PWM channels
#define RED_CH   0
#define GREEN_CH 1
#define BLUE_CH  2

// ---------- State ----------
String detectedColor = "None";
std::map<String,int> colorCounts = {
  {"Blue",0}, {"Red",0}, {"Yellow",0}, {"Pink",0}, {"No Color",0}
};

// Control flags (can be toggled from web)
bool servosEnabled = true;   // master servo switch
bool buzzerEnabled = true;
bool voiceEnabled = true;    // kept server-side for reference though TTS is client-side

// Sensor readings
int R=0, G=0, B=0;

// Calibration references (editable from dashboard)
struct ColorRef {
  String name;
  int r, g, b;
};
std::vector<ColorRef> references = {
  {"Blue",   50, 60, 52},
  {"Red",    40, 66, 63},
  {"Yellow", 40, 55, 70},
  {"Pink",   44, 70, 59},
  {"No Color",   37, 56, 56},
};

// ---------- HTML page (dashboard) ----------
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
  <title>SpectraBot - Color Sorting Robot</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Poppins:wght@300;400;600&display=swap');

  :root {
    --bg1: #071028;
    --bg2: #030612;
    --accent: #00c6ff;
    --card: rgba(255,255,255,0.03);
  }

  body {
    font-family: 'Poppins', sans-serif;
    background: radial-gradient(circle at center, var(--bg1), var(--bg2));
    color: #e6eef8;
    margin: 0;
    padding: 0;
  }

  .app {
    display: flex;
    min-height: 100vh;
  }

  /* Sidebar */
  .sidebar {
    width: 72px;
    background: linear-gradient(180deg, #0b0e23, #1b1f36);
    padding: 16px;
    box-shadow: 2px 0 18px rgba(0, 0, 0, 0.6);
    transition: width 0.7s ease;
  }

  .sidebar.expanded {
    width: 150px;
  }

  .sb-item {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin: 12px 0;
    color: #cfe6ff;
  }

  .sb-label {
    margin-left: 12px;
    opacity: 0;
    transition: opacity 0.25s;
  }

  .sidebar.expanded .sb-label {
    opacity: 1;
  }

  /* Modern Switch Style */
  .switch {
    position: relative;
    width: 44px;
    height: 24px;
  }

  .switch input {
    opacity: 0;
    width: 0;
    height: 0;
  }

  .slider {
    position: absolute;
    cursor: pointer;
    top: 0;
    left: 0;
    right: 0;
    bottom: 0;
    background: #bbb;
    transition: .7s;
    border-radius: 34px;
  }

  .slider:before {
    position: absolute;
    content: "";
    height: 18px;
    width: 18px;
    left: 3px;
    bottom: 3px;
    background: white;
    transition: .7s;
    border-radius: 50%;
  }

  input:checked + .slider {
    background: linear-gradient(135deg, #00c6ff, #0072ff);
  }

  input:checked + .slider:before {
    transform: translateX(20px);
  }

  /* Main */
  .main {
    flex: 1;
    padding: 26px 30px;
  }

  header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding-bottom: 18px;
  }

  h1 {
    margin: 0;
    font-size: 40px;
    text-align: center;
  }

  /* Grid layout */
  .grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(340px, 1fr));
    gap: 20px;
    align-items: center;
  }

  .card {
    background: linear-gradient(180deg, rgba(255,255,255,0.02), rgba(255,255,255,0.01));
    border-radius: 16px;
    padding: 22px;
    box-shadow: 0 8px 30px rgba(0,0,0,0.45);
    display: flex;
    flex-direction: column;
    justify-content: space-between;
  }

  .center {
    text-align: center;
  }

  .color-circle {
    width: 150px;
    height: 150px;
    border-radius: 50%;
    margin: 0 auto;
    box-shadow: 0 0 35px rgba(0,0,0,0.25);
  }

  .color-name {
    margin-top: 12px;
    font-weight: 600;
    font-size: 50px;
  }

  .btn {
    margin-top: 14px;
    padding: 10px 16px;
    border-radius: 999px;
    border: none;
    background: linear-gradient(135deg, #00c6ff, #0072ff);
    color: white;
    font-weight: 600;
    cursor: pointer;
  }

  table {
    width: 100%;
    border-collapse: collapse;
    margin-top: 10px;
    font-size: 15px;
  }

  th, td {
    padding: 14px;
    text-align: center;
    color: #cfe6ff;
  }

  th {
    color: #8fd3ff;
  }

  .cal-row {
    display: flex;
    gap: 200px;
    margin-bottom: 10px;
    align-items: center;
  }

  .cal-row input {
    width: 180px;
    padding: 6px;
    border-radius: 6px;
    border: none;
  }

  .small {
    font-size: 15px;
    color: #9fcfff;
    margin-top: 8px;
  }

  .topbar {
    display: flex;
    gap: 10px;
    align-items: center;
  }

  .toggle-expander {
    background: transparent;
    border: none;
    color: var(--accent);
    font-size: 20px;
    width: 50px;
    height: 50px;
    cursor: pointer;
  }

  /* Equal height for main cards */
  .grid > .card:nth-child(-n+3) {
    min-height: 400px;
  }

  /* Calibration card spans full width */
  .grid > .card:last-child {
    grid-column: 1 / -1;
  }

  @media(max-width:500px) {
    .sidebar {
      display: none;
    }
    .main {
      padding: 16px;
    }
  }
</style>

</head>
<body>
<div class="app">
  <div class="sidebar" id="sidebar">
    <div style="display:flex;align-items:left;justify-content:left;height:36px; margin-top: 230px;"></div>

    <div class="sb-item">
      <div style="display:flex;align-items:center;">
        <label class="switch"><input type="checkbox" id="servosToggle" checked><span class="slider"></span></label>
        <span class="sb-label">Servos</span>
      </div>
    </div>

    <div class="sb-item">
      <div style="display:flex;align-items:center;">
        <label class="switch"><input type="checkbox" id="buzzerToggle" checked><span class="slider"></span></label>
        <span class="sb-label">Buzzer</span>
      </div>
    </div>

    <div class="sb-item">
      <div style="display:flex;align-items:center;">
        <label class="switch"><input type="checkbox" id="voiceToggle" checked><span class="slider"></span></label>
        <span class="sb-label">Voice</span>
      </div>
    </div>

    <div style="margin-top:18px;padding:0 6px; margin-bottom: 30px;">
      <button id="expandBtn" class="btn" style="width:100%;padding:8px 10px;background:transparent;border:1px solid rgba(255,255,255,0.06);"> CLICK </button>
    </div>
  </div>

  <div class="main">
    <header>
      <h1> SpectraBot - Color Sorting Robot</h1>
      <div class="topbar">
        <div class="small">IP: <span id="ip">...</span></div>
        <button class="btn" id="resetBtn">Reset Counts</button>
      </div>
    </header>

    <div class="grid">
      <!-- Latest Detected Color -->
      <div class="card center">
        <h2>Latest Detected Color</h2>
        <div class="color-circle" id="colorCircle" style="background:#000"></div>
        <div class="color-name" id="colorName">Waiting...</div>
        <div class="small">RGB: <span id="rgbVal">-</span></div>
      </div>

      <!-- Color Detection Counts -->
      <div class="card center">
        <h2>Color Detection Counts</h2>
        <table>
          <thead><tr><th>Color</th><th>Count</th></tr></thead>
          <tbody id="countTable"></tbody>
        </table>
      </div>

      <!-- Color Chart -->
      <div class="card center">
        <h2>Color Chart</h2>
        <canvas id="colorChart" style="max-width:100%;height:300px"></canvas>
        <div class="small">Chart updates live</div>
      </div>

      <!-- Calibration -->
      <div class="card">
        <h2>Calibration (Edit & Save)</h2>
        <div id="calArea"></div>
        <button class="btn" id="saveCal">Save Calibration</button>
        <div class="small">Edit R,G,B values for each color and click Save to send to device.</div>
      </div>
    </div>
  </div>
</div>

<script>
/* Client-side JavaScript */
const ws = new WebSocket(`ws://${location.hostname}/ws`);
const ipSpan = document.getElementById('ip');
ipSpan.textContent = location.hostname;

const colorLabels = ["Blue","Red","Yellow","Pink","No Color"];
const colorMap = {"Blue":"#2196F3","Red":"#F44336","Yellow":"#FFEB3B","Pink":"#E91E63","No Color":"#9E9E9E"};

let chart;

// populate chart---------------------------------------------------------------------------
function initChart(){
  const ctx = document.getElementById('colorChart').getContext('2d');
  chart = new Chart(ctx, {
    type:'bar',
    data:{ 
      labels: colorLabels,
      datasets:[{ 
        label:'Counts', 
        data: Array(colorLabels.length).fill(0), 
        backgroundColor:Object.values(colorMap) 
      }] 
    },
    options:{ scales:{ y:{ beginAtZero:true } } }
  });
}
initChart();

// --- helper to sanitize names into valid element IDs ---
function safeName(name){
  return name.replace(/\s+/g, '_'); // replace spaces with underscore
}

// create calibration inputs area
function renderCalibration(cal){
  const area = document.getElementById('calArea');
  area.innerHTML = '';
  cal.forEach(item=>{
    const idSafe = safeName(item.name);
    const row = document.createElement('div');
    row.className='cal-row';
    row.innerHTML = `
      <div style="width:150px">${item.name}</div>
      <input type="number" min="0" id="r_${idSafe}" value="${item.r}">
      <input type="number" min="0" id="g_${idSafe}" value="${item.g}">
      <input type="number" min="0" id="b_${idSafe}" value="${item.b}">
    `;
    area.appendChild(row);
  });
}

// send calibration data
document.getElementById('saveCal').addEventListener('click', ()=>{
  const cal = [];
  const inputs = document.querySelectorAll('#calArea .cal-row');
  inputs.forEach(row=>{
    const label = row.querySelector('div').textContent.trim();
    const idSafe = safeName(label);
    const rEl = document.getElementById('r_'+idSafe);
    const gEl = document.getElementById('g_'+idSafe);
    const bEl = document.getElementById('b_'+idSafe);
    const r = rEl ? parseInt(rEl.value) || 0 : 0;
    const g = gEl ? parseInt(gEl.value) || 0 : 0;
    const b = bEl ? parseInt(bEl.value) || 0 : 0;
    cal.push({name: label, r, g, b});
  });

  // show JSON in console for debugging
  console.log("Sending calibration:", JSON.stringify({type:'calibrate', payload:cal}));

  ws.send(JSON.stringify({type:'calibrate', payload:cal}));
  alert('Calibration sent to device.');
});

// toggles
document.getElementById('servosToggle').addEventListener('change', e=>{
  ws.send(JSON.stringify({type:'toggle', target:'servos', value: e.target.checked?1:0}));
});
document.getElementById('buzzerToggle').addEventListener('change', e=>{
  ws.send(JSON.stringify({type:'toggle', target:'buzzer', value: e.target.checked?1:0}));
});
document.getElementById('voiceToggle').addEventListener('change', e=>{
  ws.send(JSON.stringify({type:'toggle', target:'voice', value: e.target.checked?1:0}));
});

// reset
document.getElementById('resetBtn').addEventListener('click', ()=>{
  ws.send(JSON.stringify({type:'reset'}));
});

// expand sidebar
const sidebar = document.getElementById('sidebar');
document.getElementById('expandBtn').addEventListener('click', ()=>{
  sidebar.classList.toggle('expanded');
  document.getElementById('expandBtn').textContent = sidebar.classList.contains('expanded') ? 'HIDE' : 'CLICK';
});

// handle messages from server
ws.onmessage = (evt) => {
  const data = JSON.parse(evt.data);
  if(data.type === 'calibration'){
    renderCalibration(data.payload);
    return;
  }
  if(data.type === 'state'){
    const color = data.color || 'No Color Detected';
    const counts = data.counts || {};
    document.getElementById('colorCircle').style.background = colorMap[color] || '#000';
    document.getElementById('colorName').textContent = color;
    document.getElementById('rgbVal').textContent = data.rgb || '-';

    // table
    const tbody = document.getElementById('countTable'); tbody.innerHTML = '';
    colorLabels.forEach(c=>{
      const ct = counts[c] || 0;
      const tr = `<tr><td style="text-align:center">${c}</td><td>${ct}</td></tr>`;
      tbody.innerHTML += tr;
      chart.data.datasets[0].data[colorLabels.indexOf(c)] = ct;
    });
    chart.update();

    // voice (client-side TTS)
    const voiceToggle = document.getElementById('voiceToggle');
    if(voiceToggle.checked){
      const msg = new SpeechSynthesisUtterance(color + ' detected');
      msg.lang = 'en-US'; msg.rate = 1; msg.pitch = 1;
      speechSynthesis.speak(msg);
    }
  }
};

ws.onopen = ()=>{ ws.send(JSON.stringify({type:'hello'})); };
ws.onerror = (e)=> console.warn('WebSocket error', e);
</script>
</body>
</html>
)rawliteral";

// ---------- Helper: notify clients with current state ----------
void notifyClients() {
  // Build counts JSON (send numbers, not strings)
  String countsJson = "";
  bool first = true;
  for(auto &entry : colorCounts){
    if(!first) countsJson += ",";
    countsJson += "\"" + entry.first + "\":" + String(entry.second);
    first = false;
  }
  String rgbstr = String(R) + "," + String(G) + "," + String(B);
  String payload = "{\"type\":\"state\",\"color\":\"" + detectedColor + "\",\"counts\":{"+countsJson+"},\"rgb\":\""+rgbstr+"\"}";
  ws.textAll(payload);
}

// ---------- Read TCS3200 (same approach as yours) ----------
void readRGB() {
  digitalWrite(S2, LOW); 
  digitalWrite(S3, LOW);    
  R = pulseIn(sensorOut, LOW); 

  digitalWrite(S2, HIGH); 
  digitalWrite(S3, HIGH); 
  G = pulseIn(sensorOut, LOW);

  digitalWrite(S2, LOW); 
  digitalWrite(S3, HIGH);  
  B = pulseIn(sensorOut, LOW);

  Serial.printf("R=%d G=%d B=%d\n", R, G, B);
}

// ---------- Classify by nearest reference ----------
String classifyColorFromRefs() {
  long bestDiff = LONG_MAX;
  String best = "No Color Detected";
  for(auto &ref : references){
    long diff = abs(R - ref.r) + abs(G - ref.g) + abs(B - ref.b);
    if(diff < bestDiff){
      bestDiff = diff;
      best = ref.name;
    }
  }
  return best;
}

// ---------- WebSocket event handler ----------
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len){
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if(info->opcode != WS_TEXT) return;
  String msg = String((char*)data).substring(0,len);

  // parse simple JSON-ish messages from client
  // We don't include a complete JSON parser (avoid ArduinoJson here), instead do lightweight checks
  if(msg.indexOf("\"type\":\"hello\"") >= 0 || msg.indexOf("{\"type\":\"hello\"") >= 0){
    // send calibration and state immediately
    // calibration
    String cal = "{\"type\":\"calibration\",\"payload\":[";
    for(size_t i=0;i<references.size();++i){
      auto &r = references[i];
      if(i) cal += ",";
      cal += "{\"name\":\""+r.name+"\",\"r\":"+String(r.r)+",\"g\":"+String(r.g)+",\"b\":"+String(r.b)+"}";
    }
    cal += "]}";
    ws.textAll(cal);
    // state will be sent by notifyClients shortly
    notifyClients();
    return;
  }

  // reset
  if(msg.indexOf("\"type\":\"reset\"") >= 0){
    for(auto &e : colorCounts) e.second = 0;
    detectedColor = "None";
    notifyClients();
    return;
  }

  // toggle message: {"type":"toggle","target":"buzzer","value":1}
  if(msg.indexOf("\"type\":\"toggle\"") >= 0){
    if(msg.indexOf("\"target\":\"servos\"") >= 0){
      servosEnabled = (msg.indexOf("\"value\":1") >= 0);
      if(!servosEnabled){
        topServo.detach();
        bottomServo.detach();
      } else {
        topServo.attach(TOP_SERVO_PIN);
        bottomServo.attach(BOTTOM_SERVO_PIN);
      }
    } else if(msg.indexOf("\"target\":\"buzzer\"") >= 0){
      buzzerEnabled = (msg.indexOf("\"value\":1") >= 0);
    } else if(msg.indexOf("\"target\":\"voice\"") >= 0){
      voiceEnabled = (msg.indexOf("\"value\":1") >= 0);
      // voiceEnabled stored server-side for persistence; actual TTS runs on client
    }
    notifyClients();
    return;
  }

  // calibration payload: {"type":"calibrate","payload":[{"name":"Blue","r":60,"g":70,"b":90},...]}
  if(msg.indexOf("\"type\":\"calibrate\"") >= 0 || msg.indexOf("\"type\":\"calibration\"") >= 0){
    // lightweight parse: find occurrences of {"name":"X","r":n,"g":n,"b":n}
    // We'll iterate over known color names and extract r/g/b values if present
    for(auto &ref : references){
      String nameKey = "\"name\":\"" + ref.name + "\"";
      int pos = msg.indexOf(nameKey);
      if(pos >= 0){
        // find "r":value after pos
        int rpos = msg.indexOf("\"r\":", pos);
        int gpos = msg.indexOf("\"g\":", pos);
        int bpos = msg.indexOf("\"b\":", pos);
        if(rpos >= 0 && gpos >= 0 && bpos >= 0){
          int rend = msg.indexOf(",", rpos);
          if(rend < 0) rend = msg.indexOf("}", rpos);
          int gend = msg.indexOf(",", gpos);
          if(gend < 0) gend = msg.indexOf("}", gpos);
          int bend = msg.indexOf(",", bpos);
          if(bend < 0) bend = msg.indexOf("}", bpos);
          if(rend>rpos && gend>gpos && bend>bpos){
            String rs = msg.substring(rpos+4, rend);
            String gs = msg.substring(gpos+4, gend);
            String bs = msg.substring(bpos+4, bend);
            ref.r = rs.toInt();
            ref.g = gs.toInt();
            ref.b = bs.toInt();
            Serial.printf("Cal updated %s -> r:%d g:%d b:%d\n", ref.name.c_str(), ref.r, ref.g, ref.b);
          }
        }
      }
    }
    notifyClients();
    return;
  }
}

// AsyncWebSocket event
void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    Serial.printf("WS client %u connected\n", client->id());
    // send calibration + state on connect
    handleWebSocketMessage((void*)"{\"type\":\"hello\"}", (uint8_t*)"{\"type\":\"hello\"}", strlen("{\"type\":\"hello\"}"));
  } else if(type == WS_EVT_DISCONNECT){
    Serial.printf("WS client %u disconnected\n", client->id());
  } else if(type == WS_EVT_DATA){
    handleWebSocketMessage(arg, data, len);
  }
}

// ---------- setup ----------
void setup(){
  Serial.begin(115200);

  pinMode(S0,OUTPUT); pinMode(S1,OUTPUT); pinMode(S2,OUTPUT); pinMode(S3,OUTPUT);
  pinMode(sensorOut,INPUT);
  digitalWrite(S0,HIGH); digitalWrite(S1,LOW); // set frequency scaling

  pinMode(buzzerPin,OUTPUT);
  pinMode(resetButtonPin, INPUT_PULLUP);

  ledcSetup(RED_CH, 5000, 8);
  ledcSetup(GREEN_CH, 5000, 8);
  ledcSetup(BLUE_CH, 5000, 8);
  ledcAttachPin(RED_LED_PIN, RED_CH);
  ledcAttachPin(GREEN_LED_PIN, GREEN_CH);
  ledcAttachPin(BLUE_LED_PIN, BLUE_CH);

  topServo.attach(TOP_SERVO_PIN);
  bottomServo.attach(BOTTOM_SERVO_PIN);

  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  while(WiFi.status() != WL_CONNECTED){
    delay(400);
    Serial.print(".");
  }
  Serial.println("\nConnected. IP: "); Serial.println(WiFi.localIP());

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send_P(200, "text/html", index_html);
  });
  server.begin();
}

// ---------- beep helper ----------
void beepN(int times){
  if(!buzzerEnabled)
    return;

  for(int i=0;i<times;i++){
    digitalWrite(buzzerPin, HIGH);
    delay(120);

    digitalWrite(buzzerPin, LOW);
    delay(120);
  }
}

// ---------- set RGB LED ----------
void setRGB(int r, int g, int b){
  ledcWrite(RED_CH, r);
  ledcWrite(GREEN_CH, g);
  ledcWrite(BLUE_CH, b);
}

// ---------- main loop ----------
void loop(){
  ws.cleanupClients();

  // physical reset button (optional)
  if(digitalRead(resetButtonPin) == LOW){
    for(auto &e : colorCounts) e.second = 0;
    detectedColor = "None";
    notifyClients();
    delay(600); // debounce + avoid too fast repeat
  }

  // if servos disabled, detach but still respond to toggles
  if(!servosEnabled){
    // ensure servos detached to avoid jitter
    topServo.detach();
    bottomServo.detach();
    // still let clients know state periodically
    notifyClients();
    delay(250);
    return;
  } else {
    // ensure servos attached if enabled
    if(!topServo.attached()) topServo.attach(TOP_SERVO_PIN);
    if(!bottomServo.attached()) bottomServo.attach(BOTTOM_SERVO_PIN);
  }

  // Move top servo to drop pebble 
  topServo.write(95);
  delay(1000);
  
  for(int i=95;i>30;i--){ 
    topServo.write(i); 
    delay(5); 
  }

  delay(400);

  // Read sensor 
  readRGB();
  detectedColor = classifyColorFromRefs();

  // Update LED, bottom servo, buzzer (only when servo enabled and bottom servo enabled)
  if(detectedColor == "Blue"){ 
    setRGB(0,0,255); 
    bottomServo.write(50); 
    beepN(1); 
  }
  else if(detectedColor == "Red"){ 
    setRGB(255,0,0); 
    bottomServo.write(75); 
    beepN(1); 
  }
  else if(detectedColor == "Yellow"){ 
    setRGB(255,255,0); 
    bottomServo.write(100); 
    beepN(1); 
  }
  else if(detectedColor == "Pink"){ 
    setRGB(255,105,180); 
    bottomServo.write(125); 
    beepN(1); 
  }
  else if(detectedColor == "No Color"){ 
    setRGB(0,0,0); 
  }

  // increment count
  if(colorCounts.find(detectedColor) != colorCounts.end()) colorCounts[detectedColor]++;
  else colorCounts["No Color Detected"]++;

  notifyClients();

  // Reset top servo to resting position
  delay(1000);
  for(int i=30;i>=0;i--){ 
    topServo.write(i); 
    delay(5); 
  }

  delay(200);

  for(int i=0;i<=95;i++){ 
    topServo.write(i); 
    delay(5); 
  }
}
