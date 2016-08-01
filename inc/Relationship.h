/**
 * @file Relationship.h
 * Relationship class declaration
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
#ifndef __RELATIONSHIP_H__
#define __RELATIONSHIP_H__

#include <string>
#include <uuid/uuid.h>
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>

//! undefined relationship for remote process group outgoing port and root process group incoming port
#define UNDEFINED_RELATIONSHIP "undefined"

inline bool isRelationshipNameUndefined(std::string name)
{
	if (name == UNDEFINED_RELATIONSHIP)
		return true;
	else
		return false;
}

//! Relationship Class
class Relationship {

public:
	//! Constructor
	/*!
	 * Create a new relationship 
	 */
	Relationship(const std::string name, const std::string description)
		: _name(name), _description(description) {
	}
	Relationship()
		: _name(UNDEFINED_RELATIONSHIP) {
	}
	//! Destructor
	virtual ~Relationship() {
	}
	//! Get Name for the relationship
	std::string getName() {
		return _name;
	}
	//! Get Description for the relationship
	std::string getDescription() {
		return _description;
	}
	//! Compare
	bool operator < (const Relationship & right) const {
		return _name < right._name;
	}
	//! Whether it is a undefined relationship
	bool isRelationshipUndefined()
	{
		return isRelationshipNameUndefined(_name);
	}

protected:

	//! Name
	std::string _name;
	//! Description
	std::string _description;

private:
};

#endif
