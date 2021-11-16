#!/bin/bash
# warning, the classpaths set are relative so this script must be run from zcm/examples
CLASSPATH=$CLASSPATH:build/java/example.jar:build/types/examplezcmtypes.jar:/usr/local/share/java/zcm.jar java -ea example.apps.Pub
