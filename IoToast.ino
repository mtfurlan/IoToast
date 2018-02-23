#include <Arduino.h>
#include <HLW8012.h>
#include <mqtt-wrapper.h>

// GPIOs
#define RELAY_PIN                       12
#define SEL_PIN                         5
#define CF1_PIN                         13
#define CF_PIN                          14
#define BUZZER 3
// or 13
#define LED 15
#define BUTTON 0

// Check values every 10 seconds
/*
#define STATUS_INTERVAL 1000UL
/*/
#define STATUS_INTERVAL 420000UL
// */

// Set SEL_PIN to HIGH to sample current
// This is the case for Itead's Sonoff POW, where a
// the SEL_PIN drives a transistor that pulls down
// the SEL pin in the HLW8012 when closed
#define CURRENT_MODE                    HIGH

// These are the nominal values for the resistors in the circuit
#define CURRENT_RESISTOR                0.001
#define VOLTAGE_RESISTOR_UPSTREAM       ( 5 * 470000 ) // Real: 2280k
#define VOLTAGE_RESISTOR_DOWNSTREAM     ( 1000 ) // Real 1.009k

const char* host_name = "IoToast";
const char* ssid = "i3detroit";
const char* password = "i3detroit";
const char* mqtt_server = "10.13.0.22";
const int mqtt_port = 1883;
const char* fullTopic = "i3/test/IoToast";

struct mqtt_wrapper_options mqtt_options;

HLW8012 hlw8012;

char buf[1024];
char topicBuf[1024];

//said you can't check faster than twice interrrupt number or something.
//Somethingsomething default 2 seconds.
static unsigned long checkInterval = 3000UL;

// Toggle buzzer every buzz_delay_us, for a duration of buzz_length_ms.

void buzz_sound(long buzz_length_ms, int buzz_delay_us) {
  // Convert total play time from milliseconds to microseconds
  long buzz_length_us = buzz_length_ms * (long)1000;
  // Loop until the remaining play time is less than a single buzz_delay_us
  while (buzz_length_us > (buzz_delay_us * 2)) {
    buzz_length_us -= 200000; //buzz_delay_us * 2; //Decrease the remaining play time
    // Toggle the buzzer at various speeds
    digitalWrite(BUZZER, LOW);
    // delayMicroseconds(buzz_delay_us);
    // //digitalWrite(BUZZER, HIGH);
    // delayMicroseconds(buzz_delay_us);
    delay(1);
    digitalWrite(BUZZER, HIGH);
    delay(1);
  }
  digitalWrite(BUZZER, LOW);
}

// When using interrupts we have to call the library entry point
// whenever an interrupt is triggered
void ICACHE_RAM_ATTR hlw8012_cf1_interrupt() {
  hlw8012.cf1_interrupt();
}
void ICACHE_RAM_ATTR hlw8012_cf_interrupt() {
  hlw8012.cf_interrupt();
}

// Library expects an interrupt on both edges
void setInterrupts() {
  attachInterrupt(CF1_PIN, hlw8012_cf1_interrupt, CHANGE);
  attachInterrupt(CF_PIN, hlw8012_cf_interrupt, CHANGE);
}

void setup() {
  //Serial.begin(115200);

  mqtt_options.connectedLoop = connectedLoop;
  mqtt_options.callback = callback;
  mqtt_options.connectSuccess = connectSuccess;
  mqtt_options.ssid = ssid;
  mqtt_options.password = password;
  mqtt_options.mqtt_server = mqtt_server;
  mqtt_options.mqtt_port = mqtt_port;
  mqtt_options.host_name = host_name;
  mqtt_options.fullTopic = fullTopic;
  setup_mqtt(&mqtt_options);

  // Close the relay to switch on the load
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  // Initialize HLW8012
  // void begin(unsigned char cf_pin, unsigned char cf1_pin, unsigned char sel_pin, unsigned char currentWhen = HIGH, bool use_interrupts = false, unsigned long pulse_timeout = PULSE_TIMEOUT);
  // * cf_pin, cf1_pin and sel_pin are GPIOs to the HLW8012 IC
  // * currentWhen is the value in sel_pin to select current sampling
  // * set use_interrupts to true to use interrupts to monitor pulse widths
  // * leave pulse_timeout to the default value, recommended when using interrupts

  hlw8012.begin(CF_PIN, CF1_PIN, SEL_PIN, CURRENT_MODE, true);
  //hlw8012.begin(CF_PIN, CF1_PIN, SEL_PIN, CURRENT_MODE, false, 500000);


  // These values are used to calculate current, voltage and power factors as per datasheet formula
  // These are the nominal values for the Sonoff POW resistors:
  // * The CURRENT_RESISTOR is the 1milliOhm copper-manganese resistor in series with the main line
  // * The VOLTAGE_RESISTOR_UPSTREAM are the 5 470kOhm resistors in the voltage divider that feeds the V2P pin in the HLW8012
  // * The VOLTAGE_RESISTOR_DOWNSTREAM is the 1kOhm resistor in the voltage divider that feeds the V2P pin in the HLW8012
  hlw8012.setResistors(CURRENT_RESISTOR, VOLTAGE_RESISTOR_UPSTREAM, VOLTAGE_RESISTOR_DOWNSTREAM);

  //Measured using load balancer and code in example
  hlw8012.setCurrentMultiplier(16309.9799);
  hlw8012.setVoltageMultiplier(458283.9398);
  hlw8012.setPowerMultiplier(12759947.2634);

  setInterrupts();

}

void callback(char* topic, byte* payload, unsigned int length, PubSubClient *client) {
}

void connectSuccess(PubSubClient* client, char* ip) {
}

void connectedLoop(PubSubClient* client) {
  static unsigned long nextCheck = 0UL;
  static bool lastState = false;
  //said you can't check faster than twice interrrupt number or something.
  //Somethingsomething default 2 seconds.

  if( (long)( millis() - nextCheck ) >= 0) {
    nextCheck = millis() + checkInterval;
    sprintf(topicBuf, "stat/%s/check", fullTopic);
    client->publish(topicBuf, "check?");

    static unsigned long nextStatus = 0UL;

    if( (long)( millis() - nextStatus ) >= 0) {
      nextStatus = millis() + STATUS_INTERVAL;
      // Serial.print("[HLW] Active Power (W)    : "); Serial.println(hlw8012.getActivePower());
      // Serial.print("[HLW] Voltage (V)         : "); Serial.println(hlw8012.getVoltage());
      // Serial.print("[HLW] Current (A)         : "); Serial.println(hlw8012.getCurrent());
      // Serial.print("[HLW] Apparent Power (VA) : "); Serial.println(hlw8012.getApparentPower());
      // Serial.print("[HLW] Power Factor (%)    : "); Serial.println((int) (100 * hlw8012.getPowerFactor()));
      // Serial.print("[HLW] Agg. energy (Ws)    : "); Serial.println(hlw8012.getEnergy());
      // Serial.println();

      sprintf(topicBuf, "stat/%s/active-power", fullTopic);
      dtostrf(hlw8012.getActivePower(), 1023, 2, buf);
      client->publish(topicBuf, buf);

      sprintf(topicBuf, "stat/%s/voltage", fullTopic);
      dtostrf(hlw8012.getVoltage(), 1023, 2, buf);
      client->publish(topicBuf, buf);

      sprintf(topicBuf, "stat/%s/currentA", fullTopic);
      dtostrf(hlw8012.getCurrent(), 1023, 2, buf);
      client->publish(topicBuf, buf);

      sprintf(topicBuf, "stat/%s/apparent-power", fullTopic);
      dtostrf(hlw8012.getApparentPower(), 1023, 2, buf);
      client->publish(topicBuf, buf);

      sprintf(topicBuf, "stat/%s/power-factor", fullTopic);
      sprintf(buf, "%d", (int) (100 * hlw8012.getPowerFactor()));
      client->publish(topicBuf, buf);

      sprintf(topicBuf, "stat/%s/energy", fullTopic);
      dtostrf(hlw8012.getEnergy(), 1023, 2, buf);
      client->publish(topicBuf, buf);

    }

    if(hlw8012.getCurrent() < 0.04) {
      if(lastState == true) {
        sprintf(topicBuf, "stat/%s/done", fullTopic);
        client->publish(topicBuf, "ding?\a");
        dtostrf(hlw8012.getCurrent(), 1023, 2, buf);
        client->publish(topicBuf, buf);
      }
    }
  }
}

void loop() {
  loop_mqtt();
  static unsigned long nextCheck = 0UL;
  static bool lastState = false;

  if( (long)( millis() - nextCheck ) >= 0) {
    nextCheck = millis() + checkInterval;

    //IMPORTANT FOR REASONS
    hlw8012.getPowerFactor();

    if(hlw8012.getCurrent() < 0.04) {
      if(lastState == true) {
        //falling edge buzz
        buzz_sound(15000, 1500);
        lastState = false;
        digitalWrite(LED, 0);
      }
    } else {
      lastState = true;
      digitalWrite(LED, 1);
    }
  }
}
