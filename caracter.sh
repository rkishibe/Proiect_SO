#!/bin/bash

if [ "$#" -ne 1 ]; then
    exit 1
fi

caracter=$1
contor=0

while IFS= read -r linie || [[ -n "$linie" ]]; do
    if [[ $linie =~ ^[A-Z] && ( $linie =~ [\.!?]$ ) ]]; then
        if [[ $linie =~ ^[A-Za-z0-9\ \,\.\!\?]+$ ]] && ! [[ $linie =~ \,[[:space:]]*și ]]; then
            if [[ $linie == *$caracter* ]]; then
                ((contor++))
            fi
        fi
    fi
done

echo $contor
