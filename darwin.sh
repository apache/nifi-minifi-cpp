#!/bin/bash

verify_enable() {
  feature="$1"
  feature_status=${!1}
  if [ "$feature" = "BUSTACHE_ENABLED" ]; then
  	BUSTACHE_MAX="9"
    ## we should check the xcode version
    CLANG_VERSION=`clang --version | head -n 1 | awk '{print $4}'`
    CLANG_MAJOR=`echo $CLANG_VERSION | cut -d. -f1`
    CLANG_MINOR=`echo $CLANG_VERSION | cut -d. -f2`
    CLANG_REVISION=`echo $CLANG_VERSION | cut -d. -f3`
    if [ "$CLANG_MAJOR" -ge "$BUSTACHE_MAX" ]; then
      echo "false"
    else
      echo "true"
    fi
  else
    echo "true"
  fi
}

add_os_flags(){
  :
}

bootstrap_cmake(){
  brew install cmake
}
build_deps(){

  COMMAND="brew install cmake"
  INSTALLED=()
  for option in "${OPTIONS[@]}" ; do
    option_value="${!option}"
    if [ "$option_value" = "${TRUE}" ]; then
      # option is enabled
      FOUND_VALUE=""
      for cmake_opt in "${DEPENDENCIES[@]}" ; do
        KEY=${cmake_opt%%:*}
        VALUE=${cmake_opt#*:}
        if [ "$KEY" = "$option" ]; then
          FOUND_VALUE="$VALUE"
          if [ "$FOUND_VALUE" = "libcurl" ]; then
            INSTALLED+=("curl")
          elif [ "$FOUND_VALUE" = "libpcap" ]; then
            INSTALLED+=("libpcap")
          elif [ "$FOUND_VALUE" = "openssl" ]; then
            INSTALLED+=("openssl")
          elif [ "$FOUND_VALUE" = "libusb" ]; then
            INSTALLED+=("libusb")
          elif [ "$FOUND_VALUE" = "libpng" ]; then
            INSTALLED+=("libpng")
          elif [ "$FOUND_VALUE" = "bison" ]; then
            INSTALLED+=("bison")
          elif [ "$FOUND_VALUE" = "flex" ]; then
            INSTALLED+=("flex")
          elif [ "$FOUND_VALUE" = "python" ]; then
            INSTALLED+=("python")
          elif [ "$FOUND_VALUE" = "lua" ]; then
            INSTALLED+=("lua")
          elif [ "$FOUND_VALUE" = "gpsd" ]; then
            INSTALLED+=("gpsd")
          elif [ "$FOUND_VALUE" = "libarchive" ]; then
            INSTALLED+=("xz")
            INSTALLED+=("bzip2")
          fi
        fi
      done

    fi
  done

  for option in "${INSTALLED[@]}" ; do
    COMMAND="${COMMAND} $option"
  done

  echo "Ensuring you have all dependencies installed..."
  ${COMMAND} > /dev/null 2>&1

  for option in "${INSTALLED[@]}" ; do
    if [ "$option" = "curl" ]; then
      brew link curl --force > /dev/null 2>&1
    fi
  done

}
