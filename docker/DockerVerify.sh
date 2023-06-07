#!/bin/bash

# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the \"License\"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an \"AS IS\" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -e

die()
{
  local _ret="${2:-1}"
  test "${_PRINT_HELP:-no}" = yes && print_help >&2
  echo "$1" >&2
  exit "${_ret}"
}

_positionals=()
_arg_image_tag_prefix=
_arg_tags_to_exclude=
_arg_parallel_processes=3

print_help()
{
  printf '%s\n' "Runs the provided behave tests in a containerized environment"
  printf 'Usage: %s [--image-tag-prefix <arg>] [-h|--help] <minifi_version> <feature_path-1> [<feature_path-2>] ... [<feature_path-n>] ...\n' "$0"
  printf '\t%s\n' "<minifi_version>: the version of minifi"
  printf '\t%s\n' "<tags_to_run>: include these tags"
  printf '\t%s\n' "--tags_to_exclude: optional tags that should be skipped (no default)"
  printf '\t%s\n' "--image-tag-prefix: optional prefix to the docker tag (no default)"
  printf '\t%s\n' "--parallel_processes: optional argument that specifies the number of parallel processes that can be executed simultaneously. (default: 3)"
  printf '\t%s\n' "-h, --help: Prints help"
}


parse_commandline()
{
  _positionals_count=0
  while test $# -gt 0
  do
    _key="$1"
    case "$_key" in
      --image-tag-prefix)
        test $# -lt 2 && die "Missing value for the optional argument '$_key'." 1
        _arg_image_tag_prefix="$2"
        shift
        ;;
      --image-tag-prefix=*)
        _arg_image_tag_prefix="${_key##--image-tag-prefix=}"
        ;;
      --tags_to_exclude)
        test $# -lt 2 && die "Missing value for the optional argument '$_key'." 1
        _arg_tags_to_exclude="$2"
        shift
        ;;
      --tags_to_exclude=*)
        _arg_tags_to_exclude="${_key##--tags_to_exclude=}"
        ;;
      --parallel_processes)
        test $# -lt 2 && die "Missing value for the optional argument '$_key'." 1
        _arg_parallel_processes="$2"
        shift
        ;;
      --parallel_processes=*)
        _arg_parallel_processes="${_key##--parallel_processes=}"
        ;;
      -h|--help)
        print_help
        exit 0
        ;;
      -h*)
        print_help
        exit 0
        ;;
      *)
        _last_positional="$1"
        _positionals+=("$_last_positional")
        _positionals_count=$((_positionals_count + 1))
        ;;
    esac
    shift
  done
}


handle_passed_args_count()
{
  local _required_args_string="'minifi_version' and 'tags_to_run'"
  test "${_positionals_count}" -ge 2 || _PRINT_HELP=yes die "FATAL ERROR: Not enough positional arguments - we require at least 2 (namely: $_required_args_string), but got only ${_positionals_count}." 1
}


assign_positional_args()
{
  local _positional_name _shift_for=$1
  _positional_names="_arg_minifi_version _arg_tags_to_run "

  shift "$_shift_for"
  for _positional_name in ${_positional_names}
  do
    test $# -gt 0 || break
    eval "$_positional_name=\${1}" || die "Error during argument parsing." 1
    shift
  done
}

parse_commandline "$@"
handle_passed_args_count
assign_positional_args 1 "${_positionals[@]}"

docker_dir="$( cd "${0%/*}" && pwd )"

# shellcheck disable=SC2154
export MINIFI_VERSION=${_arg_minifi_version}
if test -z "$_arg_image_tag_prefix"
then
  export MINIFI_TAG_PREFIX=""
else
  export MINIFI_TAG_PREFIX=${_arg_image_tag_prefix}-
fi

# Create virtual environment for testing
if [[ ! -d ./test-env-py3 ]]; then
  echo "Creating virtual environment in ./test-env-py3" 1>&2
  virtualenv --python=python3 ./test-env-py3
fi

echo "Activating virtual environment..." 1>&2
# shellcheck disable=SC1091
. ./test-env-py3/bin/activate
pip install --trusted-host pypi.python.org --upgrade pip setuptools

# Install test dependencies
echo "Installing test dependencies..." 1>&2

# hint include/library paths if homewbrew is in use
if brew list 2> /dev/null | grep openssl > /dev/null 2>&1; then
  echo "Using homebrew paths for openssl" 1>&2
  LDFLAGS="-L$(brew --prefix openssl@1.1)/lib"
  export LDFLAGS
  CFLAGS="-I$(brew --prefix openssl@1.1)/include"
  export CFLAGS
  SWIG_FEATURES="-cpperraswarn -includeall -I$(brew --prefix openssl@1.1)/include"
  export SWIG_FEATURES
fi

if ! command swig -version &> /dev/null; then
  echo "Swig could not be found on your system (dependency of m2crypto python library). Please install swig to continue."
  exit 1
fi

pip install -r "${docker_dir}/requirements.txt"
JAVA_HOME="/usr/lib/jvm/default-jvm"
export JAVA_HOME
PATH="$PATH:/usr/lib/jvm/default-jvm/bin"
export PATH

TEST_DIRECTORY="${docker_dir}/test/integration"
export TEST_DIRECTORY

# Add --no-logcapture to see logs interleaved with the test output
BEHAVE_OPTS=(--logging-level INFO --parallel-processes "${_arg_parallel_processes}" --parallel-scheme feature -o "${PWD}/behavex_output" -t "${_arg_tags_to_run}")
if ! test -z "${_arg_tags_to_exclude}"
then
  IFS=','
  read -ra splits <<< "${_arg_tags_to_exclude}"
  for split in "${splits[@]}"
  do
      BEHAVE_OPTS=("${BEHAVE_OPTS[@]}" -t "~${split}")
  done
fi

echo "${BEHAVE_OPTS[@]}"

cd "${docker_dir}/test/integration"
exec
  behavex "${BEHAVE_OPTS[@]}"
