Testing keys v3
------------------------
uid           [ultimate] Alice <alice@example.com>
keyid         BCCE3FDFBA019D7E
passphrase    whiterabbit
RSA, the primary key carries the encrypt capability itself, no subkeys

uid           [ultimate] Bob Personal <bob@home.io>
uid           [ultimate] Bob Primary <bob@work.com>
keyid         A06749BA4F34B0E5
no passphrase

uid           [ultimate] Dave <dave@example.com>
keyid         297B6A88887FB64F
passphrase    gardenparty
Ed25519 sign-only primary key plus a Cv25519 encryption subkey, which is the
layout `gpg --gen-key` produces by default. Encrypting to this key only works
if the encryption subkey is selected rather than the primary key.
  gpg --quick-generate-key 'Dave <dave@example.com>' ed25519 sign never
  gpg --quick-add-key <fingerprint> cv25519 encr never

uid           [ultimate] Erin <erin@example.com>
keyid         8C96441440A22F74
no passphrase
Ed25519 sign-only primary key with no subkeys at all, so it cannot be used for
encryption. Public key only.
  gpg --quick-generate-key 'Erin <erin@example.com>' ed25519 sign never

uid           [ultimate] Bob Personal <bob@home.io.attacker.test>
keyid         756FC2EBECB8747C
no passphrase
A look-alike of Bob whose User ID has Bob's own address as a prefix, so that a
"bob@home.io" search matches both keys. Only used to build
ambiguous_keyring.gpg. Public key only.

Keyrings
------------------------
keyring.{asc,gpg}               public:  Alice + Bob
secret_keyring.{asc,gpg}        secret:  Alice + Bob
ambiguous_keyring.gpg           public:  Alice + Bob + the look-alike Bob, so
                                that a "bob@home.io" User ID search is
                                ambiguous
mixed_secret_keyring.gpg        secret:  Alice + Dave, i.e. two keys whose
                                passphrases differ, which requires more than
                                one Key Password to unlock both

The binary keyrings are concatenations of the individual dearmored keys:
  gpg --dearmor < keyring.asc > keyring.bin
  gpg --dearmor < spoofed_bob.asc > spoofed.bin
  cat keyring.bin spoofed.bin > ambiguous_keyring.gpg

v3 note: v2 shipped two distinct Alice keys sharing the User ID
"Alice <alice@example.com>", which made a "Alice" key search ambiguous. The
stray key (keyid 1BB0EC4BF35325F6) was dropped; the remaining Alice key is the
one the messages in test_messages/ were encrypted to.
