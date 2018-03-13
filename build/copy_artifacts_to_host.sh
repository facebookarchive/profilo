#!/bin/bash

set -e
docker cp $(docker ps --latest -q):out/ .
