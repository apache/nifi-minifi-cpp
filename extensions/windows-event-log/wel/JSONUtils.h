/**
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

#pragma once


#include <string>

#include "pugixml.hpp"
#include "rapidjson/document.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace wel {

/**
 * * !!WARNING!! The json document must not outlive the xml argument. For better performance,
 * the created json document stores references to values in the xml node. Accessing the
 * json document after the xml node has been changed or destroyed results in undefined behavior.
 *
 * Converts each xml element node to a json object of
 * the form {name: String, attributes: Object, children: Array, text: String}
 * Aims to preserve most of the input xml structure.
 */
rapidjson::Document toRawJSON(const pugi::xml_node& root);

/**
 * * !!WARNING!! The json document must not outlive the xml argument. For better performance,
 * the created json document stores references to values in the xml node. Accessing the
 * json document after the xml node has been changed or destroyed results in undefined behavior.
 *
 * Retains some hierarchical structure of the original xml event,
 * e.g. transforms
 *   <Event><System><Provider Name="Banana" Guid="{5}"/></System></Event>
 * into
 *   {System: {Provider: {Name: "Banana", Guid: "{5}"}}}
 */
rapidjson::Document toSimpleJSON(const pugi::xml_node& root);

/**
 * * !!WARNING!! The json document must not outlive the xml argument. For better performance,
 * the created json document stores references to values in the xml node. Accessing the
 * json document after the xml node has been changed or destroyed results in undefined behavior.
 *
 * Flattens most of the structure, i.e. removes intermediate
 * objects and lifts innermost string-valued keys to the root.
 * e.g. {System: {Provider: {Name: String}}} => {Name: String}
 * 
 * Moreover it also flattens each named data element where the
 * name does not conflict with already existing members 
 * (e.g. a data with name "Guid" won't be flattened as it would
 * overwrite the existing "Guid" field).
 * 
 * e.g. {EventData: [{Name: "Test", Content: "X"}]} => {Test: "X"}
 * 
 * In order to mitigate data loss, it preserves the EventData
 * array in its entirety as well.
 * (otherwise a "Guid" data would be lost)
 */
rapidjson::Document toFlattenedJSON(const pugi::xml_node& root);

std::string jsonToString(const rapidjson::Document& doc);

}  // namespace wel
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
