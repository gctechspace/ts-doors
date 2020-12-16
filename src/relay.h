enum DoorState { DOOR_CLOSED = 0, DOOR_OPENED, MAX_DOOR_STATES };

DoorState doorState = DOOR_CLOSED;
int doorOpenTime = 0;

bool doorChangedState = true;
int doorChangedTime = 0;

void dooropen(){
  doorState = DOOR_OPENED;
  digitalWrite(DOOR_STRIKE_PIN, LOW);   
  messagesend(MQTT_STATE_TOPIC,"OPENED",1 ,false);
  DebugPrintln("Door Open");  
}

void doorclose(){
  doorState = DOOR_CLOSED;
  digitalWrite(DOOR_STRIKE_PIN, HIGH);     
  messagesend(MQTT_STATE_TOPIC,"CLOSED",1 ,false);
  DebugPrintln("Door Close");
}

void DoorToggle(){
 if(doorState == DOOR_CLOSED){
  dooropen();
 }else{
  doorclose();
 }
}

void onDoorStateChange() {
  uint32_t now = millis();

  // Debounce the hall effect state change
  if (now - doorChangedTime > DOOR_HALL_EFFECT_TIMEOUT) {
    // Mark the door state as changed
    doorChangedState = true;
    doorChangedTime = now;
  }
}

void relaysetup(){
 
  pinMode(DOOR_STRIKE_PIN, OUTPUT);
  pinMode(DOOR_HALL_EFFECT_PIN, INPUT_PULLUP);
  doorclose();

  // Attach the interrupt
  attachInterrupt(digitalPinToInterrupt(DOOR_HALL_EFFECT_PIN),  onDoorStateChange, CHANGE);
}


void relayloop(){
  if (doorChangedState) {
    // Set the status to open or closed
    const char *status = digitalRead(DOOR_HALL_EFFECT_PIN) ? "open" : "closed";

    // Send status via mqtt
    messagesend(MQTT_STATE_TOPIC,  status, 1, false);

    // Mark the changed state as false
    doorChangedState = false;
  }

    // Check the door state
  if (triprelay == true && doorState == DOOR_CLOSED) {
    triprelay=false;
    dooropen();
    doorChangedState = true;
    doorOpenTime = millis();
  } else if (doorState == DOOR_OPENED) {
    // Has the strike timeout elapsed?
    if (millis() - doorOpenTime > DOOR_STRIKE_TIMEOUT) {
      doorclose();
    }
  }
}
