#!/usr/bin/env python3
"""
Makes the HTTPS certificates for the White Noise Machine.

Phones only run the app's service worker on a secure (HTTPS) page, and no
public certificate authority will vouch for a box on a home network. So this
creates a tiny private authority of your own, once, and uses it to sign the
box's certificate.

    python3 -m pip install cryptography
    python3 tools/make_certs.py --ip 192.168.1.50      # the box's reserved IP

First run creates, in certs/ (gitignored -- these are private keys):
    ca.key / ca.crt          your authority, valid 10 years. Install ca.crt on
                             each phone once; keep ca.key private.
Every run (re)issues:
    server.key / server.crt  the box's certificate for whitenoise.local (and
                             any --ip), valid 825 days -- Apple's maximum.
                             Built into the firmware; rebuild and flash after.

Renewing in two years is just re-running this and updating the box. The
phones keep trusting it, because the authority hasn't changed.
"""

import argparse
import datetime
import ipaddress
import os
import pathlib
import sys

try:
    from cryptography import x509
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import ec
    from cryptography.x509.oid import ExtendedKeyUsageOID, NameOID
except ImportError:
    sys.exit("Needs the 'cryptography' package:  python3 -m pip install cryptography")


CERTS_DIR   = pathlib.Path(__file__).resolve().parent.parent / "certs"
CA_DAYS     = 3650   # 10 years
SERVER_DAYS = 825    # Apple rejects server certificates valid for longer


def write_key(path: pathlib.Path, key) -> None:
    path.write_bytes(key.private_bytes(
        serialization.Encoding.PEM,
        serialization.PrivateFormat.PKCS8,
        serialization.NoEncryption(),
    ))
    os.chmod(path, 0o600)


def write_cert(path: pathlib.Path, cert: x509.Certificate) -> None:
    path.write_bytes(cert.public_bytes(serialization.Encoding.PEM))


def make_ca(now: datetime.datetime):
    key  = ec.generate_private_key(ec.SECP256R1())
    name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "White Noise Machine home CA")])

    cert = (
        x509.CertificateBuilder()
        .subject_name(name)
        .issuer_name(name)
        .public_key(key.public_key())
        .serial_number(x509.random_serial_number())
        .not_valid_before(now - datetime.timedelta(hours=1))
        .not_valid_after(now + datetime.timedelta(days=CA_DAYS))
        .add_extension(x509.BasicConstraints(ca=True, path_length=0), critical=True)
        .add_extension(x509.KeyUsage(
            digital_signature=True, key_cert_sign=True, crl_sign=True,
            content_commitment=False, key_encipherment=False, data_encipherment=False,
            key_agreement=False, encipher_only=False, decipher_only=False,
        ), critical=True)
        .add_extension(x509.SubjectKeyIdentifier.from_public_key(key.public_key()), critical=False)
        .sign(key, hashes.SHA256())
    )
    return key, cert


def make_server(ca_key, ca_cert: x509.Certificate, host: str, ips, now: datetime.datetime):
    key = ec.generate_private_key(ec.SECP256R1())

    names = [x509.DNSName(host)] + [x509.IPAddress(ip) for ip in ips]
    start = now - datetime.timedelta(hours=1)

    cert = (
        x509.CertificateBuilder()
        .subject_name(x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, host)]))
        .issuer_name(ca_cert.subject)
        .public_key(key.public_key())
        .serial_number(x509.random_serial_number())
        .not_valid_before(start)
        .not_valid_after(start + datetime.timedelta(days=SERVER_DAYS))
        .add_extension(x509.SubjectAlternativeName(names), critical=False)
        .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)
        .add_extension(x509.KeyUsage(
            digital_signature=True, key_cert_sign=False, crl_sign=False,
            content_commitment=False, key_encipherment=False, data_encipherment=False,
            key_agreement=False, encipher_only=False, decipher_only=False,
        ), critical=True)
        .add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH]), critical=False)
        .add_extension(x509.SubjectKeyIdentifier.from_public_key(key.public_key()), critical=False)
        .add_extension(x509.AuthorityKeyIdentifier.from_issuer_public_key(ca_key.public_key()), critical=False)
        .sign(ca_key, hashes.SHA256())
    )
    return key, cert


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", default="whitenoise.local", help="the box's mDNS name (default: %(default)s)")
    parser.add_argument("--ip", action="append", default=[], type=ipaddress.ip_address,
                        help="the box's reserved IP address; repeat for more than one")
    parser.add_argument("--new-ca", action="store_true",
                        help="replace the authority too (every phone must then install the new ca.crt)")
    args = parser.parse_args()

    CERTS_DIR.mkdir(exist_ok=True)
    now = datetime.datetime.now(datetime.timezone.utc)

    ca_key_path, ca_crt_path = CERTS_DIR / "ca.key", CERTS_DIR / "ca.crt"

    if ca_key_path.exists() and ca_crt_path.exists() and not args.new_ca:
        ca_key  = serialization.load_pem_private_key(ca_key_path.read_bytes(), password=None)
        ca_cert = x509.load_pem_x509_certificate(ca_crt_path.read_bytes())
        print(f"Using the existing authority in {ca_crt_path}")
    else:
        ca_key, ca_cert = make_ca(now)
        write_key(ca_key_path, ca_key)
        write_cert(ca_crt_path, ca_cert)
        print(f"Created a new authority: {ca_crt_path}  <- install this on each phone")

    key, cert = make_server(ca_key, ca_cert, args.host, args.ip, now)
    write_key(CERTS_DIR / "server.key", key)
    write_cert(CERTS_DIR / "server.crt", cert)

    names = ", ".join([args.host] + [str(ip) for ip in args.ip])
    print(f"Issued the box's certificate for {names}")
    print(f"  expires {cert.not_valid_after_utc:%Y-%m-%d} -- re-run this before then")
    if not args.ip:
        print("  tip: add --ip <the box's reserved IP> so opening it by address works too")
    print("Next: rebuild and flash the firmware so it uses the new certificate.")


if __name__ == "__main__":
    main()
