/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "Thermostat.h"

static TempSensor* tempSensor;
static Stats* stats;
static RelayDriver* relayDriver;
static Display* display;
static ServiceSuspender* serviceSuspender;
static Buttons* buttons;
static SystemStatus* systemStatus;

static void init(Initializable* ini){
	ini->init();
}

void setup() {
	log_setup();
	util_setup();

	tempSensor = new TempSensor();
	stats = new Stats(tempSensor);
	relayDriver = new RelayDriver(tempSensor);
	serviceSuspender = new ServiceSuspender();
	systemStatus = new SystemStatus();
	//display = new Display(tempSensor, stats, relayDriver);
	//buttons = new Buttons();

	// init phase
	init(tempSensor);
	init(stats);
	init(relayDriver);
	//init(display);
	//init(buttons);
}

void loop() {
	util_cycle();
	log_cycle();

	// Hart beat
	eb_fire(BusEvent::CYCLE);
}
