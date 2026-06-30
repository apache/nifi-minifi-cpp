<!--
Licensed to the Apache Software Foundation (ASF) under one or more
contributor license agreements.  See the NOTICE file distributed with
this work for additional information regarding copyright ownership.
The ASF licenses this file to You under the Apache License, Version 2.0
(the "License"); you may not use this file except in compliance with
the License.  You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

## Table of Contents

### Processors

- [DecryptContentPGP](#DecryptContentPGP)
- [EncryptContentPGP](#EncryptContentPGP)
### Controller Services

- [PGPPrivateKeyService](#PGPPrivateKeyService)
- [PGPPublicKeyService](#PGPPublicKeyService)


## DecryptContentPGP

### Description

Decrypt contents of OpenPGP messages. Using the Packaged Decryption Strategy preserves OpenPGP encoding to support subsequent signature verification.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                | Default Value | Allowable Values       | Description                                                                                                 |
|---------------------|---------------|------------------------|-------------------------------------------------------------------------------------------------------------|
| Decryption Strategy | DECRYPTED     | DECRYPTED<br/>PACKAGED | Strategy for writing files to success after decryption                                                      |
| Symmetric Password  |               |                        | Password used for decrypting data encrypted with Password-Based Encryption<br/>**Sensitive Property: true** |
| Private Key Service |               |                        | PGP Private Key Service for decrypting data encrypted with Public Key Encryption                            |

### Relationships

| Name    | Description          |
|---------|----------------------|
| success | Decryption Succeeded |
| failure | Decryption Failed    |

### Output Attributes

| Attribute                 | Relationship | Description                                                                                                                                                                                                                                                                                                                                                  |
|---------------------------|--------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| pgp.literal.data.filename | success      | Filename from decrypted Literal Data (Note that OpenPGP signatures do not include the formatting octet, the file name, and the date field of the Literal Data packet in a signature hash; therefore, those fields are not protected against tampering in a signed document. Therefore a lot of implementation omit these inherently malleable metadata)      |
| pgp.literal.data.modified | success      | Modified Date from decrypted Literal Data (Note that OpenPGP signatures do not include the formatting octet, the file name, and the date field of the Literal Data packet in a signature hash; therefore, those fields are not protected against tampering in a signed document. Therefore a lot of implementation omit these inherently malleable metadata) |


## EncryptContentPGP

### Description

Encrypt contents using OpenPGP.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name               | Default Value | Allowable Values | Description                                                                                                                                                                          |
|--------------------|---------------|------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **File Encoding**  | BINARY        | ASCII<br/>BINARY | File Encoding for encryption                                                                                                                                                         |
| Symmetric Password |               |                  | Password used for encrypting data with Password-Based Encryption<br/>**Sensitive Property: true**                                                                                    |
| Public Key Search  |               |                  | PGP Public Key Search will be used to match against the User ID or Key ID when formatted as uppercase hexadecimal string of 16 characters<br/>**Supports Expression Language: true** |
| Public Key Service |               |                  | PGP Public Key Service for encrypting data with Public Key Encryption                                                                                                                |

### Relationships

| Name    | Description          |
|---------|----------------------|
| success | Encryption Succeeded |
| failure | Encryption Failed    |

### Output Attributes

| Attribute         | Relationship | Description   |
|-------------------|--------------|---------------|
| pgp.file.encoding | success      | File Encoding |


## PGPPrivateKeyService

### Description

PGP Private Key Service provides Private Keys loaded from files or properties

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name           | Default Value | Allowable Values | Description                                                                                             |
|----------------|---------------|------------------|---------------------------------------------------------------------------------------------------------|
| Key File       |               |                  | File path to PGP Secret Key encoded in binary or ASCII Armor<br/>**Supports Expression Language: true** |
| Key            |               |                  | Secret Key encoded in ASCII Armor<br/>**Sensitive Property: true**                                      |
| Key Passphrase |               |                  | Passphrase used for decrypting Private Keys<br/>**Sensitive Property: true**                            |


## PGPPublicKeyService

### Description

PGP Public Key Service providing Public Keys loaded from files

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name         | Default Value | Allowable Values | Description                                                                                                        |
|--------------|---------------|------------------|--------------------------------------------------------------------------------------------------------------------|
| Keyring File |               |                  | File path to PGP Keyring or Secret Key encoded in binary or ASCII Armor<br/>**Supports Expression Language: true** |
| Keyring      |               |                  | PGP Keyring or Secret Key encoded in ASCII Armor<br/>**Sensitive Property: true**                                  |
