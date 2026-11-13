// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#ifdef WIN32
  #ifdef LIBMINIFI
    #define MINIFIAPI __declspec(dllexport)
  #else
    #define MINIFIAPI __declspec(dllimport)
  #endif
  #ifdef MODULE_NAME
    #define EXTENSIONAPI __declspec(dllexport)
  #else
    #define EXTENSIONAPI __declspec(dllimport)
  #endif
#else
  #define MINIFIAPI
  #define EXTENSIONAPI
#endif
