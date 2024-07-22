#!/bin/bash
#

FILE=$1

if [ -f "$FILE" ]; then
  echo "Deploying file $FILE to artifactory"
else
  echo "First argument must be the .deb file to deploy"
  exit 1
fi



USER=`whoami`
read -s -p "Windows Password: " PASSWORD

if [ -n "$PASSWORD" ]; then
  curl -u$USER:$PASSWORD -XPUT "https://artifacts.se.axis.com/artifactory/axis-debian/pool/bws-log-viewer/;deb.distribution=bookworm;deb.distribution=jammy;deb.component=axis;deb.architecture=amd64" -T $FILE
else
  echo "No password, skipping deploy"
fi
