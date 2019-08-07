/**
 * @file MiNiFiMain.cpp
 * MiNiFiMain implementation
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


#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <Strsafe.h>
#include <tuple>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "legacy_stdio_definitions.lib")
#ifdef ENABLE_JNI
	#pragma comment(lib, "jvm.lib")
#endif
#include <direct.h>

#endif

#include <fcntl.h>
#include <stdio.h>
#include <cstdlib>
#include <semaphore.h>
#include <signal.h>
#include <vector>
#include <queue>
#include <map>
#include <iostream>
#include "ResourceClaim.h"
#include "core/Core.h"

#include "core/FlowConfiguration.h"
#include "core/ConfigurationFactory.h"
#include "core/RepositoryFactory.h"
#include "FlowController.h"
#include "Main.h"
 // Variables that allow us to avoid a timed wait.
sem_t *running;
//! Flow Controller

/**
 * Removed the stop command from the signal handler so that we could trigger
 * unload after we exit the semaphore controlled critical section in main.
 *
 * Semaphores are a portable choice when using signal handlers. Threads,
 * mutexes, and condition variables are not guaranteed to work within
 * a signal handler. Consequently we will use the semaphore to avoid thread
 * safety issues.
 */

#ifdef WIN32
BOOL WINAPI consoleSignalHandler(DWORD signal) {
	if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT)
	{
		
		sem_post(running);
		if (sem_wait(running) == -1)
			perror("sem_wait");
	}

	return TRUE;
}
#endif
void sigHandler(int signal) {
	if (signal == SIGINT || signal == SIGTERM) {
		// avoid stopping the controller here.
		sem_post(running);
	}
}

#ifdef WIN32

//#define DEBUG_SERVICE

#ifdef DEBUG_SERVICE
  #define LOG_INFO(...)       OutputDebug(__VA_ARGS__)
  #define LOG_ERROR(...)      OutputDebug(__VA_ARGS__)
  #define LOG_LASTERROR(str)  OutputDebug(str " lastError %x", GetLastError())
#else
  #define LOG_INFO(...)       Log()->log_info(__VA_ARGS__)
  #define LOG_ERROR(...)      Log()->log_error(__VA_ARGS__)
  #define LOG_LASTERROR(str)  Log()->log_error(str " lastError %x", GetLastError())
#endif

#undef DEBUG_SERVICE

static char* SERVICE_TERMINATION_EVENT_NAME = "MiNiFiServiceTermination";

void CheckRunAsService() {
  static const int MAX_RETRIES_START_EXE = 3;
  static const int WAIT_TIME_EXE_TERMINATION = 5000;
  static const int WAIT_TIME_BEFORE_EXE_RESTART = 3000;

  static SERVICE_STATUS s_serviceStatus;
  static SERVICE_STATUS_HANDLE s_statusHandle;
  static bool s_stopService;
  static HANDLE s_hProcess;

  static auto OutputDebugArgs = [](const char* format, va_list args)
  {
    char buf[256];
    sprintf_s(buf, _countof(buf), "%s: %s", SERVICE_NAME, format);

    char out[1024];
    StringCbVPrintfA(out, sizeof(out), buf, args);

    OutputDebugStringA(out);
  };

  static auto OutputDebug = [](const char* format, ...) {
    va_list args;
    va_start(args, format);

    OutputDebugArgs(format, args);

    va_end(args);
  };

  static auto Log = []() {
    static std::shared_ptr<logging::Logger> s_logger = logging::LoggerConfiguration::getConfiguration().getLogger("service");
    return s_logger;
  };

  SERVICE_TABLE_ENTRY serviceTable[] =
  {
    {
      SERVICE_NAME,
      [](DWORD argc, LPTSTR *argv)
      {
        LOG_INFO("ServiceCtrlDispatcher");

        s_statusHandle = RegisterServiceCtrlHandler(
          SERVICE_NAME,
          [](DWORD ctrlCode) {
            LOG_INFO("ServiceCtrlHandler ctrlCode %d", ctrlCode);

            if (SERVICE_CONTROL_STOP == ctrlCode) {
              LOG_INFO("ServiceCtrlHandler ctrlCode = SERVICE_CONTROL_STOP");

              s_stopService = true;

              // Set service status SERVICE_STOP_PENDING.
              s_serviceStatus.dwControlsAccepted = 0;
              s_serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
              s_serviceStatus.dwWin32ExitCode = 0;

              if (!SetServiceStatus(s_statusHandle, &s_serviceStatus)) {
                LOG_LASTERROR("!SetServiceStatus SERVICE_STOP_PENDING");
              }

              bool exeTerminated = false;
              auto hEvent = CreateEvent(0, FALSE, FALSE, SERVICE_TERMINATION_EVENT_NAME); 
              if (hEvent) {
                if (GetLastError() == ERROR_ALREADY_EXISTS) {
                  SetEvent(hEvent);

                  LOG_INFO("Wait for exe termination");
                  auto res = WaitForSingleObject(s_hProcess, WAIT_TIME_EXE_TERMINATION);
                  switch (res) {
                  case WAIT_OBJECT_0:
                    LOG_INFO("Exe terminated");
                    exeTerminated = true;
                    break;

                  case WAIT_TIMEOUT:
                    LOG_ERROR("WaitForSingleObject timeout %d", WAIT_TIME_EXE_TERMINATION);
                    break;

                  default:
                    LOG_ERROR("!WaitForSingleObject return %d", res);
                  }
                } else {
                  LOG_ERROR("Termination event doesn't exist");
                }
                CloseHandle(hEvent);
              } else {
                LOG_LASTERROR("!CreateEvent");
              }

              if (!exeTerminated) {
                LOG_INFO("TerminateProcess");
                if (TerminateProcess(s_hProcess, 0)) {
                  s_serviceStatus.dwControlsAccepted = 0;
                  s_serviceStatus.dwCurrentState = SERVICE_STOPPED;
                  s_serviceStatus.dwWin32ExitCode = 0;

                  if (!SetServiceStatus(s_statusHandle, &s_serviceStatus)) {
                    LOG_LASTERROR("!SetServiceStatus SERVICE_STOPPED");
                  }
                } else {
                  LOG_LASTERROR("!TerminateProcess");
                }
              }
            }
          }
        );

        if (!s_statusHandle) {
          LOG_LASTERROR("!RegisterServiceCtrlHandler");
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
          LOG_LASTERROR("!SetServiceStatus SERVICE_START_PENDING");
          return;
        }

        // Set service status SERVICE_RUNNING.
        s_serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
        s_serviceStatus.dwCurrentState = SERVICE_RUNNING;
        s_serviceStatus.dwWin32ExitCode = 0;

        if (!SetServiceStatus(s_statusHandle, &s_serviceStatus)) {
          LOG_LASTERROR("!SetServiceStatus SERVICE_RUNNING");

          // Set service status SERVICE_START_PENDING.
          s_serviceStatus.dwControlsAccepted = 0;
          s_serviceStatus.dwCurrentState = SERVICE_STOPPED;
          s_serviceStatus.dwWin32ExitCode = GetLastError();

          if (!SetServiceStatus(s_statusHandle, &s_serviceStatus)) {
            LOG_LASTERROR("!SetServiceStatus SERVICE_STOPPED");
          }

          return;
        }

        char filePath[MAX_PATH];
        if (!GetModuleFileName(0, filePath, _countof(filePath))) {
          LOG_LASTERROR("!GetModuleFileName");
          return;
        }

        LOG_INFO("Start exe path %s", filePath);
        int retries = 1;
        do {
          STARTUPINFO startupInfo{};
          startupInfo.cb = sizeof(startupInfo);

          PROCESS_INFORMATION processInformation{};

          if (!CreateProcess(filePath, 0, 0, 0, 0, FALSE, 0, 0, &startupInfo, &processInformation)) {
            LOG_LASTERROR("!CreateProcess");
            return;
          }

          s_hProcess = processInformation.hProcess;

          LOG_INFO("%s started", filePath);

          auto res = WaitForSingleObject(processInformation.hProcess, INFINITE);
          CloseHandle(processInformation.hProcess);
          CloseHandle(processInformation.hThread);

          if (s_stopService)
            break;

          if (WAIT_FAILED == res) {
            LOG_LASTERROR("!WaitForSingleObject");
            return;
          }

          if (WAIT_OBJECT_0 != res) {
            LOG_ERROR("!WaitForSingleObject return %d", res);
            return;
          }

          retries++;

          LOG_INFO("Retry start exe %d", retries);

          Sleep(WAIT_TIME_BEFORE_EXE_RESTART);
        } while (retries <= MAX_RETRIES_START_EXE);

        s_serviceStatus.dwControlsAccepted = 0;
        s_serviceStatus.dwCurrentState = SERVICE_STOPPED;
        s_serviceStatus.dwWin32ExitCode = 0;

        if (!SetServiceStatus(s_statusHandle, &s_serviceStatus)) {
          LOG_LASTERROR("!SetServiceStatus SERVICE_STOPPED");
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

    LOG_LASTERROR("!StartServiceCtrlDispatcher");

    ExitProcess(1);
  }

  LOG_INFO("Service exit");

  ExitProcess(0);
}

bool CreateServiceTerminationThread(std::shared_ptr<logging::Logger> logger) {
  HANDLE hEvent = CreateEvent(0, FALSE, FALSE, SERVICE_TERMINATION_EVENT_NAME);
  if (!hEvent) {
    logger->log_error("!CreateEvent lastError %x", GetLastError());
    return false;
  }

  using ThreadInfo = std::tuple<std::shared_ptr<logging::Logger>, HANDLE>;
  auto pThreadInfo = new ThreadInfo(logger, hEvent);

  HANDLE hThread = (HANDLE)_beginthreadex(
    0, 0,
    [](void* pPar) {
      auto pThreadInfo = static_cast<ThreadInfo*>(pPar);
      auto logger = std::get<0>(*pThreadInfo);
      auto hEvent = std::get<1>(*pThreadInfo);
      delete pThreadInfo;

      auto res = WaitForSingleObject(hEvent, INFINITE);
      if (WAIT_FAILED == res) {
        logger->log_error("!WaitForSingleObject");
        return 0U;
      }

      if (WAIT_OBJECT_0 != res) {
        logger->log_error("!WaitForSingleObject return %d", res);
        return 0U;
      }

      sem_post(running);

      return 0U;
    },
    pThreadInfo,
    0, 0);
  if (!hThread) {
    logger->log_error("!_beginthreadex lastError %x", GetLastError());

    delete pThreadInfo;

    return false;
  }

  return true;
}
 
#endif

int main(int argc, char **argv) {
#ifdef WIN32
  CheckRunAsService();
#endif

	std::shared_ptr<logging::Logger> logger = logging::LoggerConfiguration::getConfiguration().getLogger("main");

#ifdef WIN32
  if (!CreateServiceTerminationThread(logger)) {
    return -1;
  }
#endif

	uint16_t stop_wait_time = STOP_WAIT_TIME_MS;

	// initialize static functions that were defined apriori
	core::FlowConfiguration::initialize_static_functions();

	std::string graceful_shutdown_seconds = "";
	std::string prov_repo_class = "provenancerepository";
	std::string flow_repo_class = "flowfilerepository";
	std::string nifi_configuration_class_name = "yamlconfiguration";
	std::string content_repo_class = "filesystemrepository";

	running = sem_open("/MiNiFiMain", O_CREAT, 0644, 0);
	if (running == SEM_FAILED || running == 0) {

		logger->log_error("could not initialize semaphore");
		perror("initialization failure");
	}

#ifdef WIN32
	if (!SetConsoleCtrlHandler(consoleSignalHandler, TRUE)) {
		logger->log_error("Cannot install signal handler");
		std::cerr << "Cannot install signal handler" << std::endl;
		return 1;
	}

	if (signal(SIGINT, sigHandler) == SIG_ERR || signal(SIGTERM, sigHandler) == SIG_ERR ) {
		std::cerr << "Cannot install signal handler" << std::endl;
		return -1;
	}
#ifdef SIGBREAK
	if (signal(SIGBREAK, sigHandler) == SIG_ERR) {
		std::cerr << "Cannot install signal handler" << std::endl;
		return -1;
	}
#endif
#else
	if (signal(SIGINT, sigHandler) == SIG_ERR || signal(SIGTERM, sigHandler) == SIG_ERR || signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		std::cerr << "Cannot install signal handler" << std::endl;
		return -1;
	}
#endif
	// assumes POSIX compliant environment
	std::string minifiHome;
	if (const char *env_p = std::getenv(MINIFI_HOME_ENV_KEY)) {
		minifiHome = env_p;
		logger->log_info("Using MINIFI_HOME=%s from environment.", minifiHome);
	}
	else {
		logger->log_info("MINIFI_HOME is not set; determining based on environment.");
		char *path = nullptr;
		char full_path[PATH_MAX];
#ifndef WIN32
		path = realpath(argv[0], full_path);
#else
		path = nullptr;
#endif

		if (path != nullptr) {
			std::string minifiHomePath(path);
			if (minifiHomePath.find_last_of("/\\") != std::string::npos) {
				minifiHomePath = minifiHomePath.substr(0, minifiHomePath.find_last_of("/\\"));  //Remove /minifi from path
				minifiHome = minifiHomePath.substr(0, minifiHomePath.find_last_of("/\\"));    //Remove /bin from path
			}
		}

		// attempt to use cwd as MINIFI_HOME
		if (minifiHome.empty() || !validHome(minifiHome)) {
			char cwd[PATH_MAX];
#ifdef WIN32
			_getcwd(cwd, PATH_MAX);
#else
			getcwd(cwd, PATH_MAX);
#endif
			minifiHome = cwd;
		}


		logger->log_debug("Setting %s to %s", MINIFI_HOME_ENV_KEY, minifiHome);
#ifdef WIN32
		SetEnvironmentVariable(MINIFI_HOME_ENV_KEY, minifiHome.c_str());
#else
		setenv(MINIFI_HOME_ENV_KEY, minifiHome.c_str(), 0);
#endif
	}

	if (!validHome(minifiHome)) {
		minifiHome = minifiHome.substr(0, minifiHome.find_last_of("/\\"));    //Remove /bin from path
		if (!validHome(minifiHome)) {
			logger->log_error("No valid MINIFI_HOME could be inferred. "
				"Please set MINIFI_HOME or run minifi from a valid location. minifiHome is %s", minifiHome);
			return -1;
		}
	}



	std::shared_ptr<logging::LoggerProperties> log_properties = std::make_shared<logging::LoggerProperties>();
	log_properties->setHome(minifiHome);
	log_properties->loadConfigureFile(DEFAULT_LOG_PROPERTIES_FILE);
	logging::LoggerConfiguration::getConfiguration().initialize(log_properties);

	std::shared_ptr<minifi::Properties> uid_properties = std::make_shared<minifi::Properties>();
	uid_properties->setHome(minifiHome);
	uid_properties->loadConfigureFile(DEFAULT_UID_PROPERTIES_FILE);
	utils::IdGenerator::getIdGenerator()->initialize(uid_properties);

	// Make a record of minifi home in the configured log file.
	logger->log_info("MINIFI_HOME=%s", minifiHome);

	std::shared_ptr<minifi::Configure> configure = std::make_shared<minifi::Configure>();
	configure->setHome(minifiHome);
	configure->loadConfigureFile(DEFAULT_NIFI_PROPERTIES_FILE);


	if (configure->get(minifi::Configure::nifi_graceful_shutdown_seconds, graceful_shutdown_seconds)) {
		try {
			stop_wait_time = std::stoi(graceful_shutdown_seconds);
		}
		catch (const std::out_of_range &e) {
			logger->log_error("%s is out of range. %s", minifi::Configure::nifi_graceful_shutdown_seconds, e.what());
		}
		catch (const std::invalid_argument &e) {
			logger->log_error("%s contains an invalid argument set. %s", minifi::Configure::nifi_graceful_shutdown_seconds, e.what());
		}
	}
	else {
		logger->log_debug("%s not set, defaulting to %d", minifi::Configure::nifi_graceful_shutdown_seconds,
			STOP_WAIT_TIME_MS);
	}

	configure->get(minifi::Configure::nifi_provenance_repository_class_name, prov_repo_class);
	// Create repos for flow record and provenance
	std::shared_ptr<core::Repository> prov_repo = core::createRepository(prov_repo_class, true, "provenance");

	if (!prov_repo->initialize(configure)) {
		std::cerr << "Provenance repository failed to initialize, exiting.." << std::endl;
		exit(1);
	}

	configure->get(minifi::Configure::nifi_flow_repository_class_name, flow_repo_class);

	std::shared_ptr<core::Repository> flow_repo = core::createRepository(flow_repo_class, true, "flowfile");

	if (!flow_repo->initialize(configure)) {
		std::cerr << "Flow file repository failed to initialize, exiting.." << std::endl;
		exit(1);
	}

	configure->get(minifi::Configure::nifi_content_repository_class_name, content_repo_class);

	std::shared_ptr<core::ContentRepository> content_repo = core::createContentRepository(content_repo_class, true, "content");

	if (!content_repo->initialize(configure)) {
		std::cerr << "Content repository failed to initialize, exiting.." << std::endl;
		exit(1);
	}

	std::string content_repo_path;
	if (configure->get(minifi::Configure::nifi_dbcontent_repository_directory_default, content_repo_path)) {
		std::cout << "setting default dir to " << content_repo_path << std::endl;
		minifi::setDefaultDirectory(content_repo_path);
	}

	configure->get(minifi::Configure::nifi_configuration_class_name, nifi_configuration_class_name);

	std::shared_ptr<minifi::io::StreamFactory> stream_factory = minifi::io::StreamFactory::getInstance(configure);

	std::unique_ptr<core::FlowConfiguration> flow_configuration = core::createFlowConfiguration(prov_repo, flow_repo, content_repo, configure, stream_factory, nifi_configuration_class_name);

	std::shared_ptr<minifi::FlowController> controller = std::unique_ptr<minifi::FlowController>(
		new minifi::FlowController(prov_repo, flow_repo, configure, std::move(flow_configuration), content_repo));

	logger->log_info("Loading FlowController");

	// Load flow from specified configuration file
	try {
		controller->load();
	}
	catch (std::exception &e) {
		logger->log_error("Failed to load configuration due to exception: %s", e.what());
		return -1;
	}
	catch (...) {
		logger->log_error("Failed to load configuration due to unknown exception");
		return -1;
	}

	// Start Processing the flow

	controller->start();
	logger->log_info("MiNiFi started");

	/**
	 * Sem wait provides us the ability to have a controlled
	 * yield without the need for a more complex construct and
	 * a spin lock
	 */
	if (sem_wait(running) == -1)
		perror("sem_wait");

	if (sem_close(running) == -1)
		perror("sem_close");

	if (sem_unlink("/MiNiFiMain") == -1)
		perror("sem_unlink");

	/**
	 * Trigger unload -- wait stop_wait_time
	 */
	controller->waitUnload(stop_wait_time);

	flow_repo = nullptr;

	prov_repo = nullptr;

	logger->log_info("MiNiFi exit");

#ifdef WIN32
	sem_post(running);
#endif

	return 0;
}
