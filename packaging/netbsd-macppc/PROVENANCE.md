# NetBSD 11.1/macppc set digests: provenance

`netbsd-11.1-macppc-sets.sha512` pins four release sets from
`NetBSD-11.1/macppc/binary/sets/`: `base`, `comp`, `xbase`, `xcomp`.

## Where the digests came from

The `SHA512` file NetBSD publishes next to the sets, read on 2026-11-23. `sha512sum` of the sets
downloaded on 2026-11-21 for the G4 acceptance (issue #212) produced the same four values.

## How they were authenticated

NetBSD publishes **no detached signature** for these sets: `SHA512.asc`, `SHA512.sig` and
`MD5.asc` return 404 in the sets directory and the release root. Authentication is therefore
weaker than a signature check, and is stated as such:

- HTTPS to four hostnames operated separately (`cdn.netbsd.org`, `ftp.netbsd.org`,
  `mirror.planetunix.net`, `ftp.nluug.nl`) returned the same four digests on 2026-11-23.
- Every CI run downloads the sets, checks them against the pins in this repository, and
  compares the pins with the published `SHA512` of a second host (`NETBSD_CROSSCHECK_MIRROR`).
  Either mismatch stops the job.
- A digest changing in this file is a reviewed diff; it is never fetched and trusted at run time.

This does not defend against a compromise that serves the same wrong files on every host before the
first pin, nor against an attacker who controls this repository. Re-pin only from a fresh
multi-host comparison, and record the date and hosts here.
