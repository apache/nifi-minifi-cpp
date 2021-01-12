import time
import logging

from M2Crypto import X509, EVP, RSA, ASN1

def gen_cert():
    """
    Generate TLS certificate request for testing
    """

    req, key = gen_req()
    pub_key = req.get_pubkey()
    subject = req.get_subject()
    cert = X509.X509()
    # noinspection PyTypeChecker
    cert.set_serial_number(1)
    cert.set_version(2)
    cert.set_subject(subject)
    t = int(time.time())
    now = ASN1.ASN1_UTCTIME()
    now.set_time(t)
    now_plus_year = ASN1.ASN1_UTCTIME()
    now_plus_year.set_time(t + 60 * 60 * 24 * 365)
    cert.set_not_before(now)
    cert.set_not_after(now_plus_year)
    issuer = X509.X509_Name()
    issuer.C = 'US'
    issuer.CN = 'minifi-listen'
    cert.set_issuer(issuer)
    cert.set_pubkey(pub_key)
    cert.sign(key, 'sha256')

    return cert, key

def rsa_gen_key_callback():
    pass

def gen_req():
    """
    Generate TLS certificate request for testing
    """

    logging.info('Generating test certificate request')
    key = EVP.PKey()
    req = X509.Request()
    rsa = RSA.gen_key(1024, 65537, rsa_gen_key_callback)
    key.assign_rsa(rsa)
    req.set_pubkey(key)
    name = req.get_subject()
    name.C = 'US'
    name.CN = 'minifi-listen'
    req.sign(key, 'sha256')

    return req, key
