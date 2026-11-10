// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#ifdef WIN32
  #ifdef EXTENSION_NAME
    #define EXTENSIONAPI __declspec(dllexport)
  #else
    #define EXTENSIONAPI __declspec(dllimport)
  #endif
#else
  #define EXTENSIONAPI
#endif
