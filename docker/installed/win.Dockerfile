#escape=`

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