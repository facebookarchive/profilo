#!/bin/bash
MUSEUM_VERSION=$1
ART_PATH=$2

if [ -z "${MUSEUM_VERSION}" ] || [ -z "${ART_PATH}" ]; then
  echo "Usage: ./generate_art_diff.sh \$museum_version \$art_path"
  exit 1
fi

read -r -p "Did you already checkout the ART repo to the correct version ($MUSEUM_VERSION)? [y/N]" response
case "$response" in
  [yY]) 
    # Cool.
    ;;
  *)
    exit 1;
    ;;
esac

cd $MUSEUM_VERSION
rm -f art-diff.facebook

for x in $(find .); do
  if [[ -f "$x" ]] && [[ -f "$ART_PATH/$x" ]]; then
    diff -U10 "$ART_PATH/$x" "$x" >> art-diff.facebook
  fi
done

echo "Created diff at $MUSEUM_VERSION/art-diff.facebook"
