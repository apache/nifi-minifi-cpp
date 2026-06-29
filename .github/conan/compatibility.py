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

from conan.tools.build import supported_cppstd, supported_cstd


def cppstd_compat(conanfile):
    # It will try to find packages with all the cppstd versions
    extension_properties = getattr(conanfile, "extension_properties", {})
    compiler = conanfile.settings.get_safe("compiler")
    compiler_version = conanfile.settings.get_safe("compiler.version")
    cppstd = conanfile.settings.get_safe("compiler.cppstd")
    if not compiler or not compiler_version:
        return []
    factors = []  # List of list, each sublist is a potential combination
    if cppstd is not None and extension_properties.get("compatibility_cppstd") is not False:
        cppstd_possible_values = supported_cppstd(conanfile)
        if cppstd_possible_values is None:
            conanfile.output.warning(f'No cppstd compatibility defined for compiler "{compiler}"')
        else:  # The current cppst must be included in case there is other factor
            factors.append([{"compiler.cppstd": v} for v in cppstd_possible_values])

    cstd = conanfile.settings.get_safe("compiler.cstd")
    if cstd is not None and extension_properties.get("compatibility_cstd") is not False:
        cstd_possible_values = supported_cstd(conanfile)
        if cstd_possible_values is None:
            conanfile.output.warning(f'No cstd compatibility defined for compiler "{compiler}"')
        else:
            factors.append([{"compiler.cstd": v} for v in cstd_possible_values if v != cstd])
    return factors


def compatibility(conanfile):
    # By default, different compiler.cppstd are compatible
    # factors is a list of lists
    factors = cppstd_compat(conanfile)

    # MSVC fallback compatibility
    compiler = conanfile.settings.get_safe("compiler")
    compiler_version = conanfile.settings.get_safe("compiler.version")
    if compiler == "msvc":
        msvc_fallbacks = {
            "195": ["194", "193"],
            "194": ["193"],
        }.get(compiler_version, [])

        if msvc_fallbacks:
            factors.append([{"compiler.version": v} for v in msvc_fallbacks])

    # macOS / apple-clang: accept any compiler.version >= 13 as compatible
    os_ = conanfile.settings.get_safe("os")
    if os_ == "Macos" and compiler == "apple-clang" and compiler_version is not None:
        try:
            current_version = int(str(compiler_version).split(".")[0])
        except ValueError:
            current_version = None
        if current_version is not None:
            min_version = 13
            max_version = 21  # support up to Xcode 27 with apple-clang 21 for now
            candidate_versions = [
                str(v) for v in range(min_version, max_version + 1)
                if v != current_version
            ]
            factors.append([{"compiler.version": v} for v in candidate_versions])

    combinations = _factors_combinations(factors)
    return [{"settings": [(k, v) for k, v in comb.items()]} for comb in combinations]


def _factors_combinations(factors):
    combinations = []
    for factor in factors:
        if not combinations:
            combinations = list(factor)
            continue
        new_combinations = []
        for comb in combinations:
            for f in factor:
                new_comb = comb.copy()
                new_comb.update(f)
                new_combinations.append(new_comb)
        combinations.extend(new_combinations)
    return combinations
