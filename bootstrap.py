#!/usr/bin/python
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

from __future__ import print_function

import argparse
import sys
import json
import os.path
import platform
import tarfile

from distutils.util import strtobool
from ftplib import FTP

if sys.version_info[0] < 3:
    from urllib2 import urlopen
    input = raw_input
else:
    from urllib.request import urlopen

MINIFI_SUBFOLDER = '/nifi/nifi-minifi-cpp/'
APACHE_CLOSER_REPO_JSON_URL = 'https://www.apache.org/dyn/closer.cgi?as_json=1&path=/nifi/nifi-minifi-cpp'
APACHE_MIRROR_LIST = "http://www.apache.org/mirrors/"


def install_package(package_name):
    try:
        import pip
        if hasattr(pip, 'main'):
            pipcode = pip.main(['install', package_name])
        else:
            pipcode = pip._internal.main(['install', package_name])
        return pipcode == 0
    except ImportError:
        return False


distro_available = False

try:
    import distro

    distro_available = True
except ImportError:
    distro_available = install_package("distro")


def get_distro():
    if is_mac():
        return ["osx", "", "darwin"]
    try:
        if distro_available:
            return distro.linux_distribution(full_distribution_name=False)
        else:
            return platform.linux_distribution()
    except Exception:
        return ["N/A", "N/A", "N/A"]


def is_mac():
    return platform.system() == "Darwin"


def mapped_distro():
    distro_info = get_distro()
    distro = distro_info[0].lower()
    release = distro_info[2].lower()
    if any(d in distro for d in ["rhel", "red hat", "centos"]):
        return "rhel", release
    else:
        return distro, release


def find_closest_mirror():
    try:
        url = urlopen(APACHE_CLOSER_REPO_JSON_URL)
        data = json.loads(url.read().decode())

        return data['ftp'][0]
    except Exception:
        print("Failed to find closest mirror, please specify one!")
        return ""


def get_release_and_binaries_from_ftp(host, apache_dir, version=None):
    ftp = FTP(host)
    ftp.login()
    ftp.cwd(apache_dir + MINIFI_SUBFOLDER)
    # list files with ftplib
    file_list = list(filter(lambda x: any(char.isdigit() for char in x),
                            ftp.nlst("")))  # to filter "." and ".." - relese names contain number
    file_list.sort(reverse=True)
    if not version:
        latest_release = file_list[0]
    else:
        if version not in file_list:
            print("The specified version (" + version + ") doesn't exist. Please use one of the following: " + ", ".join(file_list))
            exit(-1)
        latest_release = version

    ftp.cwd("./" + latest_release)
    binaries = list(filter(lambda x: any(char.isdigit() for char in x), ftp.nlst("")))

    ftp.quit()

    return latest_release, binaries


def download_binary_from_ftp(host, apache_dir, release, binary):
    successful_download = False

    try:
        ftp = FTP(host)
        ftp.login()
        ftp.cwd(apache_dir + MINIFI_SUBFOLDER + release)

        print("Downloading: ftp://" + host + "/" + MINIFI_SUBFOLDER + release + "/" + binary)

        with open(os.path.join(os.getcwd(), binary), "wb") as targetfile:
            ftp.retrbinary("RETR " + binary, targetfile.write)
        successful_download = True
    except Exception:
        print("Failed to download binary")
    finally:
        ftp.quit()

    return successful_download


def main(args):
    print(get_distro())
    binaries = []

    try:
        local_repo = args.mirror if args.mirror else find_closest_mirror()

        print(local_repo)

        host, dir = local_repo.replace('ftp://', '').split('/', 1)
        latest_release, binaries = get_release_and_binaries_from_ftp(host, dir, args.version if args.version else None)

    except Exception:
        print("Failed to get binaries from Apache mirror")
        return -1

    matching_binaries = []

    for binary in binaries:
        distro, release = mapped_distro()
        if release and release in binary:
            matching_binaries.append(binary)
        elif distro and distro in binary:
            matching_binaries.append(binary)

    if not matching_binaries:
        print("No compatible binary found, MiNiFi needs to be compiled locally")
        return 1

    invalid_input = True
    download = None
    selected_binary = None

    if len(matching_binaries) == 1:
        print("A binary in Apache repo seems to match your system: " + matching_binaries[0])
        while invalid_input:
            try:
                download = strtobool(input("Would you like to download? [y/n]"))
                invalid_input = False
                if download:
                    selected_binary = matching_binaries[0]
            except Exception:
                pass

    else:
        print("The following binaries in Apache repo seem to match your system: ")
        for i, item in enumerate(matching_binaries):
            print(str(i + 1) + " - " + item)
        print()
        while invalid_input:
            try:
                user_input = input("Please select one to download (1 to " + str(len(matching_binaries)) + ") or \"s\" to skip and compile locally\n")
                user_input.lower()
                if user_input == "s":
                    invalid_input = False
                    download = False
                    break
                idx = int(user_input) - 1
                if (idx < 0):
                    continue
                selected_binary = matching_binaries[idx]
                download = True
                invalid_input = False
            except Exception:
                pass

    if not download:
        return 1

    if not download_binary_from_ftp(host, dir, latest_release, selected_binary):
        return -1

    try:
        with tarfile.open(os.path.join(os.getcwd(), selected_binary), "r:gz") as tar:
            tar.extractall()
    except Exception:
        print("Failed to extract tar file")
        return -1

    print("Successfully downloaded and extracted MiNiFi")
    return 0


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Download latest MiNiFi release")
    parser.add_argument("-m", "--mirror", dest="mirror", help="user-specified apache mirror")
    parser.add_argument("-v", "--version", dest="version", help="user-specified version to be downloaded")
    args = parser.parse_args()
    main(args)
