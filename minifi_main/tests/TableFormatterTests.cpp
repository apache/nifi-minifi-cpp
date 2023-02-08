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

#include "Catch.h"
#include "TableFormatter.h"

namespace minifi = org::apache::nifi::minifi;

TEST_CASE("An empty table can be built") {
  minifi::docs::Table table{{"Name", "Address", "Occupation"}};
  CHECK(table.toString() == "| Name | Address | Occupation |\n"
                            "|------|---------|------------|\n");
}

TEST_CASE("We can add rows to the table") {
  minifi::docs::Table table{{"Name", "Address", "Occupation"}};
  table.addRow({"Sherlock Holmes", "221b Baker Street", "Detective"});
  table.addRow({"Petunia Dursley", "4 Privet Drive", "Housewife"});
  table.addRow({"Wallace", "62 West Wallaby Street", "Inventor"});
  CHECK(table.toString() == "| Name            | Address                | Occupation |\n"
                            "|-----------------|------------------------|------------|\n"
                            "| Sherlock Holmes | 221b Baker Street      | Detective  |\n"
                            "| Petunia Dursley | 4 Privet Drive         | Housewife  |\n"
                            "| Wallace         | 62 West Wallaby Street | Inventor   |\n");
}

TEST_CASE("Columns get resized when we add rows with longer elements") {
  minifi::docs::Table table{{"Name", "Address", "Occupation"}};
  table.addRow({"S. H.", "Baker St", "Detective"});
  CHECK(table.toString() == "| Name  | Address  | Occupation |\n"
                            "|-------|----------|------------|\n"
                            "| S. H. | Baker St | Detective  |\n");

  table.addRow({"Sherlock Holmes", "221b Baker Street", "Private Investigator"});
  CHECK(table.toString() == "| Name            | Address           | Occupation           |\n"
                            "|-----------------|-------------------|----------------------|\n"
                            "| S. H.           | Baker St          | Detective            |\n"
                            "| Sherlock Holmes | 221b Baker Street | Private Investigator |\n");
}
