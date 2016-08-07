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
#include "Display.h"

Display::Display(TempSensor *tempSensor, Stats *stats) :
		lcd(DIG_PIN_LCD_RS, DIG_PIN_LCD_ENABLE, DIG_PIN_LCD_D0, DIG_PIN_LCD_D1, DIG_PIN_LCD_D2, DIG_PIN_LCD_D3), tempSensor(
				tempSensor), stats(stats), mainState(this), runtimeState(this), relayTimeState(this), dayStatsState(
				this), driver(4, &mainState, &runtimeState, &relayTimeState, &dayStatsState) {
	lcd.begin(16, 2);
	lcd.noAutoscroll();

	lcd.setCursor(0, 0);
	driver.changeState(STATE_MAIN);
}

void Display::onEvent(BusEvent event, va_list ap) {
	if (event == SERVICE_RESUME) {
		driver.changeState(STATE_MAIN);
	} else {
		driver.execute(event);
	}
}

void Display::cycle() {
	driver.execute(NO_EVENT);
}

inline void Display::printlnNa(uint8_t row, const char *fmt) {
	// TODO implement it without sprintf
	println(row, fmt);
}

inline void Display::println(uint8_t row, const char *fmt, ...) {
	lcd.setCursor(0, row);

	va_list va;
	va_start(va, fmt);
	short chars = vsprintf(lcdBuf, fmt, va);
	va_end(va);

	cleanRight(lcdBuf, chars, LINE_LENGTH);
	lcd.print(lcdBuf);
}

inline void Display::cleanRight(char *array, short from, short size) {
	for (short int i = from; i < size; i++) {
		array[i] = ' ';
	}
	array[size] = '\0';
}

inline void Display::clcd(uint8_t row) {
	lcd.setCursor(0, row);
	lcd.print("                ");
	lcd.setCursor(0, row);
}

void Display::printTime(uint8_t row, Time* time) {
	println(row, "%04d -> %02d:%02d:%02d", time->dd, time->hh, time->mm, time->ss);
}

// ##################### MainState #####################
Display::MainState::MainState(Display* display) :
		display(display), lastUpdateMs(0) {
}

Display::MainState::~MainState() {
}

inline void Display::MainState::update() {
	int8_t tempNow = display->tempSensor->getQuickTemp();
	Temp* actual = display->stats->getActual();
	display->println(1, "%3d|%3d|%3d|%3d", tempNow, actual->min, actual->max, actual->avg);
}

uint8_t Display::MainState::execute(BusEvent event) {
	if (event != NO_EVENT) {
		if (event == BUTTON_NEXT) {
			return STATE_RUNTIME;

		} else if (event == BUTTON_PREV) {
			return STATE_RUNTIME;
		}
	}
	uint32_t millis = util_millis();
	if (millis - lastUpdateMs >= UPDATE_FREQ) {
		update();
		lastUpdateMs = millis;
	}

	return STATE_NOCHANGE;
}

void Display::MainState::init() {
#if LOG
	log(F("Display - main state"));
#endif
	lastUpdateMs = 0;
	display->printlnNa(0, "NOW|MIN|MAX|AVG");
	update();
}

// ##################### RuntimeState #####################
Display::RuntimeState::RuntimeState(Display* display) :
		display(display) {
}

Display::RuntimeState::~RuntimeState() {
}

uint8_t Display::RuntimeState::execute(BusEvent event) {
	if (event == NO_EVENT) {
		return STATE_NOCHANGE;
	}
	if (event == BUTTON_NEXT) {
		return STATE_MAIN;

	} else if (event == BUTTON_PREV) {
		return STATE_MAIN;

	}
	return STATE_NOCHANGE;
}

void Display::RuntimeState::init() {
#if LOG
	log(F("Display - on time"));
#endif
	display->printlnNa(0, "System on time");
	display->printTime(1, display->stats->getUpTime());
}

// ##################### RelayTimeState #####################
Display::RelayTimeState::RelayTimeState(Display* display) :
		display(display), relayIdx(0) {
}

Display::RelayTimeState::~RelayTimeState() {
}

uint8_t Display::RelayTimeState::execute(BusEvent event) {
	if (event == NO_EVENT) {
		return STATE_NOCHANGE;
	}
	if (event == BUTTON_NEXT) {
		relayIdx++;
		if (relayIdx == RELAYS_AMOUNT) {
			return STATE_DAY_STATS;
		}
		updateDisplay();
	} else if (event == BUTTON_PREV) {
		// cannot decrease before checking because it's unsigned int
		if (relayIdx == 0) {
			return STATE_RUNTIME;
		}
		relayIdx--;
		updateDisplay();
	}
	return STATE_NOCHANGE;
}

inline void Display::RelayTimeState::updateDisplay() {
	Time* rt = display->stats->getRelayTime(relayIdx);
	display->println(0, "Relay %d on time", relayIdx + 1);
	display->printTime(1, rt);
}

void Display::RelayTimeState::init() {
#if LOG
	log(F("Display - relay time"));
#endif
	relayIdx = 0;
}

// ##################### DayStatsState #####################
Display::DayStatsState::DayStatsState(Display* display) :
		display(display), daySize(0), showedInfo(false) {
}

Display::DayStatsState::~DayStatsState() {
}

uint8_t Display::DayStatsState::execute(BusEvent event) {
	if (event == NO_EVENT) {
		return STATE_NOCHANGE;
	}
	if (event == BUTTON_NEXT) {
		if (!showedInfo) {
			showedInfo = true;
			showInfo();
		} else {
			if (!display->stats->dit_hasNext()) {
				return STATE_MAIN;
			}
			updateDisplay(display->stats->dit_next());
		}
	} else if (event == BUTTON_PREV) {
		// cannot decrease before checking because it's unsigned int
		if (!display->stats->dit_hasPrev()) {
			return STATE_RELAY_TIME;
		}
		updateDisplay(display->stats->dit_prev());
	}
	return STATE_NOCHANGE;
}

inline void Display::DayStatsState::updateDisplay(Temp* temp) {
	display->println(0, "%2d-> %2d|%2d|%2d", temp->day, temp->min, temp->max, temp->avg);

	// TODO support flexible amount of relays
	if (RELAYS_AMOUNT == 2) {
		display->println(1, "%2d:%2d, %2d:%2d", temp->realyOnHH[0], temp->realyOnMM[0], temp->realyOnHH[1],
				temp->realyOnMM[1]);
	}
}

void Display::DayStatsState::showInfo() {
	display->printlnNa(0, "DD-> MIN|MAX|AVG");
	display->printlnNa(1, "ON times[HH:MM,]");
}

void Display::DayStatsState::init() {
#if LOG
	log(F("Display - day stats"));
#endif
	showedInfo = false;
	daySize = display->stats->dit_size();
	display->stats->dit_reset();
	display->println(0, "Statistics for %d days", daySize);

}
