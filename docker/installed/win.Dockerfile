#escape=`

# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements. See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership. The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License. You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the License for the
# specific language governing permissions and limitations
# under the License.
#

FROM mcr.microsoft.com/windows/servercore:ltsc2022

LABEL maintainer="Apache NiFi <dev@nifi.apache.org>"

ARG MSI_SOURCE="nifi-minifi-cpp.msi"

ENV MINIFI_HOME="C:\Program Files\ApacheNiFiMiNiFi\nifi-minifi-cpp"

SHELL ["powershell", "-Command", "$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue';"]

RUN Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))

RUN choco install git -y
RUN choco install vcredist140 -y

COPY ${MSI_SOURCE} C:\temp\minifi.msi

SHELL ["cmd", "/S", "/C"]

RUN C:\Windows\System32\msiexec.exe /i C:\temp\minifi.msi /qn /norestart /L*V C:\minifi_install.log

CMD ["C:\\Program Files\\ApacheNiFiMiNiFi\\nifi-minifi-cpp\\bin\\minifi.exe"]