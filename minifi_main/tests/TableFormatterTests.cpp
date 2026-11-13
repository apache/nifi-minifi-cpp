// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "unit/Catch.h"
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
