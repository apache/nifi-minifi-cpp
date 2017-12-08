#!/bin/bash
verify_enable(){
	echo "true"
}
add_os_flags() {
	:
}
bootstrap_cmake(){
	sudo yum -y install wget
	wget https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
	sudo yum -y install epel-release-latest-7.noarch.rpm
    sudo yum -y install cmake3
}
build_deps(){
# Install epel-release so that cmake3 will be available for installation
sudo yum -y install wget
wget https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
sudo yum -y install epel-release-latest-7.noarch.rpm

COMMAND="sudo yum -y install gcc gcc-c++ libuuid libuuid-devel"
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
				INSTALLED+=("libcurl-devel")
				elif [ "$FOUND_VALUE" = "libpcap" ]; then				
					INSTALLED+=("libpcap-devel")	
				elif [ "$FOUND_VALUE" = "openssl" ]; then				
					INSTALLED+=("openssl")	
					INSTALLED+=("openssl-devel")	
				elif [ "$FOUND_VALUE" = "libusb" ]; then				
					INSTALLED+=("libusb-devel")	
				elif [ "$FOUND_VALUE" = "libpng" ]; then				
					INSTALLED+=("libpng-devel")	
				elif [ "$FOUND_VALUE" = "bison" ]; then				
					INSTALLED+=("bison")	
				elif [ "$FOUND_VALUE" = "flex" ]; then				
					INSTALLED+=("flex")	
				elif [ "$FOUND_VALUE" = "python" ]; then				
					INSTALLED+=("python3-devel")	
				elif [ "$FOUND_VALUE" = "lua" ]; then				
					INSTALLED+=("lua-devel")	
				elif [ "$FOUND_VALUE" = "gpsd" ]; then				
					INSTALLED+=("gpsd-devel")	
				elif [ "$FOUND_VALUE" = "libarchive" ]; then				
					INSTALLED+=("xz-devel")	
					INSTALLED+=("bzip2-devel")
				fi
    		fi
		done

	fi
done

for option in "${INSTALLED[@]}" ; do
	COMMAND="${COMMAND} $option"
done

${COMMAND}

}
