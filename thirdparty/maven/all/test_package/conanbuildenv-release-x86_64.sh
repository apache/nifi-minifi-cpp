script_folder="/home/ubuntu/src/james/pipeline/nifi-minifi-cpp/thirdparty/maven/all/test_package"
echo "echo Restoring environment" > "$script_folder/deactivate_conanbuildenv-release-x86_64.sh"
for v in MAVEN_HOME MAVEN_EXECUTABLE PATH JAVA_HOME
do
    is_defined="true"
    value=$(printenv $v) || is_defined="" || true
    if [ -n "$value" ] || [ -n "$is_defined" ]
    then
        echo export "$v='$value'" >> "$script_folder/deactivate_conanbuildenv-release-x86_64.sh"
    else
        echo unset $v >> "$script_folder/deactivate_conanbuildenv-release-x86_64.sh"
    fi
done


export MAVEN_HOME="/home/ubuntu/.conan2/p/b/maven7207cfc89c6f4/p"
export MAVEN_EXECUTABLE="/home/ubuntu/.conan2/p/b/maven7207cfc89c6f4/p/bin/mvn"
export PATH="/home/ubuntu/.conan2/p/b/maven7207cfc89c6f4/p/bin:/home/ubuntu/.conan2/p/zulu-68f60a2c42691/p/bin:$PATH"
export JAVA_HOME="/home/ubuntu/.conan2/p/zulu-68f60a2c42691/p"