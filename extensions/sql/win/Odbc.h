#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <time.h>
#include <string>

struct ColInfo {
  SQLCHAR			  colName[128]{};
  SQLSMALLINT		colNameLength{};
  SQLSMALLINT		colType{};
  SQLUINTEGER		colSize{};
  SQLSMALLINT		decimalDigits{};
  SQLSMALLINT		nullable{};
  SQLPOINTER		targetValue{};
};

class ODBCDatabase {
public:
  ODBCDatabase() {
    SQLAlloc();
  };

  ~ODBCDatabase() {
    Close();
  };

  operator SQLHDBC() { return hDbc_; }

  bool DriverConnect(const std::string& connStr);

  void SetConnectionTimeout(LONG seconds);

  LONG GetConnectionTimeout();

  void SetLoginTimeout(LONG seconds) { loginTimeout_ = seconds; };

  bool Execute(CHAR* sqlStr);

  int RowCount() { return rowCount_; };

  bool IsConnected() { return isConnected_; };

  void Close();

protected:
  void SQLFree();
  void SQLAlloc();

protected:
  SQLHDBC hDbc_{};
  LONG loginTimeout_{5};
  LONG connectionTimeout_{};
  bool isConnected_{FALSE};
  SQLHENV hEnv_{};
  int rowCount_{};
};

class ODBCRecordset
{
public:
  ODBCRecordset(ODBCDatabase* pDb);

  ~ODBCRecordset();

  bool Open(const std::string& sqlStr);

  SQLUINTEGER GetColLength(int col);

  bool GetColInfo(int col, ColInfo& colInfo);

  int GetNumCols();

  bool GetColValue(int col, std::string& data);

  bool GetColDisplaySize(int col, SQLLEN& displaySize);

  bool MoveNext();

  bool IsEof() { return isEOF_; };

  void Close();

private:
  void AllocStmt();

protected:
  bool isEOF_{false};
  SQLHDBC hDbc_{};
  SQLHSTMT hStmt_{};
};
