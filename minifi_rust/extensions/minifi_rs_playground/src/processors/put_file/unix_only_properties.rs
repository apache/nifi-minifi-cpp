// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use crate::processors::put_file::unix_permissions::UnixPermission;
use minifi_native::Property;

pub(crate) const PERMISSIONS: Property<Option<UnixPermission>> = Property::new(
    "Permissions",
    "Sets the permissions on the output file to the value of this attribute. Must be an octal number (e.g. 644 or 0755). Not supported on Windows systems.",
);

pub(crate) const DIRECTORY_PERMISSIONS: Property<Option<UnixPermission>> = Property::new(
    "Directory Permissions",
    "Sets the permissions on the directories being created if 'Create Missing Directories' property is set. Must be an octal number (e.g. 644 or 0755). Not supported on Windows systems.",
);
