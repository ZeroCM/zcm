#!/bin/bash

grep -n --color=auto -I -R RRR --exclude-dir=".git" --exclude-dir="build" --exclude "waf" --exclude "review-comments.sh"
