# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
import logging
import json
from couchbase.cluster import Cluster
from couchbase.options import ClusterOptions
from couchbase.auth import PasswordAuthenticator
from couchbase.transcoder import RawBinaryTranscoder, RawStringTranscoder


class CouchbaseChecker:
    def is_data_present_in_couchbase(self, doc_id: str, bucket_name: str, expected_data: str, expected_data_type: str):
        try:
            cluster = Cluster('couchbase://localhost', ClusterOptions(
                PasswordAuthenticator('Administrator', 'password123')))

            bucket = cluster.bucket(bucket_name)
            collection = bucket.default_collection()

            if expected_data_type.lower() == "binary":
                binary_flag = 0x03 << 24
                result = collection.get(doc_id, transcoder=RawBinaryTranscoder())
                flags = result.flags
                if not flags & binary_flag:
                    logging.error(f"Expected binary data for document '{doc_id}' but no binary flags were found.")
                    return False

                content = result.content_as[bytes]
                return content.decode('utf-8') == expected_data

            if expected_data_type.lower() == "json":
                json_flag = 0x02 << 24
                result = collection.get(doc_id)
                flags = result.flags
                if not flags & json_flag:
                    logging.error(f"Expected JSON data for document '{doc_id}' but no JSON flags were found.")
                    return False

                content = result.content_as[dict]
                return content == json.loads(expected_data)

            if expected_data_type.lower() == "string":
                string_flag = 0x04 << 24
                result = collection.get(doc_id, transcoder=RawStringTranscoder())
                flags = result.flags
                if not flags & string_flag:
                    logging.error(f"Expected string data for document '{doc_id}' but no string flags were found.")
                    return False

                content = result.content_as[str]
                return content == expected_data

            logging.error(f"Unsupported data type '{expected_data_type}'")
            return False
        except Exception as e:
            logging.error(f"Error while fetching document '{doc_id}' from bucket '{bucket_name}': {e}")
            return False
