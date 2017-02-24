/**
 * @file Property.h
 * Processor Property class declaration
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __PROPERTY_H__
#define __PROPERTY_H__

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <functional>
#include <set>
#include <stdlib.h>
#include <math.h>

//! Time Unit
enum TimeUnit {
	DAY, HOUR, MINUTE, SECOND, MILLISECOND, NANOSECOND
};

//! Property Class
class Property {

public:
	//! Constructor
	/*!
	 * Create a new property
	 */
	Property(const std::string name, const std::string description,
			const std::string value) :
			_name(name), _description(description), _value(value) {
	}
	Property() {
	}
	//! Destructor
	virtual ~Property() {
	}
	//! Get Name for the property
	std::string getName() {
		return _name;
	}
	//! Get Description for the property
	std::string getDescription() {
		return _description;
	}
	//! Get value for the property
	std::string getValue() {
		return _value;
	}
	//! Set value for the property
	void setValue(std::string value) {
		_value = value;
	}
	//! Compare
	bool operator <(const Property & right) const {
		return _name < right._name;
	}

	//! Convert TimeUnit to MilliSecond
	static bool ConvertTimeUnitToMS(int64_t input, TimeUnit unit,
			int64_t &out) {
		if (unit == MILLISECOND) {
			out = input;
			return true;
		} else if (unit == SECOND) {
			out = input * 1000;
			return true;
		} else if (unit == MINUTE) {
			out = input * 60 * 1000;
			return true;
		} else if (unit == HOUR) {
			out = input * 60 * 60 * 1000;
			return true;
		} else if (unit == DAY) {
			out = 24 * 60 * 60 * 1000;
			return true;
		} else if (unit == NANOSECOND) {
			out = input / 1000 / 1000;
			return true;
		} else {
			return false;
		}
	}
	//! Convert TimeUnit to NanoSecond
	static bool ConvertTimeUnitToNS(int64_t input, TimeUnit unit,
			int64_t &out) {
		if (unit == MILLISECOND) {
			out = input * 1000 * 1000;
			return true;
		} else if (unit == SECOND) {
			out = input * 1000 * 1000 * 1000;
			return true;
		} else if (unit == MINUTE) {
			out = input * 60 * 1000 * 1000 * 1000;
			return true;
		} else if (unit == HOUR) {
			out = input * 60 * 60 * 1000 * 1000 * 1000;
			return true;
		} else if (unit == NANOSECOND) {
			out = input;
			return true;
		} else {
			return false;
		}
	}
	//! Convert String
	static bool StringToTime(std::string input, int64_t &output,
			TimeUnit &timeunit) {
		if (input.size() == 0) {
			return false;
		}

		const char *cvalue = input.c_str();
		char *pEnd;
		long int ival = strtol(cvalue, &pEnd, 0);

		if (pEnd[0] == '\0') {
			return false;
		}

		while (*pEnd == ' ') {
			// Skip the space
			pEnd++;
		}

		std::string unit(pEnd);

		if (unit == "sec" || unit == "s" || unit == "second"
				|| unit == "seconds" || unit == "secs") {
			timeunit = SECOND;
			output = ival;
			return true;
		} else if (unit == "min" || unit == "m" || unit == "mins"
				|| unit == "minute" || unit == "minutes") {
			timeunit = MINUTE;
			output = ival;
			return true;
		} else if (unit == "ns" || unit == "nano" || unit == "nanos"
				|| unit == "nanoseconds") {
			timeunit = NANOSECOND;
			output = ival;
			return true;
		} else if (unit == "ms" || unit == "milli" || unit == "millis"
				|| unit == "milliseconds") {
			timeunit = MILLISECOND;
			output = ival;
			return true;
		} else if (unit == "h" || unit == "hr" || unit == "hour"
				|| unit == "hrs" || unit == "hours") {
			timeunit = HOUR;
			output = ival;
			return true;
		} else if (unit == "d" || unit == "day" || unit == "days") {
			timeunit = DAY;
			output = ival;
			return true;
		} else
			return false;
	}

	//! Convert String to Integer
	static bool StringToInt(std::string input, int64_t &output) {
		if (input.size() == 0) {
			return false;
		}

		const char *cvalue = input.c_str();
		char *pEnd;
		long int ival = strtol(cvalue, &pEnd, 0);

		if (pEnd[0] == '\0') {
			output = ival;
			return true;
		}

		while (*pEnd == ' ') {
			// Skip the space
			pEnd++;
		}

		char end0 = toupper(pEnd[0]);
		if ((end0 == 'K') || (end0 == 'M') || (end0 == 'G') || (end0 == 'T')
				|| (end0 == 'P')) {
			if (pEnd[1] == '\0') {
				unsigned long int multiplier = 1000;

				if ((end0 != 'K')) {
					multiplier *= 1000;
					if (end0 != 'M') {
						multiplier *= 1000;
						if (end0 != 'G') {
							multiplier *= 1000;
							if (end0 != 'T') {
								multiplier *= 1000;
							}
						}
					}
				}
				output = ival * multiplier;
				return true;

			} else if ((pEnd[1] == 'b' || pEnd[1] == 'B')
					&& (pEnd[2] == '\0')) {

				unsigned long int multiplier = 1024;

				if ((end0 != 'K')) {
					multiplier *= 1024;
					if (end0 != 'M') {
						multiplier *= 1024;
						if (end0 != 'G') {
							multiplier *= 1024;
							if (end0 != 'T') {
								multiplier *= 1024;
							}
						}
					}
				}
				output = ival * multiplier;
				return true;
			}
		}

		return false;
	}

protected:
	//! Name
	std::string _name;
	//! Description
	std::string _description;
	//! Value
	std::string _value;

private:

};

#endif
