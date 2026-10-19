#!/usr/bin/env bash
# Write SHA256SUMS for release archives in DIR (default dist): one line per
# archive, then one per .exe/.dll (zip), or per program in bin/ and launcher
# in the top folder (tar.gz).
# Check with: sha256sum -c --ignore-missing SHA256SUMS
set -euo pipefail
DIR=${1:-dist}
cd "$DIR"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
: > "$TMP/sums"
shopt -s nullglob
for a in *.zip *.tar.gz; do
  sha256sum "$a" >> "$TMP/sums"
  mkdir "$TMP/x"
  case "$a" in
    *.zip) unzip -q "$a" -d "$TMP/x"
           (cd "$TMP/x" && find . -type f \( -iname '*.exe' -o -iname '*.dll' \) -printf '%P\0' | LC_ALL=C sort -z | xargs -0 -r sha256sum) >> "$TMP/sums" ;;
    *.tar.gz) tar -xzf "$a" -C "$TMP/x"
           (cd "$TMP/x" && find . -type f -perm -u+x \( -path './*/bin/*' -o -regex '\./[^/]*/[^/]*' \) -printf '%P\0' | LC_ALL=C sort -z | xargs -0 -r sha256sum) >> "$TMP/sums" ;;
  esac
  rm -rf "$TMP/x"
done
mv "$TMP/sums" SHA256SUMS
cat SHA256SUMS
