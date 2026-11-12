#!/bin/bash
# AzaharFrame crashreport: uploads the unstripped emulator's symbols to Sentry, so the stacks of sent
# crash reports show function names. make-release.sh runs it when SENTRY_AUTH_TOKEN is set; it can be
# run by hand too:
#
#   SENTRY_AUTH_TOKEN=... SENTRY_ORG=<org> SENTRY_PROJECT=<project> steam-frame/release/upload-symbols.sh build/bin/Release/azahar
#
# The token, the organisation and the project come from the environment (or the CLI's own login and
# defaults); none of them is ever written into the repository. The CLI is sentry-cli, or Sentry's newer
# `sentry` CLI (SENTRY_CLI=path picks one). Uploads the ELF file as it is (its symbol table; a build with
# -g adds lines); the release tarball's stripped copy has the same build id, so Sentry matches them.
set -e
FILE=${1:?usage: upload-symbols.sh UNSTRIPPED_AZAHAR}
[ -f "$FILE" ] || { echo "upload-symbols: no file $FILE" >&2; exit 1; }
CLI=${SENTRY_CLI:-}
[ -n "$CLI" ] || CLI=$(command -v sentry-cli || command -v sentry || true)
[ -n "$CLI" ] || { echo "upload-symbols: neither sentry-cli nor sentry is installed (SENTRY_CLI=path)" >&2; exit 1; }
BUILD_ID=$(readelf -n "$FILE" 2> /dev/null | awk '/Build ID/ {print $3; exit}')
echo "upload-symbols: $(basename "$FILE"), build id ${BUILD_ID:-none}, with $(basename "$CLI")"
"$CLI" debug-files upload --type elf --wait "$FILE"
