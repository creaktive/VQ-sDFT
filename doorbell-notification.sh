#!/usr/bin/env bash
PUSHSAFER_KEY=...

cd "$(dirname "${BASH_SOURCE[0]}")"
stdbuf -oL ./doorbell | while IFS= read -r line; do
    timestamp="$(date --rfc-3339=seconds)"
    echo -e "$timestamp\t$line"
    curl -q -s -o /dev/null \
        --form-string "k=$PUSHSAFER_KEY" \
        --form-string "pr=2" \
        --form-string "m=$line" \
        https://www.pushsafer.com/api
done
