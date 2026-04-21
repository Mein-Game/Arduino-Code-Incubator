#include <OneWire.h>
#include <DallasTemperature.h>

const uint8_t PIN_ONEWIRE = 7;
const uint8_t PIN_HEATER  = 10;
const uint8_t PIN_PUMP    = 9;   // <-- cooling pump / fan
const uint8_t PIN_VALVE   = 8;   // not used, kept OFF

double setpointC = 37.0;

const unsigned long WINDOW_MS = 1000;
const double SAFETY_HIGH_C = 50.0;

// -------- Cooling parameters (tune these) --------
const double COOL_START_C = 0.30;     // start cooling if T > setpoint + 0.30
const double COOL_STOP_C  = 0.10;     // stop cooling  if T <= setpoint + 0.10
const unsigned long PUMP_MIN_ON_MS  = 10000;  // avoid chattering
const unsigned long PUMP_MIN_OFF_MS = 10000;

// -------- HOLD parameters (as you had) --------
const unsigned long HOLD_PULSE_MS = 300;
const unsigned long HOLD_LOCKOUT_MS = 1500;
const double HOLD_TRIGGER_C = 0.03;     // pulse if T <= setpoint - 0.03
const double HOLD_SLOPE_GATE = 0.001;   // dT/dt gate

OneWire oneWire(PIN_ONEWIRE);
DallasTemperature sensors(&oneWire);

enum Phase { WARMUP, HOLD, COOL };
Phase phase = WARMUP;

unsigned long windowStart = 0;
unsigned long lastPrint = 0;

// HOLD pulse + lockout
bool inHoldPulse=false;
unsigned long holdPulseStart=0;
unsigned long lockoutUntil=0;

// Cooling pump timing
bool pumpOn = false;
unsigned long pumpLastChange = 0; // ms of last ON/OFF transition

// dT/dt
double dTdtFilt=0;
double lastMean=NAN;
unsigned long lastSlope=0;

void heater(bool on){ digitalWrite(PIN_HEATER,on?HIGH:LOW); }
void pump(bool on)  { digitalWrite(PIN_PUMP,  on?HIGH:LOW); pumpOn = on; }

bool readT(double &t1,double &t2,double &m){
  sensors.requestTemperatures();
  t1=sensors.getTempCByIndex(0);
  t2=sensors.getTempCByIndex(1);
  if(t1==DEVICE_DISCONNECTED_C || t2==DEVICE_DISCONNECTED_C) return false;
  m=(t1+t2)/2.0;
  return true;
}

double warmupDuty(double T){
  double err=setpointC-T;
  if(err>10) return 0.70;
  if(err>5)  return 0.55;
  if(err>2)  return 0.35;
  if(err>0.7)return 0.20;
  return 0.12;   // tuned from your data
}

bool canTurnPumpOn(unsigned long now){
  return (!pumpOn) && (now - pumpLastChange >= PUMP_MIN_OFF_MS);
}
bool canTurnPumpOff(unsigned long now){
  return (pumpOn) && (now - pumpLastChange >= PUMP_MIN_ON_MS);
}

void setup(){
  Serial.begin(115200);
  pinMode(PIN_HEATER,OUTPUT);
  pinMode(PIN_PUMP,OUTPUT);
  pinMode(PIN_VALVE,OUTPUT);

  digitalWrite(PIN_VALVE,LOW);
  heater(false);
  pump(false);
  pumpLastChange = millis();

  sensors.begin();
  windowStart=millis();

  Serial.println("ms,phase,Tmean,setpoint,dTdt_filt,heater,pump");
}

void loop(){
  unsigned long now=millis();

  double t1,t2,T;
  if(!readT(t1,t2,T)){
    heater(false); pump(false);
    return;
  }
  if(T>SAFETY_HIGH_C){
    heater(false); pump(true); // optional: force cooling if safety exceeded
    return;
  }

  // slope update (every 5s)
  if(lastSlope==0){ lastSlope=now; lastMean=T; }
  if(now-lastSlope>5000){
    double slope=(T-lastMean)/((now-lastSlope)/1000.0);
    dTdtFilt=0.4*slope+0.6*dTdtFilt;
    lastMean=T;
    lastSlope=now;
  }

  // -------- Mode decision: COOL has priority if above setpoint --------
  bool wantCool = (T > setpointC + COOL_START_C);
  bool coolDone = (T <= setpointC + COOL_STOP_C);

  if (phase != COOL && wantCool) {
    phase = COOL;
    inHoldPulse = false;
  }
  if (phase == COOL && coolDone) {
    // After cooling, if we're still above setpoint, we can go HOLD (heater off anyway),
    // but typically you'll be near setpoint so HOLD is fine.
    phase = HOLD;
  }

  bool heaterOn=false;

  if(phase == COOL){
    // Heater OFF always in cool
    heaterOn = false;

    // Pump ON with min timing rules
    if (!pumpOn && canTurnPumpOn(now)) {
      pump(true); pumpLastChange = now;
    }
    // keep it ON until coolDone AND min ON time satisfied
    if (pumpOn && coolDone && canTurnPumpOff(now)) {
      pump(false); pumpLastChange = now;
    }
  }
  else {
    // Not cooling -> pump OFF (unless min-on prevents immediate off)
    if (pumpOn && canTurnPumpOff(now)) {
      pump(false); pumpLastChange = now;
    }

    // Your original HEAT logic
    if(phase==WARMUP){
      if(T>=setpointC) heaterOn=false;
      else{
        double duty=warmupDuty(T);
        // robust window alignment
        if(now - windowStart >= WINDOW_MS){
          windowStart += ((now - windowStart) / WINDOW_MS) * WINDOW_MS;
        }
        heaterOn = ((now - windowStart) < (unsigned long)(WINDOW_MS*duty));
      }
      // switch to HOLD early
      if(T >= setpointC - 0.7) phase = HOLD;
    }
    else { // HOLD
      if(T>=setpointC){ heaterOn=false; inHoldPulse=false; }
      else{
        if(inHoldPulse){
          if(now-holdPulseStart < HOLD_PULSE_MS) heaterOn=true;
          else { inHoldPulse=false; heaterOn=false; }
        } else {
          if(now>lockoutUntil &&
             T <= setpointC - HOLD_TRIGGER_C &&
             dTdtFilt <= HOLD_SLOPE_GATE){

            inHoldPulse=true;
            holdPulseStart=now;
            lockoutUntil=now + HOLD_LOCKOUT_MS;
            heaterOn=true;
          }
        }
      }
    }
  }

  heater(heaterOn);

  // log every 3s
  if(now-lastPrint>3000){
    lastPrint=now;
    Serial.print(now);Serial.print(",");
    Serial.print(phase==WARMUP?"WARMUP":(phase==HOLD?"HOLD":"COOL"));Serial.print(",");
    Serial.print(T,3);Serial.print(",");
    Serial.print(setpointC,2);Serial.print(",");
    Serial.print(dTdtFilt,6);Serial.print(",");
    Serial.print(heaterOn?"ON":"OFF");Serial.print(",");
    Serial.println(pumpOn?"ON":"OFF");
  }
}
