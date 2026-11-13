// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use crate::{GetProperty, LogLevel, Logger, MinifiError};

/// This RawControllerService will be instantiated, and called on by the agent
pub trait RawControllerService: Sized {
    type LoggerType: Logger;

    fn new(logger: Self::LoggerType) -> Self;
    fn log(&self, log_level: LogLevel, args: std::fmt::Arguments);
    fn enable<P: GetProperty>(&mut self, context: &P) -> Result<(), MinifiError>;
    fn disable(&mut self) {}
}
