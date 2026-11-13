# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if (TARGET check_epollexclusive)
    set_target_properties(check_epollexclusive PROPERTIES LINKER_LANGUAGE CXX)
endif()
