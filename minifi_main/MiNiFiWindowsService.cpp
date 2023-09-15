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

#ifdef WIN32

#include "MiNiFiWindowsService.h"

#include <Windows.h>
#include <Strsafe.h>
#include <tuple>
#include <tlhelp32.h>

#include "MainHelper.h"

namespace minifi = org::apache::nifi::minifi;

// Implemented in MiNiFiMain.cpp
void SignalExitProcess();

static const char* const SERVICE_TERMINATION_EVENT_NAME = "Global\\MiNiFiServiceTermination";

static void OutputDebug(const char* format, ...) {
  va_list args;
  va_start(args, format);

  char buf[256];
  sprintf_s(buf, _countof(buf), "%s: %s", SERVICE_NAME, format);

  char out[1024];
  StringCbVPrintfA(out, sizeof(out), buf, args);

  OutputDebugStringA(out);

  va_end(args);
};

void RunAsServiceIfNeeded() {
  static const int WAIT_TIME_EXE_TERMINATION = 5000;
  static const int WAIT_TIME_EXE_RESTART = 60000;

  static SERVICE_STATUS s_serviceStatus;
  static SERVICE_STATUS_HANDLE s_statusHandle;
  static HANDLE s_hProcess;
  static HANDLE s_hEvent;

  static char serviceNameMutable[] = SERVICE_NAME;

  static auto Log = []() {
    static std::shared_ptr<minifi::core::logging::Logger> s_logger = minifi::core::logging::LoggerConfiguration::getConfiguration().getLogger("service");
    return s_logger;
  };

  SERVICE_TABLE_ENTRY serviceTable[] = {
    {
      serviceNameMutable,
      [](DWORD argc, LPTSTR *argv) {
        setSyslogLogger();

        Log()->log_trace("ServiceCtrlDispatcher");

        s_hEvent = CreateEvent(0, TRUE, FALSE, SERVICE_TERMINATION_EVENT_NAME);
        if (!s_hEvent) {
          Log()->log_error("!CreateEvent lastError {:#x}", GetLastError());
          return;
        }

        s_statusHandle = RegisterServiceCtrlHandler(
          SERVICE_NAME,
          [](DWORD ctrlCode) {
            Log()->log_trace("ServiceCtrlHandler ctrlCode {}", ctrlCode);

            if (SERVICE_CONTROL_STOP == ctrlCode) {
              Log()->log_trace("ServiceCtrlHandler ctrlCode = SERVICE_CONTROL_STOP");

              // Set service status SERVICE_STOP_PENDING.
              s_serviceStatus.dwControlsAccepted = 0;
              s_serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
              s_serviceStatus.dwWin32ExitCode = 0;

              if (!SetServiceStatus(s_statusHandle, &s_serviceStatus)) {
                Log()->log_error("!SetServiceStatus SERVICE_STOP_PENDING lastError {:#x}", GetLastError());
              }

              bool exeTerminated = false;

              SetEvent(s_hEvent);

              Log()->log_info("Wait for exe termination");
              switch (auto res = WaitForSingleObject(s_hProcess, WAIT_TIME_EXE_TERMINATION)) {
                case WAIT_OBJECT_0:
                  Log()->log_info("Exe terminated");
                  exeTerminated = true;
                  break;

                case WAIT_TIMEOUT:
                  Log()->log_error("WaitForSingleObject timeout {}", WAIT_TIME_EXE_TERMINATION);
                  break;

                default:
                  Log()->log_error("!WaitForSingleObject return {}", res);
              }

              if (!exeTerminated) {
                Log()->log_info("TerminateProcess");
                if (TerminateProcess(s_hProcess, 0)) {
                  s_serviceStatus.dwControlsAccepted = 0;
                  s_serviceStatus.dwCurrentState = SERVICE_STOPPED;
                  s_serviceStatus.dwWin32ExitCode = 0;

                  if (!SetServiceStatus(s_statusHandle, &s_serviceStatus)) {
                    Log()->log_error("!SetServiceStatus SERVICE_STOPPED lastError {:#x}", GetLastError());
                  }
                } else {
                  Log()->log_error("!TerminateProcess lastError {:#x}", GetLastError());
                }
              }
            }
          }
        );

        if (!s_statusHandle) {
          Log()->log_error("!RegisterServiceCtrlHandler lastError {:#x}", GetLastError());
          return;
        }

        // Set service status SERVICE_START_PENDING.
        ZeroMemory(&s_serviceStatus, sizeof(s_serviceStatus));
        s_serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
        s_serviceStatus.dwControlsAccepted = 0;
        s_serviceStatus.dwCurrentState = SERVICE_START_PENDING;
        s_serviceStatus.dwWin32ExitCode = 0;
        s_serviceStatus.dwServiceSpecificExitCode = 0;

        if (!SetServiceStatus(s_statusHandle, &s_serviceStatus)) {
          Log()->log_error("!SetServiceStatus SERVICE_START_PENDING lastError {:#x}", GetLastError());
          return;
        }

        // Set service status SERVICE_RUNNING.
        s_serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
        s_serviceStatus.dwCurrentState = SERVICE_RUNNING;
        s_serviceStatus.dwWin32ExitCode = 0;

        if (!SetServiceStatus(s_statusHandle, &s_serviceStatus)) {
          Log()->log_error("!SetServiceStatus SERVICE_RUNNING lastError {:#x}", GetLastError());

          // Set service status SERVICE_START_PENDING.
          s_serviceStatus.dwControlsAccepted = 0;
          s_serviceStatus.dwCurrentState = SERVICE_STOPPED;
          s_serviceStatus.dwWin32ExitCode = GetLastError();

          if (!SetServiceStatus(s_statusHandle, &s_serviceStatus)) {
            Log()->log_error("!SetServiceStatus SERVICE_STOPPED lastError {:#x}", GetLastError());
          }

          return;
        }

        char filePath[MAX_PATH];
        if (!GetModuleFileName(0, filePath, _countof(filePath))) {
          Log()->log_error("!GetModuleFileName lastError {:#x}", GetLastError());
          return;
        }

        do {
          Log()->log_debug("Start exe path {}", filePath);

          STARTUPINFO startupInfo{};
          startupInfo.cb = sizeof(startupInfo);

          PROCESS_INFORMATION processInformation{};

          if (!CreateProcess(filePath, 0, 0, 0, 0, FALSE, 0, 0, &startupInfo, &processInformation)) {
            Log()->log_error("!CreateProcess lastError {:#x}", GetLastError());
            return;
          }

          s_hProcess = processInformation.hProcess;

          Log()->log_info("{} started", filePath);

          auto res = WaitForSingleObject(processInformation.hProcess, INFINITE);
          CloseHandle(processInformation.hProcess);
          CloseHandle(processInformation.hThread);

          if (WAIT_FAILED == res) {
            Log()->log_error("!WaitForSingleObject hProcess lastError {:#x}", GetLastError());
          } else if (WAIT_OBJECT_0 != res) {
            Log()->log_error("!WaitForSingleObject hProcess return {}", res);
          }

          Log()->log_info("Sleep {} sec before restarting exe", WAIT_TIME_EXE_RESTART/1000);
          res = WaitForSingleObject(s_hEvent, WAIT_TIME_EXE_RESTART);

          if (WAIT_OBJECT_0 == res) {
            Log()->log_info("Service was stopped, exe won't be restarted");
            break;
          }

          if (WAIT_FAILED == res) {
            Log()->log_error("!WaitForSingleObject s_hEvent lastError {:#x}", GetLastError());
          } if (WAIT_TIMEOUT != res) {
            Log()->log_error("!WaitForSingleObject s_hEvent return {}", res);
          }
        } while (true);

        s_serviceStatus.dwControlsAccepted = 0;
        s_serviceStatus.dwCurrentState = SERVICE_STOPPED;
        s_serviceStatus.dwWin32ExitCode = 0;

        if (!SetServiceStatus(s_statusHandle, &s_serviceStatus)) {
          Log()->log_error("!SetServiceStatus SERVICE_STOPPED lastError {:#x}", GetLastError());
        }
      }
    },
    {0, 0}
  };

  if (!StartServiceCtrlDispatcher(serviceTable)) {
    if (ERROR_FAILED_SERVICE_CONTROLLER_CONNECT == GetLastError()) {
      // Run this exe as console.
      return;
    }

    Log()->log_error("!StartServiceCtrlDispatcher lastError {:#x}", GetLastError());

    ExitProcess(1);
  }

  Log()->log_info("Service exit");

  ExitProcess(0);
}

GetTerminationEventHandleReturnType GetTerminationEventHandle() {
  HANDLE hEvent = CreateEvent(nullptr, TRUE, FALSE, SERVICE_TERMINATION_EVENT_NAME);

  return {
    .is_started_by_service = (GetLastError() == ERROR_ALREADY_EXISTS),
    .termination_event_handler = hEvent
  };
}

bool CreateServiceTerminationThread(std::shared_ptr<minifi::core::logging::Logger> logger, HANDLE terminationEventHandle) {
  // Get hService and monitor it - if service is terminated, then terminate current exe, otherwise the exe becomes unmanageable when service is restarted.
  auto hService = [&logger]() -> HANDLE {
    auto hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == hSnapShot) {
      logger->log_error("!CreateToolhelp32Snapshot lastError {:#x}", GetLastError());
      return 0;
    }

    auto getProcessInfo = [&logger, &hSnapShot](DWORD processId, DWORD& parentProcessId, std::string& parentProcessName) {
      parentProcessId = 0;
      parentProcessName.clear();

      PROCESSENTRY32 procentry{};
      procentry.dwSize = sizeof(procentry);

      if (!Process32First(hSnapShot, &procentry)) {
        logger->log_error("!Process32First lastError {:#x}", GetLastError());
        return;
      }

      do {
        if (processId == procentry.th32ProcessID) {
          parentProcessId = procentry.th32ParentProcessID;
          parentProcessName = procentry.szExeFile;
          return;
        }
      } while (Process32Next(hSnapShot, &procentry));
    };

    // Find current process info, which contains parentProcessId.
    DWORD parentProcessId{};
    std::string parentProcessName;
    getProcessInfo(GetCurrentProcessId(), parentProcessId, parentProcessName);

    // Find parent process info (the service which started current process), which contains service name.
    DWORD parentParentProcessId{};
    getProcessInfo(parentProcessId, parentParentProcessId, parentProcessName);

    CloseHandle(hSnapShot);

    // Just in case check that service name == current process name.
    char filePath[MAX_PATH];
    if (!GetModuleFileName(0, filePath, _countof(filePath))) {
      logger->log_error("!GetModuleFileName lastError {:#x}", GetLastError());
      return 0;
    }

    const auto pSlash = strrchr(filePath, '\\');
    if (!pSlash) {
      logger->log_error("Invalid filePath {}", filePath);
      return 0;
    }
    const std::string fileName = pSlash + 1;

    if (_stricmp(fileName.c_str(), parentProcessName.c_str())) {
      logger->log_error("Parent process {} != current process {}", parentProcessName.c_str(), fileName.c_str());
      return 0;
    }

    const auto hParentProcess = OpenProcess(SYNCHRONIZE, FALSE, parentProcessId);
    if (!hParentProcess) {
      logger->log_error("!OpenProcess lastError {:#x}", GetLastError());
      return 0;
    }

    return hParentProcess;
  }();
  if (!hService)
    return false;

  using ThreadInfo = std::tuple<std::shared_ptr<minifi::core::logging::Logger>, HANDLE, HANDLE>;
  auto pThreadInfo = new ThreadInfo(logger, terminationEventHandle, hService);

  HANDLE hThread = (HANDLE)_beginthreadex(
    0, 0,
    [](void* pPar) {
      const auto pThreadInfo = static_cast<ThreadInfo*>(pPar);
      const auto logger = std::get<0>(*pThreadInfo);
      const auto terminationEventHandle = std::get<1>(*pThreadInfo);
      const auto hService = std::get<2>(*pThreadInfo);
      delete pThreadInfo;

      HANDLE arHandle[] = { terminationEventHandle, hService };
      switch (auto res = WaitForMultipleObjects(_countof(arHandle), arHandle, FALSE, INFINITE)) {
        case WAIT_FAILED:
          logger->log_error("!WaitForSingleObject lastError {:#x}", GetLastError());
        break;

        case WAIT_OBJECT_0:
          logger->log_info("Service event received");
        break;

        case WAIT_OBJECT_0 + 1:
          logger->log_info("Service is terminated");
        break;

        default:
          logger->log_info("WaitForMultipleObjects return {}", res);
      }

      SignalExitProcess();

      return 0U;
    },
    pThreadInfo,
    0, 0);
  if (!hThread) {
    logger->log_error("!_beginthreadex lastError {:#x}", GetLastError());

    delete pThreadInfo;

    return false;
  }

  return true;
}

#endif
