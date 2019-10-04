#include "../sql/Odbc.h"

#pragma comment(lib, "Odbc32.lib")

inline bool IsSuccess(SQLRETURN ret) {
  return ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO;
}


// ODBCDatabase class
void ODBCDatabase::SQLAlloc() {
  SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv_);
  SQLSetEnvAttr(hEnv_, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0); 
  SQLAllocHandle(SQL_HANDLE_DBC, hEnv_, &hDbc_); 
}

void ODBCDatabase::SQLFree() {
  if (hDbc_) {
    SQLFreeHandle(SQL_HANDLE_DBC, hDbc_);
  }
  hDbc_ = 0;
  
  if (hEnv_) {
    SQLFreeHandle(SQL_HANDLE_ENV, hEnv_);
  }
  hEnv_ = 0;
}

void ODBCDatabase::Close() {
  isConnected_ = false;
  
  if (hDbc_) {
    SQLDisconnect(hDbc_);
  }

  SQLFree();
}

void ODBCDatabase::SetConnectionTimeout(LONG seconds) {
  if (hDbc_) {
    SQLSetConnectAttr(hDbc_, SQL_ATTR_CONNECTION_TIMEOUT, (SQLPOINTER)seconds, 0);
  }

	connectionTimeout_ = seconds;
}

bool ODBCDatabase::DriverConnect(const std::string& connStr) {
  SQLSMALLINT connStrOut{};
  auto ret = SQLDriverConnect(hDbc_, 0,  (SQLCHAR*)connStr.c_str(),  SQL_NTS,  0, 0,  &connStrOut,  0);

  return isConnected_ = IsSuccess(ret);
}

LONG ODBCDatabase::GetConnectionTimeout() {
  LONG seconds{};
  SQLGetConnectAttr(hDbc_, SQL_ATTR_CONNECTION_TIMEOUT, &seconds, 0, 0);

  return seconds;
}

bool ODBCDatabase::Execute(CHAR *sqlStr) {
  SQLHSTMT hStmt{};
  SQLAllocHandle(SQL_HANDLE_STMT, hDbc_, &hStmt);
  auto ret = SQLExecDirect(hStmt, (SQLCHAR*)sqlStr, SQL_NTS);
  if (!IsSuccess(ret)) {
    rowCount_ = 0;
    return false;
  }

  SQLINTEGER rowCount{};
  SQLRowCount(hStmt, &rowCount);

  rowCount_ = rowCount;

  return true;
}


// ODBCRecordset Class
ODBCRecordset::ODBCRecordset(ODBCDatabase* pDb) {
  hDbc_ = *pDb;

  AllocStmt();
}

ODBCRecordset::~ODBCRecordset() {
  Close();
}


void ODBCRecordset::AllocStmt() {
  SQLAllocHandle(SQL_HANDLE_STMT, hDbc_, &hStmt_);
}

bool ODBCRecordset::Open(const std::string& sqlStr) {
  auto ret = SQLExecDirect(hStmt_, (SQLCHAR*)sqlStr.c_str(), SQL_NTS);
  if (!IsSuccess(ret))	{
    return false;
  }

  ret = SQLFetch(hStmt_);

  return IsSuccess(ret);
}

bool ODBCRecordset::GetColValue(int col, std::string& data) {
  SQLLEN displaySize{};
  if (!GetColDisplaySize(col, displaySize)) {
    return false;
  }

  data.resize(displaySize + 1);

  SQLINTEGER value{};
  auto ret = SQLGetData(hStmt_, (SQLUSMALLINT)col + 1, SQL_C_CHAR, &data[0], displaySize, &value);
  if (!IsSuccess(ret))
    return false;

  if (SQL_NULL_DATA == value) {
    data = "<NULL>";
  } else {
    data.resize(value);
  }

  return true;
}

bool ODBCRecordset::MoveNext() {
  auto ret = SQLFetchScroll(hStmt_, SQL_FETCH_NEXT, 0);

  isEOF_ = ret == SQL_NO_DATA;

  return IsSuccess(ret);
}

SQLUINTEGER ODBCRecordset::GetColLength(int col) {
  SQLUINTEGER colSize{};

  SQLDescribeCol(hStmt_, col + 1, 0, 0, 0, 0, &colSize, 0, 0);

  return colSize;
}

bool ODBCRecordset::GetColInfo(int col, ColInfo& colInfo) {
  auto ret = SQLDescribeCol(
    hStmt_, 
    col + 1, 
    colInfo.colName, 
    sizeof(colInfo.colName), 
    &colInfo.colNameLength, 
    &colInfo.colType,
    &colInfo.colSize,
    &colInfo.decimalDigits,
    &colInfo.nullable
  );

  return IsSuccess(ret);
}

bool ODBCRecordset::GetColDisplaySize(int col, SQLLEN& displaySize) {
  auto ret = SQLColAttribute(hStmt_, col + 1, SQL_DESC_DISPLAY_SIZE, 0, 0, 0, &displaySize);

  return IsSuccess(ret);
}

int ODBCRecordset::GetNumCols() {
  SQLSMALLINT numCol{};
  SQLNumResultCols(hStmt_, &numCol);

  return numCol;
}

void ODBCRecordset::Close() {
  if (hStmt_) {
    SQLFreeHandle(SQL_HANDLE_STMT, hStmt_);
  }

  hStmt_ = 0;
}
