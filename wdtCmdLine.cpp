/*
 * Copyright 2014 Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Sender.h"
#include "Receiver.h"
#include <folly/String.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <iostream>
#include <signal.h>
#include "WdtFlags.h"

DEFINE_bool(run_as_daemon, true,
            "If true, run the receiver as never ending process");

DEFINE_string(directory, ".", "Source/Destination directory");
DEFINE_bool(files, false,
            "If true, read a list of files and optional "
            "filesizes from stdin relative to the directory and transfer then");
DEFINE_string(
    destination, "",
    "empty is server (destination) mode, non empty is destination host");

DECLARE_bool(logtostderr);  // default of standard glog is off - let's set it on

using namespace facebook::wdt;

int main(int argc, char *argv[]) {
  FLAGS_logtostderr = true;
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  signal(SIGPIPE, SIG_IGN);
  WdtFlags::initializeFromFlags();
  LOG(INFO) << "Starting with directory = " << FLAGS_directory
            << " and destination = " << FLAGS_destination
            << " num sockets = " << FLAGS_wdt_num_ports
            << " from port = " << FLAGS_wdt_start_port;
  const auto &options = WdtOptions::getMutable();
  LOG(INFO) << "options " << options.port_ << " " << options.numSockets_;

  ErrorCode retCode = OK;
  if (FLAGS_destination.empty()) {
    Receiver receiver(FLAGS_wdt_start_port, FLAGS_wdt_num_ports,
                      FLAGS_directory);
    // TODO fix this
    if (!FLAGS_run_as_daemon) {
      receiver.transferAsync();
      std::unique_ptr<TransferReport> report = receiver.finish();
      retCode = report->getSummary().getErrorCode();
    } else {
      receiver.runForever();
      retCode = OK;
    }
    return retCode;
  } else {
    std::vector<FileInfo> fileInfo;
    if (FLAGS_files) {
      // Each line should have the filename and optionally
      // the filesize separated by a single space
      std::string line;
      while (std::getline(std::cin, line)) {
        std::vector<std::string> fields;
        folly::split('\t', line, fields, true);
        if (fields.empty() || fields.size() > 2) {
          LOG(FATAL) << "Invalid input in stdin: " << line;
        }
        int64_t filesize =
            fields.size() > 1 ? folly::to<int64_t>(fields[1]) : -1;
        fileInfo.emplace_back(fields[0], filesize);
      }
    }
    Sender sender(FLAGS_destination, FLAGS_directory);
    sender.setIncludeRegex(FLAGS_wdt_include_regex);
    sender.setExcludeRegex(FLAGS_wdt_exclude_regex);
    sender.setPruneDirRegex(FLAGS_wdt_prune_dir_regex);
    sender.setSrcFileInfo(fileInfo);
    // TODO fix that
    auto report = sender.start();
    retCode = report->getSummary().getErrorCode();
  }
  return retCode;
}
