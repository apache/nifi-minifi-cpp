// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use crate::api::errors::MinifiError;
use crate::{LogLevel, Logger, ProcessContext, ProcessSession};
use std::marker::PhantomData;

pub enum ProcessorInputRequirement {
    Required,
    Allowed,
    Forbidden,
}

#[derive(Debug, PartialEq, Eq)]
pub enum OnTriggerResult {
    Ok,
    Yield,
}

/// This RawProcessor will be instantiated, and called on by the agent
pub trait RawProcessor: Sized {
    type Threading: ThreadingModel;
    type LoggerType: Logger;

    fn new(logger: Self::LoggerType) -> Self;
    fn log(&self, log_level: LogLevel, args: std::fmt::Arguments);
    fn schedule<P: ProcessContext>(&mut self, context: &P) -> Result<(), MinifiError>;
    fn unschedule(&mut self);
}

/// To differentiate between single and multithreaded processors
pub trait ThreadingModel: sealed::Sealed {
    const IS_EXCLUSIVE: bool;
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct MultiThreaded;
impl ThreadingModel for MultiThreaded {
    const IS_EXCLUSIVE: bool = false;
}

/// Whether the framework loads and saves the processor's `#[persisted]` fields around each trigger.
pub trait StatePolicy: sealed::Sealed {}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Stateless;
impl StatePolicy for Stateless {}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Stateful;
impl StatePolicy for Stateful {}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct SingleThreaded<S: StatePolicy = Stateless>(PhantomData<S>);
impl<S: StatePolicy> ThreadingModel for SingleThreaded<S> {
    const IS_EXCLUSIVE: bool = true;
}

/// A single-threaded processor whose `StateSchema` is persisted with each trigger's session.
pub type SingleThreadedStateful = SingleThreaded<Stateful>;

pub trait IsSingleThreaded: sealed::Sealed {}
impl<S: StatePolicy> IsSingleThreaded for SingleThreaded<S> {}

mod sealed {
    pub trait Sealed {}
    impl Sealed for super::MultiThreaded {}
    impl<S: super::StatePolicy> Sealed for super::SingleThreaded<S> {}
    impl Sealed for super::Stateless {}
    impl Sealed for super::Stateful {}
}

pub trait SingleThreadedTrigger: RawProcessor<Threading: IsSingleThreaded> {
    fn trigger<PC, PS>(
        &mut self,
        context: &mut PC,
        session: &mut PS,
    ) -> Result<OnTriggerResult, MinifiError>
    where
        PC: ProcessContext,
        PS: ProcessSession<FlowFile = PC::FlowFile>;
}

pub trait MultiThreadedTrigger: RawProcessor<Threading = MultiThreaded> {
    fn trigger<PC, PS>(
        &self,
        context: &mut PC,
        session: &mut PS,
    ) -> Result<OnTriggerResult, MinifiError>
    where
        PC: ProcessContext,
        PS: ProcessSession<FlowFile = PC::FlowFile>;
}
