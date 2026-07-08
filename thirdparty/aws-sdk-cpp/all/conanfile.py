#  Licensed to the Apache Software Foundation (ASF) under one or more
#  contributor license agreements.  See the NOTICE file distributed with
#  this work for additional information regarding copyright ownership.
#  The ASF licenses this file to You under the Apache License, Version 2.0
#  (the "License"); you may not use this file except in compliance with
#  the License.  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

# Recipe based on https://github.com/conan-io/conan-center-index/blob/master/recipes/aws-sdk-cpp/all/conanfile.py

import os

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import cross_building
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy, export_conandata_patches, patch, rm, rmdir
from conan.tools.microsoft import is_msvc
from conan.tools.scm import Git, Version

required_conan_version = ">=2"


class AwsSdkCppConan(ConanFile):
    name = "aws-sdk-cpp"
    license = "Apache-2.0"
    url = "https://github.com/conan-io/conan-center-index"
    homepage = "https://github.com/aws/aws-sdk-cpp"
    description = "AWS SDK for C++"
    topics = ("aws", "cpp", "cross-platform", "amazon", "cloud")
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    # This list comes from tools/code-generation/api-description, which then generates the sdk sources
    # To generate for a new one, run the build once and check src/generated/src inside your build_folder, remove the common aws-sdk-cpp prefix
    # NON_GENERATED_CLIENT_LIST in src/cmake/sdks.cmake contains extra ones that should also be added here
    # and that's the list of sdks. Then join it to this one
    _sdks = (
        "AWSMigrationHub",
        "accessanalyzer",
        "account",
        "acm",
        "acm-pca",
        "aiops",
        "amp",
        "amplify",
        "amplifybackend",
        "amplifyuibuilder",
        "apigateway",
        "apigatewaymanagementapi",
        "apigatewayv2",
        "appconfig",
        "appconfigdata",
        "appfabric",
        "appflow",
        "appintegrations",
        "application-autoscaling",
        "application-insights",
        "application-signals",
        "applicationcostprofiler",
        "appmesh",
        "apprunner",
        "appstream",
        "appsync",
        "arc-region-switch",
        "arc-zonal-shift",
        "artifact",
        "athena",
        "auditmanager",
        "autoscaling",
        "autoscaling-plans",
        "awstransfer",
        "b2bi",
        "backup",
        "backup-gateway",
        "backupsearch",
        "batch",
        "bcm-dashboards",
        "bcm-data-exports",
        "bcm-pricing-calculator",
        "bcm-recommended-actions",
        "bedrock",
        "bedrock-agent",
        "bedrock-agent-runtime",
        "bedrock-agentcore",
        "bedrock-agentcore-control",
        "bedrock-data-automation",
        "bedrock-data-automation-runtime",
        "bedrock-runtime",
        "billing",
        "billingconductor",
        "braket",
        "budgets",
        "ce",
        "chatbot",
        "chime",
        "chime-sdk-identity",
        "chime-sdk-media-pipelines",
        "chime-sdk-meetings",
        "chime-sdk-messaging",
        "chime-sdk-voice",
        "cleanrooms",
        "cleanroomsml",
        "cloud9",
        "cloudcontrol",
        "clouddirectory",
        "cloudformation",
        "cloudfront",
        "cloudfront-keyvaluestore",
        "cloudhsm",
        "cloudhsmv2",
        "cloudsearch",
        "cloudsearchdomain",
        "cloudtrail",
        "cloudtrail-data",
        "codeartifact",
        "codebuild",
        "codecatalyst",
        "codecommit",
        "codeconnections",
        "codedeploy",
        "codeguru-reviewer",
        "codeguru-security",
        "codeguruprofiler",
        "codepipeline",
        "codestar-connections",
        "codestar-notifications",
        "cognito-identity",
        "cognito-idp",
        "cognito-sync",
        "comprehend",
        "comprehendmedical",
        "compute-optimizer",
        "config",
        "connect",
        "connect-contact-lens",
        "connectcampaigns",
        "connectcampaignsv2",
        "connectcases",
        "connectparticipant",
        "controlcatalog",
        "controltower",
        "cost-optimization-hub",
        "cur",
        "customer-profiles",
        "databrew",
        "dataexchange",
        "datapipeline",
        "datasync",
        "datazone",
        "dax",
        "deadline",
        "detective",
        "devicefarm",
        "devops-guru",
        "directconnect",
        "directory-service-data",
        "discovery",
        "dlm",
        "dms",
        "docdb",
        "docdb-elastic",
        "drs",
        "ds",
        "dsql",
        "dynamodb",
        "dynamodbstreams",
        "ebs",
        "ec2",
        "ec2-instance-connect",
        "ecr",
        "ecr-public",
        "ecs",
        "eks",
        "eks-auth",
        "elasticache",
        "elasticbeanstalk",
        "elasticfilesystem",
        "elasticloadbalancing",
        "elasticloadbalancingv2",
        "elasticmapreduce",
        "elastictranscoder",
        "email",
        "emr-containers",
        "emr-serverless",
        "entityresolution",
        "es",
        "eventbridge",
        "events",
        "evidently",
        "evs",
        "finspace",
        "finspace-data",
        "firehose",
        "fis",
        "fms",
        "forecast",
        "forecastquery",
        "frauddetector",
        "freetier",
        "fsx",
        "gamelift",
        "gameliftstreams",
        "geo-maps",
        "geo-places",
        "geo-routes",
        "glacier",
        "globalaccelerator",
        "glue",
        "grafana",
        "greengrass",
        "greengrassv2",
        "groundstation",
        "guardduty",
        "health",
        "healthlake",
        "iam",
        "identitystore",
        "imagebuilder",
        "importexport",
        "inspector",
        "inspector-scan",
        "inspector2",
        "internetmonitor",
        "invoicing",
        "iot",
        "iot-data",
        "iot-jobs-data",
        "iot-managed-integrations",
        "iotanalytics",
        "iotdeviceadvisor",
        "iotevents",
        "iotevents-data",
        "iotfleetwise",
        "iotsecuretunneling",
        "iotsitewise",
        "iotthingsgraph",
        "iottwinmaker",
        "iotwireless",
        "ivs",
        "ivs-realtime",
        "ivschat",
        "kafka",
        "kafkaconnect",
        "kendra",
        "kendra-ranking",
        "keyspaces",
        "keyspacesstreams",
        "kinesis",
        "kinesis-video-archived-media",
        "kinesis-video-media",
        "kinesis-video-signaling",
        "kinesis-video-webrtc-storage",
        "kinesisanalytics",
        "kinesisanalyticsv2",
        "kinesisvideo",
        "kms",
        "lakeformation",
        "lambda",
        "launch-wizard",
        "lex",
        "lex-models",
        "lexv2-models",
        "lexv2-runtime",
        "license-manager",
        "license-manager-linux-subscriptions",
        "license-manager-user-subscriptions",
        "lightsail",
        "location",
        "logs",
        "lookoutequipment",
        "m2",
        "machinelearning",
        "macie2",
        "mailmanager",
        "managedblockchain",
        "managedblockchain-query",
        "marketplace-agreement",
        "marketplace-catalog",
        "marketplace-deployment",
        "marketplace-entitlement",
        "marketplace-reporting",
        "marketplacecommerceanalytics",
        "mediaconnect",
        "mediaconvert",
        "medialive",
        "mediapackage",
        "mediapackage-vod",
        "mediapackagev2",
        "mediastore",
        "mediastore-data",
        "mediatailor",
        "medical-imaging",
        "memorydb",
        "meteringmarketplace",
        "mgn",
        "migration-hub-refactor-spaces",
        "migrationhub-config",
        "migrationhuborchestrator",
        "migrationhubstrategy",
        "monitoring",
        "mpa",
        "mq",
        "mturk-requester",
        "mwaa",
        "neptune",
        "neptune-graph",
        "neptunedata",
        "network-firewall",
        "networkflowmonitor",
        "networkmanager",
        "networkmonitor",
        "notifications",
        "notificationscontacts",
        "oam",
        "observabilityadmin",
        "odb",
        "omics",
        "opensearch",
        "opensearchserverless",
        "organizations",
        "osis",
        "outposts",
        "panorama",
        "partnercentral-selling",
        "payment-cryptography",
        "payment-cryptography-data",
        "pca-connector-ad",
        "pca-connector-scep",
        "pcs",
        "personalize",
        "personalize-events",
        "personalize-runtime",
        "pi",
        "pinpoint",
        "pinpoint-email",
        "pinpoint-sms-voice-v2",
        "pipes",
        "polly",
        "pricing",
        "proton",
        "qapps",
        "qbusiness",
        "qconnect",
        "quicksight",
        "ram",
        "rbin",
        "rds",
        "rds-data",
        "redshift",
        "redshift-data",
        "redshift-serverless",
        "rekognition",
        "repostspace",
        "resiliencehub",
        "resource-explorer-2",
        "resource-groups",
        "resourcegroupstaggingapi",
        "rolesanywhere",
        "route53",
        "route53-recovery-cluster",
        "route53-recovery-control-config",
        "route53-recovery-readiness",
        "route53domains",
        "route53profiles",
        "route53resolver",
        "rtbfabric",
        "rum",
        "s3",
        "s3-crt",
        "s3control",
        "s3outposts",
        "s3tables",
        "s3vectors",
        "sagemaker",
        "sagemaker-a2i-runtime",
        "sagemaker-edge",
        "sagemaker-featurestore-runtime",
        "sagemaker-geospatial",
        "sagemaker-metrics",
        "sagemaker-runtime",
        "savingsplans",
        "scheduler",
        "schemas",
        "sdb",
        "secretsmanager",
        "security-ir",
        "securityhub",
        "securitylake",
        "serverlessrepo",
        "service-quotas",
        "servicecatalog",
        "servicecatalog-appregistry",
        "servicediscovery",
        "sesv2",
        "shield",
        "signer",
        "simspaceweaver",
        "sms-voice",
        "snow-device-management",
        "snowball",
        "sns",
        "socialmessaging",
        "sqs",
        "ssm",
        "ssm-contacts",
        "ssm-guiconnect",
        "ssm-incidents",
        "ssm-quicksetup",
        "ssm-sap",
        "sso",
        "sso-admin",
        "sso-oidc",
        "states",
        "storagegateway",
        "sts",
        "supplychain",
        "support",
        "support-app",
        "swf",
        "synthetics",
        "taxsettings",
        "textract",
        "timestream-influxdb",
        "timestream-query",
        "timestream-write",
        "tnb",
        "transcribe",
        "transcribestreaming",
        "translate",
        "trustedadvisor",
        "verifiedpermissions",
        "voice-id",
        "vpc-lattice",
        "waf",
        "waf-regional",
        "wafv2",
        "wellarchitected",
        "wisdom",
        "workdocs",
        "workmail",
        "workmailmessageflow",
        "workspaces",
        "workspaces-instances",
        "workspaces-thin-client",
        "workspaces-web",
        "xray",
        # Extra modules that are not generated but exist upstream
        "access-management",
        "text-to-speech",
        "queues",
        "s3-encryption",
        "identity-management",
        "transfer"
    )
    options = {
        **{
            "shared": [True, False],
            "fPIC": [True, False],
            "min_size": [True, False],
        },
        **{sdk_name: [None, True, False] for sdk_name in _sdks},
    }
    default_options = {
        **{
            "shared": False,
            "fPIC": True,
            "min_size": False
        },
        **{sdk_name: None for sdk_name in _sdks},
    }

    def export_sources(self):
        export_conandata_patches(self)

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

        for sdk_name in self._sdks:
            if self.options.get_safe(sdk_name) == None:  # noqa
                setattr(self.options, sdk_name, False)

    def layout(self):
        cmake_layout(self, src_folder="src")

    def requirements(self):
        self.requires("openssl/3.6.2", transitive_headers=True)
        if self.settings.os != "Windows":
            # Used transitively in core/http/curl/CurlHandleContainer.h public header
            self.requires("libcurl/8.20.0", transitive_headers=True)
        if self.settings.os == "Linux":
            # Pulseaudio -> libcap, libalsa only support linux, don't use pulseaudio on other platforms
            if self.options.get_safe("text-to-speech"):
                # Used transitively in text-to-speech/PulseAudioPCMOutputDriver.h public header
                self.requires("pulseaudio/14.2", transitive_headers=True, transitive_libs=True)
        # zlib is used if ENABLE_ZLIB_REQUEST_COMPRESSION is enabled, set ot ON by default
        self.requires("zlib/1.3.2")

    def validate_build(self):
        if self.settings_build.os == "Windows" and self.settings.os == "Android":
            raise ConanInvalidConfiguration("Cross-building from Windows to Android is not supported")

        if (self.options.shared
                and self.settings.compiler == "gcc"
                and Version(self.settings.compiler.version) < "6.0"):
            raise ConanInvalidConfiguration(
                "Doesn't support gcc5 / shared. "
                "See https://github.com/conan-io/conan-center-index/pull/4401#issuecomment-802631744"
            )

    def validate(self):
        if (is_msvc(self) and self.options.shared
                and not self.dependencies["aws-c-common"].options.shared):
            raise ConanInvalidConfiguration(f"{self.ref} with shared is not supported with aws-c-common static")

    def source(self):
        git = Git(self)
        git.run("init .")
        git.run("remote add origin https://github.com/aws/aws-sdk-cpp.git")
        git.run(f"fetch --depth 1 origin refs/tags/{self.version}")
        git.run("checkout --force FETCH_HEAD")
        git.run("submodule update --init --recursive --depth 1")

        for patch_data in self.conan_data.get("patches", {}).get(self.version, []):
            patch_file = patch_data["patch_file"]
            patch(self, patch_file=os.path.join(self.export_sources_folder, patch_file),
                  base_path=self.source_folder, strip=0, fuzz=True)

    def _enabled_sdks(self):
        return [sdk for sdk in self._sdks
                if self.options.get_safe(sdk)]

    def generate(self):
        tc = CMakeToolchain(self)
        # All option() are defined before project() in upstream CMakeLists,
        # therefore we must use cache_variables

        build_only = ["core"] + self._enabled_sdks()
        tc.cache_variables["BUILD_ONLY"] = ";".join(build_only)

        tc.cache_variables["ENABLE_UNITY_BUILD"] = True
        tc.cache_variables["ENABLE_TESTING"] = False
        tc.cache_variables["AUTORUN_UNIT_TESTS"] = False
        tc.cache_variables["BUILD_DEPS"] = True
        tc.cache_variables["USE_OPENSSL"] = True
        tc.cache_variables["ENABLE_OPENSSL_ENCRYPTION"] = True
        # Point the SDK's / s2n's OpenSSL discovery at the conan-provided OpenSSL.
        tc.cache_variables["OPENSSL_ROOT_DIR"] = self.dependencies["openssl"].package_folder

        tc.cache_variables["MINIMIZE_SIZE"] = self.options.min_size
        tc.cache_variables['AWS_STATIC_MSVC_RUNTIME_LIBRARY'] = self.settings.os == "Windows" and self.settings.get_safe("compiler.runtime") == "static"

        tc.cache_variables["USE_CRT_HTTP_CLIENT"] = True

        if self.settings.os == "Windows":
            tc.cache_variables["FORCE_EXPORT_CORE_API"] = True
            tc.cache_variables["FORCE_EXPORT_S3_API"] = True
            tc.cache_variables["FORCE_EXPORT_S3_CRT_API"] = True
            tc.cache_variables["FORCE_EXPORT_KINESIS_API"] = True

        if cross_building(self):
            tc.cache_variables["CURL_HAS_H2_EXITCODE"] = "0"
            tc.cache_variables["CURL_HAS_H2_EXITCODE__TRYRUN_OUTPUT"] = ""
            tc.cache_variables["CURL_HAS_TLS_PROXY_EXITCODE"] = "0"
            tc.cache_variables["CURL_HAS_TLS_PROXY_EXITCODE__TRYRUN_OUTPUT"] = ""
        if is_msvc(self):
            tc.preprocessor_definitions["_SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING"] = "1"
        tc.cache_variables["BUILD_SHARED_LIBS"] = self.options.shared
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        if is_msvc(self):
            copy(self, "*.lib", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)
            rm(self, "*.lib", os.path.join(self.package_folder, "bin"))

        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "AWSSDK")

        is_windows = self.settings.os == "Windows"
        is_apple = self.settings.os == "Macos"
        has_s2n = self.settings.os in ["Linux", "FreeBSD"]

        # CRT libraries built in-tree from the SDK submodules (BUILD_DEPS=ON). Target names
        # and link edges mirror cmake/BundledAwsSdkCpp.cmake; the edges below are the complete
        # dependency set so the flattened static-link order is always valid.
        def crt_component(name, requires=None):
            comp = self.cpp_info.components[name]
            comp.set_property("cmake_target_name", f"AWS::{name}")
            comp.libs = [name]
            comp.requires = requires or []

        crt_component("aws-c-common")
        crt_component("aws-checksums", ["aws-c-common"])
        crt_component("aws-c-sdkutils", ["aws-c-common"])
        crt_component("aws-c-cal", ["aws-c-common"])
        crt_component("aws-c-compression", ["aws-c-common"])
        crt_component("aws-c-io", ["aws-c-common", "aws-c-cal"] + (["s2n"] if has_s2n else []))
        crt_component("aws-c-event-stream", ["aws-c-common", "aws-checksums", "aws-c-io"])
        crt_component("aws-c-http", ["aws-c-common", "aws-c-compression", "aws-c-cal", "aws-c-io"])
        crt_component("aws-c-auth", ["aws-c-common", "aws-c-cal", "aws-c-io", "aws-c-http", "aws-c-sdkutils"])
        crt_component("aws-c-mqtt", ["aws-c-common", "aws-c-http", "aws-c-io"])
        crt_component("aws-c-s3", ["aws-c-common", "aws-c-cal", "aws-c-auth", "aws-c-http", "aws-c-io", "aws-checksums"])
        crt_component("aws-crt-cpp", ["aws-c-common", "aws-c-sdkutils", "aws-c-io", "aws-c-cal", "aws-c-compression",
                                      "aws-c-http", "aws-c-auth", "aws-c-mqtt", "aws-checksums", "aws-c-event-stream", "aws-c-s3"])

        if has_s2n:
            s2n = self.cpp_info.components["s2n"]
            s2n.set_property("cmake_target_name", "AWS::s2n")
            s2n.libs = ["s2n"]
            s2n.requires = ["openssl::openssl"]

        # core component
        core = self.cpp_info.components["core"]
        core.set_property("cmake_target_name", "AWS::aws-cpp-sdk-core")
        core.set_property("pkg_config_name", "aws-sdk-cpp-core")
        core.libs = ["aws-cpp-sdk-core"]
        core.requires = ["aws-crt-cpp", "aws-c-event-stream", "zlib::zlib", "openssl::openssl"]
        if not is_windows:
            core.requires.append("libcurl::curl")

        # SDK client components (s3, s3-crt, kinesis, ...)
        for sdk in self._enabled_sdks():
            comp = self.cpp_info.components[sdk]
            comp.set_property("cmake_target_name", f"AWS::aws-cpp-sdk-{sdk}")
            comp.set_property("pkg_config_name", f"aws-sdk-cpp-{sdk}")
            comp.libs = ["aws-cpp-sdk-" + sdk]
            comp.requires = ["core"]
        if self.options.get_safe("s3-crt"):
            self.cpp_info.components["s3-crt"].requires.append("aws-crt-cpp")

        # platform-specific system libs / frameworks (mirrors BundledAwsSdkCpp.cmake:157-226)
        if is_windows:
            core.system_libs.extend([
                "userenv", "ws2_32", "wininet", "bcrypt", "version",
                "secur32", "crypt32", "shlwapi", "winhttp"])
            # aws-c-io's Windows TLS (windows_pki_utils.c) needs the NCrypt API.
            self.cpp_info.components["aws-c-io"].system_libs.append("ncrypt")
            if self.options.get_safe("text-to-speech"):
                self.cpp_info.components["text-to-speech"].system_libs.append("winmm")
        elif is_apple:
            core.frameworks.extend(["CoreFoundation", "Security", "Network"])
            if self.options.get_safe("text-to-speech"):
                self.cpp_info.components["text-to-speech"].frameworks.extend(["CoreAudio", "AudioToolbox"])

        if self.settings.os in ["Linux", "FreeBSD"]:
            core.system_libs.extend(["atomic", "pthread", "dl", "rt", "m"])
            if self.options.get_safe("text-to-speech"):
                self.cpp_info.components["text-to-speech"].requires.append("pulseaudio::pulseaudio")
