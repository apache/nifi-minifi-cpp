# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

@ENABLE_SQL
Feature: Executing SQL operations from MiNiFi-C++
  As a user of MiNiFi
  I need to have ExecuteSQL, QueryDatabaseTable and PutSQL processors

  Scenario: A MiNiFi instance can insert data to test table with PutSQL processor
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a UpdateAttribute processor with the "sql.args.1.value" property set to "42"
    And the "sql.args.2.value" property of the UpdateAttribute processor is set to "pineapple"
    And a PutSQL processor with the "SQL Statement" property set to "INSERT INTO test_table (int_col, text_col) VALUES (?, ?)"
    And the "success" relationship of the GenerateFlowFile processor is connected to the UpdateAttribute
    And the "success" relationship of the UpdateAttribute processor is connected to the PutSQL
    And an ODBCService is setup up for PutSQL with the name "ODBCService"
    And PutSQL's success relationship is auto-terminated
    And PutSQL's failure relationship is auto-terminated
    And a PostgreSQL server is set up
    When all instances start up
    Then the query "SELECT * FROM test_table WHERE int_col = 42" returns 1 rows in less than 60 seconds on the PostgreSQL server

  Scenario: A MiNiFi instance can query to test table with ExecuteSQL processor
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a UpdateAttribute processor with the "sql.args.1.value" property set to "apple"
    And the "sql.args.2.value" property of the UpdateAttribute processor is set to "banana"
    And a ExecuteSQL processor with the "SQL select query" property set to "SELECT * FROM test_table WHERE text_col = ? OR text_col = ? ORDER BY int_col DESC"
    And the "Output Format" property of the ExecuteSQL processor is set to "JSON"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GenerateFlowFile processor is connected to the UpdateAttribute
    And the "success" relationship of the UpdateAttribute processor is connected to the ExecuteSQL
    And the "success" relationship of the ExecuteSQL processor is connected to the PutFile
    And an ODBCService is setup up for ExecuteSQL with the name "ODBCService"
    And a PostgreSQL server is set up
    When all instances start up
    Then at least one file with the content "[{"int_col":2,"text_col":"banana"},{"int_col":1,"text_col":"apple"}]" is placed in the "/tmp/output" directory in less than 10 seconds

  Scenario: A MiNiFi instance can query to test table containing mixed case column names with ExecuteSQL processor
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a UpdateAttribute processor with the "sql.args.1.value" property set to "ApPlE"
    And the "sql.args.2.value" property of the UpdateAttribute processor is set to "BaNaNa"
    # in PostgreSQL we have to quote column names if they contain uppercase characters
    And a ExecuteSQL processor with the "SQL select query" property set to "SELECT * FROM test_table2 WHERE "tExT_Col" = ? OR "tExT_Col" = ? ORDER BY int_col DESC"
    And the "Output Format" property of the ExecuteSQL processor is set to "JSON"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GenerateFlowFile processor is connected to the UpdateAttribute
    And the "success" relationship of the UpdateAttribute processor is connected to the ExecuteSQL
    And the "success" relationship of the ExecuteSQL processor is connected to the PutFile
    And an ODBCService is setup up for ExecuteSQL with the name "ODBCService"
    And a PostgreSQL server is set up
    When all instances start up
    Then at least one file with the content "[{"int_col":6,"tExT_Col":"BaNaNa"},{"int_col":5,"tExT_Col":"ApPlE"}]" is placed in the "/tmp/output" directory in less than 10 seconds

  Scenario: A MiNiFi instance can query to test table with QueryDatabaseTable processor
    Given a QueryDatabaseTable processor with the "Table Name" property set to "test_table"
    And the "Columns to Return" property of the QueryDatabaseTable processor is set to "text_col"
    And the "Where Clause" property of the QueryDatabaseTable processor is set to "int_col = 1"
    And the "Output Format" property of the QueryDatabaseTable processor is set to "JSON"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the QueryDatabaseTable processor is connected to the PutFile
    And an ODBCService is setup up for QueryDatabaseTable with the name "ODBCService"
    And a PostgreSQL server is set up
    When all instances start up
    Then at least one file with the content "[{"text_col":"apple"}]" is placed in the "/tmp/output" directory in less than 10 seconds


  Scenario: A MiNiFi instance can query to test table containing mixed case column names with QueryDatabaseTable processor
    Given a QueryDatabaseTable processor with the "Table Name" property set to "test_table2"
    And the "Columns to Return" property of the QueryDatabaseTable processor is set to ""tExT_Col""
    And the "Where Clause" property of the QueryDatabaseTable processor is set to "int_col = 5"
    And the "Output Format" property of the QueryDatabaseTable processor is set to "JSON"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the QueryDatabaseTable processor is connected to the PutFile
    And an ODBCService is setup up for QueryDatabaseTable with the name "ODBCService"
    And a PostgreSQL server is set up
    When all instances start up
    Then at least one file with the content "[{"tExT_Col":"ApPlE"}]" is placed in the "/tmp/output" directory in less than 10 seconds
