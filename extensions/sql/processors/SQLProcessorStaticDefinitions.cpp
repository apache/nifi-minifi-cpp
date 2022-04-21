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

#include "core/PropertyBuilder.h"
#include "core/Resource.h"
#include "ExecuteSQL.h"
#include "QueryDatabaseTable.h"
#include "PutSQL.h"
#include "SQLProcessor.h"

// FIXME(fgerlits): we need to put all these static definitions in a single file so that they are executed in this order at runtime
// when https://issues.apache.org/jira/browse/MINIFICPP-1825 is closed, these definitions should be moved back to the cpp file of the class to which they belong

namespace org::apache::nifi::minifi::processors {

// SQLProcessor

const core::Property SQLProcessor::DBControllerService(
    core::PropertyBuilder::createProperty("DB Controller Service")
    ->isRequired(true)
    ->withDescription("Database Controller Service.")
    ->supportsExpressionLanguage(true)->build());


// ExecuteSQL

const core::Property ExecuteSQL::SQLSelectQuery(
  core::PropertyBuilder::createProperty("SQL select query")
  ->withDescription(
    "The SQL select query to execute. The query can be empty, a constant value, or built from attributes using Expression Language. "
    "If this property is specified, it will be used regardless of the content of incoming flowfiles. "
    "If this property is empty, the content of the incoming flow file is expected to contain a valid SQL select query, to be issued by the processor to the database. "
    "Note that Expression Language is not evaluated for flow file contents.")
  ->supportsExpressionLanguage(true)->build());

const core::Relationship ExecuteSQL::Success("success", "Successfully created FlowFile from SQL query result set.");

REGISTER_RESOURCE(ExecuteSQL, Processor);


// QueryDataBaseTable

const core::Property QueryDatabaseTable::TableName(
  core::PropertyBuilder::createProperty("Table Name")
  ->isRequired(true)
  ->withDescription("The name of the database table to be queried.")
  ->supportsExpressionLanguage(true)->build());

const core::Property QueryDatabaseTable::ColumnNames(
  core::PropertyBuilder::createProperty("Columns to Return")
  ->isRequired(false)
  ->withDescription(
    "A comma-separated list of column names to be used in the query. If your database requires special treatment of the names (quoting, e.g.), each name should include such treatment. "
    "If no column names are supplied, all columns in the specified table will be returned. "
    "NOTE: It is important to use consistent column names for a given table for incremental fetch to work properly.")
  ->supportsExpressionLanguage(true)->build());

const core::Property QueryDatabaseTable::MaxValueColumnNames(
  core::PropertyBuilder::createProperty("Maximum-value Columns")
  ->isRequired(false)
  ->withDescription(
    "A comma-separated list of column names. The processor will keep track of the maximum value for each column that has been returned since the processor started running. "
    "Using multiple columns implies an order to the column list, and each column's values are expected to increase more slowly than the previous columns' values. "
    "Thus, using multiple columns implies a hierarchical structure of columns, which is usually used for partitioning tables. "
    "This processor can be used to retrieve only those rows that have been added/updated since the last retrieval. "
    "Note that some ODBC types such as bit/boolean are not conducive to maintaining maximum value, so columns of these types should not be listed in this property, and will result in error(s) during processing. "
    "If no columns are provided, all rows from the table will be considered, which could have a performance impact. "
    "NOTE: It is important to use consistent max-value column names for a given table for incremental fetch to work properly. "
    "NOTE: Because of a limitation of database access library 'soci', which doesn't support milliseconds in it's 'dt_date', "
    "there is a possibility that flowfiles might have duplicated records, if a max-value column with 'dt_date' type has value with milliseconds.")
  ->supportsExpressionLanguage(true)->build());

const core::Property QueryDatabaseTable::WhereClause(
  core::PropertyBuilder::createProperty("Where Clause")
  ->isRequired(false)
  ->withDescription("A custom clause to be added in the WHERE condition when building SQL queries.")
  ->supportsExpressionLanguage(true)->build());

const core::Relationship QueryDatabaseTable::Success("success", "Successfully created FlowFile from SQL query result set.");

REGISTER_RESOURCE(QueryDatabaseTable, Processor);


// PutSQL

const core::Property PutSQL::SQLStatement(
  core::PropertyBuilder::createProperty("SQL Statement")
  ->isRequired(false)
  ->withDescription(
      "The SQL statement to execute. The statement can be empty, a constant value, or built from attributes using Expression Language. "
      "If this property is specified, it will be used regardless of the content of incoming flowfiles. If this property is empty, the content of "
      "the incoming flow file is expected to contain a valid SQL statement, to be issued by the processor to the database.")
  ->supportsExpressionLanguage(true)->build());

const core::Relationship PutSQL::Success("success", "Database is successfully updated.");

REGISTER_RESOURCE(PutSQL, Processor);

}  // namespace org::apache::nifi::minifi::processors
