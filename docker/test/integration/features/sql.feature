Feature: Execuring SQL operations from MiNiFi-C++
  As a user of MiNiFi
  I need to have ExecuteSQL, QueryDatabaseTable and PutSQL processors

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: A MiNiFi instance can insert data to test table with PutSQL processor
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a UpdateAttribute processor with the "sql.args.1.value" property set to "42"
    And the "sql.args.2.value" of the UpdateAttribute processor is set to "pineapple"
    And a PutSQL processor with the "SQL Statement" property set to "INSERT INTO test_table (int_col, text_col) VALUES (?, ?)"
    And the "success" relationship of the GenerateFlowFile processor is connected to the UpdateAttribute
    And the "success" relationship of the UpdateAttribute processor is connected to the PutSQL
    And an ODBCService is setup up for PutSQL with the name "ODBCService" and connection string "Driver={PostgreSQL ANSI};Server=postgresql-server;Port=5432;Database=postgres;Uid=postgres;Pwd=password;"
    And a PostgreSQL server "postgresql" is set up
    When all instances start up
    Then the query "SELECT * FROM test_table WHERE int_col = 42" returns 1 rows in less than 120 seconds on the "postgresql" PostgreSQL server

  Scenario: A MiNiFi instance can query to test table with ExecuteSQL processor
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a UpdateAttribute processor with the "sql.args.1.value" property set to "apple"
    And the "sql.args.2.value" of the UpdateAttribute processor is set to "banana"
    And a ExecuteSQL processor with the "SQL select query" property set to "SELECT * FROM test_table WHERE text_col = ? OR text_col = ? ORDER BY int_col DESC"
    And the "Output Format" of the ExecuteSQL processor is set to "JSON"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GenerateFlowFile processor is connected to the UpdateAttribute
    And the "success" relationship of the UpdateAttribute processor is connected to the ExecuteSQL
    And the "success" relationship of the ExecuteSQL processor is connected to the PutFile
    And an ODBCService is setup up for ExecuteSQL with the name "ODBCService" and connection string "Driver={PostgreSQL ANSI};Server=postgresql-server;Port=5432;Database=postgres;Uid=postgres;Pwd=password;"
    And a PostgreSQL server "postgresql" is set up
    When all instances start up
    Then a flowfile with the content '[{"int_col":2,"text_col":"banana"},{"int_col":1,"text_col":"apple"}]' is placed in the monitored directory in less than 120 seconds

  Scenario: A MiNiFi instance can query to test table with QueryDatabaseTable processor
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a QueryDatabaseTable processor with the "Table Name" property set to "test_table"
    And the "Columns to Return" of the QueryDatabaseTable processor is set to "text_col"
    And the "Where Clause" of the QueryDatabaseTable processor is set to "int_col = 1"
    And the "Output Format" of the QueryDatabaseTable processor is set to "JSON"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GenerateFlowFile processor is connected to the QueryDatabaseTable
    And the "success" relationship of the QueryDatabaseTable processor is connected to the PutFile
    And an ODBCService is setup up for QueryDatabaseTable with the name "ODBCService" and connection string "Driver={PostgreSQL ANSI};Server=postgresql-server;Port=5432;Database=postgres;Uid=postgres;Pwd=password;"
    And a PostgreSQL server "postgresql" is set up
    When all instances start up
    Then a flowfile with the content '[{"text_col":"apple"}]' is placed in the monitored directory in less than 120 seconds
