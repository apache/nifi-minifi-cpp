// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include <iostream>
#include <chrono>
#include <thread>

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "Usage: ./EchoParameters <delay between parameters milliseconds> <text to write>" << std::endl;
    return 1;
  }

  std::cout << argv[2] << std::endl;
  for (int i = 3; i < argc; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(std::stoi(argv[1])));
    std::cout << argv[i] << std::endl;
  }
  return 0;
}
