#include "Adafruit_VL53L0X.h"
#include <WiFi.h>
#include <WebServer.h>

Adafruit_VL53L0X lox = Adafruit_VL53L0X();
WebServer server(80);

const int pin_Trai = 39;
const int pin_Giua = 34;
const int pin_Phai = 35;

const int PWMA = 32;  
const int AIN1 = 25; 
const int AIN2 = 26; 

const int PWMB = 33; 
const int BIN1 = 27;
const int BIN2 = 13; 

float Kp = 45.0; 
float Ki = 0.0;  
float Kd = 25.0; 

int P = 0, I = 0, D = 0, PID_value = 0;
int previous_error = 0;
int base_speed = 90; 
int last_error = 0;

const char* html_page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Bảng Điều Khiển PID</title>
  <style>
    body { font-family: Arial, sans-serif; text-align: center; margin-top: 30px; background-color: #f4f4f9; }
    .card { background: white; padding: 20px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.2); max-width: 400px; margin: auto; }
    h2 { color: #333; }
    input[type=range] { width: 100%; margin: 10px 0; }
    .val { font-weight: bold; color: #d9534f; }
  </style>
  <script>
    function updatePID() {
      var kp = document.getElementById('kp').value;
      var ki = document.getElementById('ki').value;
      var kd = document.getElementById('kd').value;
      
      document.getElementById('kp_val').innerText = kp;
      document.getElementById('ki_val').innerText = ki;
      document.getElementById('kd_val').innerText = kd;
      
      fetch(`/setPID?kp=${kp}&ki=${ki}&kd=${kd}`);
    }
  </script>
</head>
<body>
  <div class="card">
    <h2>TINH CHỈNH PID</h2>
    <p>Hệ số Kp: <span class="val" id="kp_val">45</span></p>
    <input type="range" id="kp" min="0" max="100" value="45" oninput="updatePID()">
    
    <p>Hệ số Ki: <span class="val" id="ki_val">0</span></p>
    <input type="range" id="ki" min="0" max="10" step="0.1" value="0" oninput="updatePID()">
    
    <p>Hệ số Kd: <span class="val" id="kd_val">25</span></p>
    <input type="range" id="kd" min="0" max="100" value="25" oninput="updatePID()">
  </div>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);

  pinMode(pin_Trai, INPUT);
  pinMode(pin_Giua, INPUT);
  pinMode(pin_Phai, INPUT);

  pinMode(PWMA, OUTPUT); pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
  pinMode(PWMB, OUTPUT); pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);

  if (!lox.begin()) {
    Serial.println(F("Khong tim thay cam bien VL53L0X!"));
  }
  
  Serial.println("Dang phat Wi-Fi...");
  WiFi.softAP("Xe_Do_Line_ESP32", "12345678"); 
  
  server.on("/", []() {
    server.send(200, "text/html", html_page);
  });
  
  server.on("/setPID", []() {
    if (server.hasArg("kp")) Kp = server.arg("kp").toFloat();
    if (server.hasArg("ki")) Ki = server.arg("ki").toFloat();
    if (server.hasArg("kd")) Kd = server.arg("kd").toFloat();
    server.send(200, "text/plain", "Da cap nhat!");
  });

  server.begin(); 
  delay(2000); 
}

int tinh_sai_so() {
  int trai = digitalRead(pin_Trai);
  int giua = digitalRead(pin_Giua);
  int phai = digitalRead(pin_Phai);
  int error = last_error; 

  if      (trai == 0 && giua == 0 && phai == 1) { error = -10; } 
  else if (trai == 1 && giua == 0 && phai == 0) { error = 10; }
  else if (trai == 1 && giua == 0 && phai == 1) { error = 0; }   
  else if (trai == 1 && giua == 1 && phai == 0) { error = 2; }   
  else if (trai == 0 && giua == 1 && phai == 1) { error = -2; } 
  
  last_error = error; 
  return error;
}

void motor_chay_tien(int toc_do_trai, int toc_do_phai) {
  digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW);
  analogWrite(PWMA, toc_do_trai);
  analogWrite(PWMB, toc_do_phai);
}

void motor_xoay_phai_90() {
  digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW); 
  digitalWrite(BIN1, LOW); digitalWrite(BIN2, HIGH); 
  analogWrite(PWMA, 120); 
  analogWrite(PWMB, 120);
}

void motor_xoay_trai_90() {
  digitalWrite(AIN1, LOW); digitalWrite(AIN2, HIGH); 
  digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW); 
  analogWrite(PWMA, 120); 
  analogWrite(PWMB, 120);
}

void motor_dung_khan_cap() {
  analogWrite(PWMA, 0);
  analogWrite(PWMB, 0);
}

void loop() {
  server.handleClient(); 
  VL53L0X_RangingMeasurementData_t measure;
  lox.rangingTest(&measure, false); 
  
  if (measure.RangeStatus != 4 && measure.RangeMilliMeter < 120) {
    motor_dung_khan_cap();
    Serial.println("CO VAT CAN!");
    return; 
  }

  int error = tinh_sai_so();
  if (error == -10) { 
    motor_chay_tien(base_speed, base_speed); 
    delay(50);
    
    unsigned long startTurn = millis();
    while(millis() - startTurn < 250) {
      motor_xoay_trai_90();
      delay(1); 
    }
    
    while (digitalRead(pin_Giua) == 1) {
      motor_xoay_trai_90(); 
      delay(1);
    }    
    last_error = 0; return; 
  }
  
  
  if (error == 10) {
    motor_chay_tien(base_speed, base_speed); 
    delay(50);
    
    unsigned long startTurn = millis();
    while(millis() - startTurn < 250) { 
      motor_xoay_phai_90();
      delay(1);
    }
    
    while (digitalRead(pin_Giua) == 1) { 
      motor_xoay_phai_90(); 
      delay(1);
    }    
    last_error = 0; return; 
  }

  P = error;
  I = I + error;
  D = error - previous_error;
  PID_value = (Kp * P) + (Ki * I) + (Kd * D);
  previous_error = error;
  
  int speed_trai = base_speed + PID_value;
  int speed_phai = base_speed - PID_value;
  speed_trai = constrain(speed_trai, 0, 255);
  speed_phai = constrain(speed_phai, 0, 255);
    
  motor_chay_tien(speed_trai, speed_phai);
}